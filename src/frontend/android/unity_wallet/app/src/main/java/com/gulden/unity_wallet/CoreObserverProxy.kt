package com.gulden.unity_wallet

import android.app.Activity
import com.gulden.jniunifiedbackend.MutationRecord
import com.gulden.jniunifiedbackend.TransactionRecord

/** Helper for running core events on the UI thread of an activity */
class CoreObserverProxy(private var activity: Activity,
                        private var observer: UnityCore.Observer) : UnityCore.Observer {

    override fun syncProgressChanged(percent: Float): Boolean {
        activity.runOnUiThread {
            observer.syncProgressChanged(percent)
        }
        return false
    }

    override fun walletBalanceChanged(balance: Long): Boolean {
        activity.runOnUiThread {
            observer.walletBalanceChanged(balance)
        }
        return false
    }

    override fun onCoreReady(): Boolean {
        activity.runOnUiThread {
            observer.onCoreReady()
        }
        return false
    }

    override fun onCoreShutdown(): Boolean {
        activity.runOnUiThread {
            observer.onCoreShutdown()
        }
        return false
    }

    override fun createNewWallet(): Boolean {
        activity.runOnUiThread {
            observer.createNewWallet()
        }
        return false
    }

    override fun haveExistingWallet(): Boolean {
        activity.runOnUiThread {
            observer.haveExistingWallet()
        }
        return false
    }

    override fun onNewMutation(mutation: MutationRecord) {
        activity.runOnUiThread {
            observer.onNewMutation(mutation)
        }
    }
}
