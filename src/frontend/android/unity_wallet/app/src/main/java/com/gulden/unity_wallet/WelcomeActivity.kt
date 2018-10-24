// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.View
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.vision.barcode.Barcode
import com.gulden.barcodereader.BarcodeCaptureActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.ui.EnterRecoveryPhraseActivity
import com.gulden.unity_wallet.ui.ShowRecoveryPhraseActivity
import kotlin.concurrent.thread

class WelcomeActivity : Activity()
{
    private var context: Context? = null

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_welcome)
        context = this
    }

    fun onCreateNewWallet(view: View)
    {
        thread(true)
        {
            val recoveryPhrase = GuldenUnifiedBackend.GenerateRecoveryMnemonic()
            this.runOnUiThread()
            {
                var newIntent = Intent(this, ShowRecoveryPhraseActivity::class.java)
                //TODO: (GULDEN) Probably not the greatest way to do this - snooping?
                newIntent.putExtra(this.packageName + "recovery_phrase", recoveryPhrase)
                startActivity(newIntent)
            }
        }
    }

    fun onSyncWithDesktop(view: View)
    {
        val intent = Intent(applicationContext, BarcodeCaptureActivity::class.java)
        startActivityForResult(intent, REQUEST_CODE_SCAN)
    }

    /*fun showMainWallet()
    {
        // Save new wallet/settings etc.
        //((WalletApplication) getApplication()).afterLoadWallet(this);

        // Proceed to main activity
        val intent = Intent(this, WalletActivity::class.java)
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        startActivity(intent)

        finish()
    }*/


    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?)
    {
        if (requestCode == REQUEST_CODE_SCAN)
        {
            if (resultCode == CommonStatusCodes.SUCCESS)
            {
                if (data != null)
                {
                    val barcode = data.getParcelableExtra<Barcode>(BarcodeCaptureActivity.BarcodeObject)
                    val parsedQRCodeURI = Uri.parse(barcode.displayValue)

                    //TODO: Implement this
                }
            }
        }
        else
        {
            super.onActivityResult(requestCode, resultCode, data)
        }
    }

    fun onRecoverExistingWallet(view: View)
    {
        startActivity(Intent(this, EnterRecoveryPhraseActivity::class.java))
    }

    companion object
    {
        private val REQUEST_CODE_SCAN = 0
    }
}
