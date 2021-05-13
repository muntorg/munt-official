// Copyright (c) 2018 The Florin developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za), Willem de Jonge (willem@isnapp.nl)
// Distributed under the Florin software license, see the accompanying
// file COPYING

package com.florin.unity_wallet

import android.content.Intent
import android.os.Bundle
import android.view.View
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.vision.barcode.Barcode
import com.florin.barcodereader.BarcodeCaptureActivity
import com.florin.jniunifiedbackend.ILibraryController
import com.florin.unity_wallet.ui.EnterRecoveryPhraseActivity
import com.florin.unity_wallet.ui.ShowRecoveryPhraseActivity
import com.florin.unity_wallet.util.AppBaseActivity
import com.florin.unity_wallet.util.gotoWalletActivity
import org.jetbrains.anko.alert
import org.jetbrains.anko.appcompat.v7.Appcompat
import kotlin.concurrent.thread

class WelcomeActivity : AppBaseActivity(), UnityCore.Observer
{
    private val erasedWallet = UnityCore.instance.isCoreReady()

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
        // note this is now only because of the weird wiring which happens when a wallet is erased
        // consider introducing query API in Unity that would allow getting the erased wallet state
        // or some other construct such that logic on the Unity client side can be clearer
        finish()
        System.exit(0)
    }

    @Suppress("UNUSED_PARAMETER")
    fun onCreateNewWallet(view: View)
    {
        thread(true)
        {
            val recoveryPhrase = ILibraryController.GenerateRecoveryMnemonic()
            this.runOnUiThread()
            {
                val newIntent = Intent(this, ShowRecoveryPhraseActivity::class.java)
                //TODO: (FLORIN) Probably not the greatest way to do this - snooping?
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
        // TODO must have core started and createWallet signal
        if (requestCode == REQUEST_CODE_SCAN_FOR_LINK)
        {
            if (resultCode == CommonStatusCodes.SUCCESS && data != null)
            {
                val barcode = data.getParcelableExtra<Barcode>(BarcodeCaptureActivity.BarcodeObject)
                Authentication.instance.chooseAccessCode(this, null) {
                    password->
                    if (UnityCore.instance.isCoreReady())
                    {
                        if (ILibraryController.ContinueWalletLinkedFromURI(barcode.displayValue, password.joinToString("")))
                        {
                            gotoWalletActivity(this)
                            return@chooseAccessCode
                        }
                    }
                    else
                    {
                        // Create the new wallet, a coreReady event will follow which will proceed to the main activity
                        if (ILibraryController.InitWalletLinkedFromURI(barcode.displayValue, password.joinToString("")))
                        {
                            return@chooseAccessCode
                        }
                    }

                    // Got here so there was an error in init or continue linked wallet
                    alert(Appcompat,  getString(R.string.no_qrsync_warning),  getString(R.string.no_qrsync_warning_title))
                    {
                        positiveButton(getString(R.string.button_ok))
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

    override fun onWalletReady() {
        if (!erasedWallet)
            gotoWalletActivity(this)
    }

    override fun onWalletCreate() {
        // do nothing, we are supposed to sit here until the wallet was created
    }

    companion object
    {
        private const val REQUEST_CODE_SCAN_FOR_LINK = 0
    }
}
