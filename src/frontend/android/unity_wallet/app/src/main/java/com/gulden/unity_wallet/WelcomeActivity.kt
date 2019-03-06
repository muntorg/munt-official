// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.vision.barcode.Barcode
import com.gulden.barcodereader.BarcodeCaptureActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.ui.EnterRecoveryPhraseActivity
import com.gulden.unity_wallet.ui.ShowRecoveryPhraseActivity
import com.gulden.unity_wallet.util.gotoWalletActivity
import org.jetbrains.anko.alert
import org.jetbrains.anko.appcompat.v7.Appcompat
import kotlin.concurrent.thread

class WelcomeActivity : AppCompatActivity(), UnityCore.Observer
{
    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_welcome)

        UnityCore.instance.addObserver(this, fun (callback:() -> Unit) { runOnUiThread { callback() }})
    }

    override fun onDestroy()
    {
        super.onDestroy()

        UnityCore.instance.removeObserver(this)
    }

    override fun onBackPressed() {
        super.onBackPressed()

        // finish and kill myself so a next session is properly started
        finish()
        System.exit(0)
    }

    @Suppress("UNUSED_PARAMETER")
    fun onCreateNewWallet(view: View)
    {
        thread(true)
        {
            val recoveryPhrase = GuldenUnifiedBackend.GenerateRecoveryMnemonic()
            this.runOnUiThread()
            {
                val newIntent = Intent(this, ShowRecoveryPhraseActivity::class.java)
                //TODO: (GULDEN) Probably not the greatest way to do this - snooping?
                newIntent.putExtra(this.packageName + "recovery_phrase", recoveryPhrase)
                startActivity(newIntent)
            }
        }
    }

    @Suppress("UNUSED_PARAMETER")
    fun onSyncWithDesktop(view: View)
    {
        val intent = Intent(applicationContext, BarcodeCaptureActivity::class.java)
        intent.putExtra(BarcodeCaptureActivity.AutoFocus, true)
        startActivityForResult(intent, REQUEST_CODE_SCAN_FOR_LINK)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?)
    {
        if (requestCode == REQUEST_CODE_SCAN_FOR_LINK)
        {
            if (resultCode == CommonStatusCodes.SUCCESS && data != null)
            {
                val barcode = data.getParcelableExtra<Barcode>(BarcodeCaptureActivity.BarcodeObject)
                Authentication.instance.chooseAccessCode(this) {
                    password->
                    if (UnityCore.instance.isCoreReady())
                    {
                        if (GuldenUnifiedBackend.ContinueWalletLinkedFromURI(barcode.displayValue, password.joinToString("")))
                        {
                            gotoWalletActivity(this)
                            return@chooseAccessCode
                        }
                    }
                    else
                    {
                        // Create the new wallet, a coreReady event will follow which will proceed to the main activity
                        if (GuldenUnifiedBackend.InitWalletLinkedFromURI(barcode.displayValue, password.joinToString("")))
                        {
                            return@chooseAccessCode
                        }
                    }

                    // Got here so there was an error in init or continue linked wallet
                    alert(Appcompat,  getString(com.gulden.unity_wallet.R.string.no_guldensync_warning),  getString(com.gulden.unity_wallet.R.string.no_guldensync_warning_title))
                    {
                        positiveButton(getString(com.gulden.unity_wallet.R.string.button_ok))
                        {
                            it.dismiss()
                        }
                        isCancelable = true
                    }.build().show()
                }
            }
        }
        else
        {
            super.onActivityResult(requestCode, resultCode, data)
        }
    }

    @Suppress("UNUSED_PARAMETER")
    fun onRecoverExistingWallet(view: View)
    {
        startActivity(Intent(this, EnterRecoveryPhraseActivity::class.java))
    }

    override fun onCoreReady(): Boolean {
        gotoWalletActivity(this)
        return true
    }

    companion object
    {
        private const val REQUEST_CODE_SCAN_FOR_LINK = 0
    }
}
