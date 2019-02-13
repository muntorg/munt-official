// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.util.Log
import android.view.View
import android.view.inputmethod.InputMethodManager
import androidx.appcompat.app.AppCompatActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.Constants.OLD_WALLET_PROTOBUF_FILENAME
import kotlinx.android.synthetic.main.upgrade_password.view.*
import org.guldenj.core.Base58
import org.guldenj.crypto.ChildNumber
import org.guldenj.wallet.DeterministicKeyChain
import org.guldenj.wallet.DeterministicKeyChain.*
import org.guldenj.wallet.Wallet
import org.guldenj.wallet.WalletProtobufSerializer
import org.jetbrains.anko.alert
import org.jetbrains.anko.appcompat.v7.Appcompat
import org.jetbrains.anko.design.longSnackbar
import java.io.File
import java.io.FileInputStream
import kotlin.math.max
import kotlin.math.min

private const val TAG = "upgrade-activity"

class UpgradeActivity : AppCompatActivity(), UnityCore.Observer
{
    private val coreObserverProxy = CoreObserverProxy(this, this)
    private var wallet: Wallet? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_upgrade)

        UnityCore.instance.addObserver(coreObserverProxy)

        wallet = walletFromProtobufFile(getFileStreamPath(OLD_WALLET_PROTOBUF_FILENAME))
    }

    override fun onDestroy() {
        super.onDestroy()

        UnityCore.instance.removeObserver(coreObserverProxy)
    }

    override fun onCoreReady(): Boolean {
        gotoActivity(WalletActivity::class.java)
        return true
    }

    fun onUpgrade(view: View)
    {
        wallet?.let { mywallet ->
            if (mywallet.isEncrypted) {
                val myCustomView = layoutInflater.inflate(R.layout.upgrade_password, null)
                val builder = alert(Appcompat) {
                    title = getString(R.string.upgrade_enter_password)
                    customView = myCustomView
                    positiveButton("OK") {
                        try {
                            mywallet.decrypt(myCustomView.password.text)
                            extractAndInitRecovery(mywallet)
                        }
                        catch (exception: Throwable) {
                            view.longSnackbar(getString(R.string.upgrade_wrong_password).format(exception.message!!))
                        }
                    }
                }
                val dialog = builder.build()
                dialog.setOnShowListener {
                    myCustomView.password.requestFocus()
                    val imm = application.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                    imm.showSoftInput(myCustomView.password, InputMethodManager.SHOW_IMPLICIT)
                }
                dialog.show()
            }
            else
                extractAndInitRecovery(mywallet)
        }
    }

    private fun extractAndInitRecovery(wallet: Wallet) {
        assert(!wallet.isEncrypted)
        val recoveryMnemonic = wallet.keyChainSeed.mnemonicCode?.joinToString(" ")
        // first attempt to extract mnemonic seed
        if (recoveryMnemonic != null) {
            // use transaction time and last processed block to determine birth time
            var recoveryTime = wallet.lastBlockSeenTimeSecs
            for (tx in wallet.transactionsByTime) {
                recoveryTime = min(recoveryTime, tx.updateTime.time / 1000)
            }
            recoveryTime = max(0, recoveryTime  - 24 * 60 * 60)

            val recoveryPhrase = GuldenUnifiedBackend.ComposeRecoveryPhrase(recoveryMnemonic, recoveryTime)
            assert(GuldenUnifiedBackend.IsValidRecoveryPhrase(recoveryPhrase))

            Log.i(TAG, "old wallet mnemonic extracted")

            GuldenUnifiedBackend.InitWalletFromRecoveryPhrase(recoveryPhrase)
        }
        else { // mnemonic extraction failed, we should have a linked wallet
            val key = wallet.getKeyByPath(DeterministicKeyChain.DESKTOP_SYNC_ZERO_PATH)
            val timeString = Base58.encode(key.creationTimeSeconds.toString().toByteArray())
            val pvk = Base58.encode(key.privKeyBytes33).substring(1)
            val chaincode= Base58.encode(key.chainCode)
            val linkedUri = "guldensync:%s-%s:%s;unused_payout".format(pvk,chaincode,timeString)
            GuldenUnifiedBackend.InitWalletLinkedFromURI(linkedUri)
        }
    }

    private fun walletFromProtobufFile(walletFile: File): Wallet {
        Log.i(TAG, "start loading old protobuf wallet")
        assert(walletFile.exists()) // require existence has already been established
        val walletStream = FileInputStream(walletFile)
        val wallet = WalletProtobufSerializer().readWallet(walletStream)
        Log.i(TAG, "finished loading protobuf wallet")
        return wallet
    }

    private fun gotoActivity(cls: Class<*> )
    {
            val intent = Intent(this, cls)
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
            startActivity(intent)
            finish()
    }
}
