// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.util.Log
import com.gulden.jniunifiedbackend.*
import kotlinx.coroutines.CompletableDeferred
import java.util.concurrent.locks.Lock
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.thread
import kotlin.concurrent.withLock

private const val TAG = "unity_core"

data class UnityConfig(val dataDir: String, val apkPath: String, val staticFilterOffset: Long, val staticFilterLength: Long, val testnet: Boolean)

class UnityCore {
    val walletReady
        get() = intWalletReady
    val walletCreate
        get() = intWalletCreate

    private var intWalletReady = CompletableDeferred<Unit>()
    private var intWalletCreate = CompletableDeferred<Unit>()

    interface Observer {
        fun syncProgressChanged(percent: Float) {}
        fun walletBalanceChanged(balance: Long) {}
        fun onCoreShutdown() {}
        fun onNewMutation(mutation: MutationRecord, selfCommitted: Boolean) {}
        fun updatedTransaction(transaction: TransactionRecord) {}
        fun onAddressBookChanged() {}
    }

    companion object {
        val instance: UnityCore = UnityCore()
        // Have we previously been started
        var started = false
    }

    @Synchronized
    fun configure(_config: UnityConfig) {
        if (config != null)
            throw RuntimeException("Can only configure once")
        config = _config
    }

    fun addObserver(observer: Observer, wrapper: (() -> Unit)-> Unit = fun (body: () -> Unit) { body() }) {
        observersLock.withLock {
            observers.add(ObserverEntry(observer, wrapper))
        }
    }

    fun removeObserver(observer: Observer) {
        observersLock.withLock {
            observers.removeAll(
                    observers.filter { it.observer == observer }
            )
        }
    }

    @Synchronized
    fun startCore() {
        if (!started) {
            if (config == null)
                throw RuntimeException("Configure before starting Unity core")

            val cfg: UnityConfig = config as UnityConfig

            thread(true)
            {
                System.loadLibrary("gulden_unity_jni")
                GuldenUnifiedBackend.InitUnityLib(cfg.dataDir, cfg.apkPath, cfg.staticFilterOffset, cfg.staticFilterLength, cfg.testnet, coreLibrarySignalHandler, "")
            }

            started = true
        }
    }

    fun isCoreReady(): Boolean {
        val deferred = walletReady
        return deferred.isCompleted && !deferred.isCancelled
    }

    private var config: UnityConfig? = null

    class ObserverEntry(val observer: Observer, val wrapper: (() ->Unit) -> Unit)
    private var observersLock: Lock = ReentrantLock()
    private var observers: MutableSet<ObserverEntry> = mutableSetOf()

    private var stateTrackLock:  Lock = ReentrantLock()

    val progressPercent: Float
        get() {
            return GuldenUnifiedBackend.getUnifiedProgress() * 100
        }

    var balanceAmount: Long = 0
        set(value) {
            stateTrackLock.withLock {
                field = value
            }
        }
        get() {
            stateTrackLock.withLock {
                return field
            }
        }

    // TODO wrappers here could be moved into core later
    fun addAddressBookRecord(record: AddressRecord) {
        GuldenUnifiedBackend.addAddressBookRecord(record)
        coreLibrarySignalHandler.notifyAddressBookChanged()
    }

    fun deleteAddressBookRecord(record: AddressRecord) {
        GuldenUnifiedBackend.deleteAddressBookRecord(record)
        coreLibrarySignalHandler.notifyAddressBookChanged()
    }

    // Handle signals from core library, convert and broadcast to all registered observers
    private val coreLibrarySignalHandler = object : GuldenUnifiedFrontend() {
        override fun logPrint(str: String?) {
            // logging already done at C++ level
        }

        override fun notifyUnifiedProgress(progress: Float) {
            val percent: Float = 100 * progress

            observersLock.withLock {
                observers.forEach {
                    it.wrapper { it.observer.syncProgressChanged(percent) }
                }
            }
        }

        override fun notifyBalanceChange(newBalance: BalanceRecord) {
            balanceAmount = newBalance.availableIncludingLocked + newBalance.immatureIncludingLocked + newBalance.unconfirmedIncludingLocked
            observersLock.withLock {
                observers.forEach {
                    it.wrapper {
                        it.observer.walletBalanceChanged(balanceAmount)
                    }
                }
            }
        }

        override fun notifyNewMutation(mutation: MutationRecord, selfCommitted: Boolean) {
            observersLock.withLock {
                observers.forEach {
                    it.wrapper { it.observer.onNewMutation(mutation, selfCommitted) }
                }
            }
        }

        override fun notifyUpdatedTransaction(transaction: TransactionRecord) {
            observersLock.withLock {
                observers.forEach {
                    it.wrapper { it.observer.updatedTransaction(transaction) }
                }
            }
        }

        override fun notifyShutdown() {
            observersLock.withLock {
                observers.forEach {
                    it.wrapper { it.observer.onCoreShutdown() }
                }
            }
        }

        override fun notifyCoreReady() {
            try {
                // We have a functioning wallet, there will never be a notifyInitWithoutExistingWallet event,
                // so cancel the current walletCreate. Though it might have completed already, so create a new
                // one and cancel that as well.
                intWalletCreate.cancel()
                intWalletCreate = CompletableDeferred<Unit>()
                intWalletCreate.cancel()
                walletReady.complete(Unit)
            }
            catch (e: Throwable) {
                Log.e(TAG, "Exception in Java/Kotlin notification handler notifyCoreReady() $e")
            }
        }

        override fun notifyInitWithExistingWallet() {
            // not used
        }

        override fun notifyInitWithoutExistingWallet() {
            try {
                // There is no wallet yet. Cancel anything that is currently waiting for the wallet to get ready
                // and put a new deferred there that can be waited on after the wallet create is properly handled.
                intWalletReady.cancel()
                intWalletReady = CompletableDeferred<Unit>()
                walletCreate.complete(Unit)
            }
            catch (e: Throwable) {
                Log.e(TAG, "Exception in Java/Kotlin notification handler notifyInitWithoutExistingWallet() $e")
            }
        }

        /*override*/ fun notifyAddressBookChanged() {
            observersLock.withLock {
                observers.forEach {
                    it.wrapper { it.observer.onAddressBookChanged() }
                }
            }
        }
    }

}
