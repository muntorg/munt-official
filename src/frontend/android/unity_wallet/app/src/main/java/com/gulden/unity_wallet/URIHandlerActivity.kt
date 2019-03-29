// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.UriRecipient
import com.gulden.jniunifiedbackend.UriRecord
import org.apache.commons.validator.routines.IBANValidator

// URI handlers need unity core active to function - however as they may be called when app is closed it is necessary to manually start the core in some cases
// All URI handlers therefore come via this transparent activity
// Which takes care of detecting whether core is already active or not and then handles the URI appropriately

//TODO - dedup some of the common code this shares with IntroActivity
class URIHandlerActivity : AppCompatActivity(), UnityCore.Observer
{
    private fun toastAndExit()
    {
        handleURI = false
        Toast.makeText(this, getString(R.string.toast_warn_uri_attempt_before_wallet_creation), Toast.LENGTH_SHORT).show()
        finish()
    }

    private var handleURI = true
    override fun createNewWallet(): Boolean
    {

        toastAndExit()
        return true
    }
    override fun haveExistingWallet(): Boolean {
        return true
    }

    override fun onCoreReady(): Boolean {
        if (UnityCore.receivedExistingWalletEvent && handleURI)
        {
            handleURI = false
            handleURIAndClose()
        }
        else
        {
            toastAndExit()
        }
        return true
    }

    private fun handleURIAndClose()
    {
        if ((intentUri != null) && (scheme != null))
        {
            //TODO: Improve this, and consider moving more of the work into unity core
            val address = if (intentUri?.host!=null) intentUri?.host else ""
            val amount = if (intentUri?.queryParameterNames?.contains("amount")!!) intentUri?.getQueryParameter("amount") else "0"
            val label = if (intentUri?.queryParameterNames?.contains("label")!!) intentUri?.getQueryParameter("label") else ""
            var recipient : UriRecipient? = null
            if (IBANValidator.getInstance().isValid(address))
            {
                recipient = UriRecipient(false, address, label, amount)
            }
            else if (GuldenUnifiedBackend.IsValidRecipient(UriRecord("gulden", address, HashMap<String,String>())).valid)
            {
                recipient = GuldenUnifiedBackend.IsValidRecipient(UriRecord("gulden", address, HashMap<String, String>()))
                recipient = UriRecipient(recipient.valid, recipient.address, if (recipient.label!="") recipient.label else label, amount)
            }
            else if (uriRecipient(address!!).valid)
            {
                recipient = UriRecipient(true, address, label, amount)
            }

            if (recipient != null)
            {
                SendCoinsFragment.newInstance(recipient, true).show(supportFragmentManager, SendCoinsFragment::class.java.simpleName)
            }
        }
    }

    private var intentUri : Uri? = null
    private var scheme : String? = null
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        intentUri = intent.data
        scheme = intentUri?.getScheme()
        if ((Intent.ACTION_VIEW == intent.action)
                && (intentUri != null)
                && (scheme != null)
                && (scheme?.toLowerCase()?.startsWith("gulden")!!
                        || scheme?.toLowerCase()?.startsWith("guldencoin")!!
                        || scheme?.toLowerCase()?.startsWith("iban")!!
                        || scheme?.toLowerCase()?.startsWith("sepa")!!)
        )
        {
            val core = UnityCore.instance

            UnityCore.instance.addObserver(this, fun (callback:() -> Unit) { runOnUiThread { callback() }})
            core.startCore()

            // if core is already ready we are resuming a session and can go directly to the URI handler
            // (there will be no further coreReady or haveExistingWallet event)
            // Otherwise we must wait for the event
            if (core.isCoreReady())
            {
                handleURIAndClose()
            }
            // If we have already previously received a create new wallet event and not handled it
            // Then we are inside some edge case (e.g. someone has opened the app and is sitting on the create new wallet screen - and has now clicked a URI as well...)
            // So just toast and exit
            else if(!UnityCore.receivedExistingWalletEvent && UnityCore.receivedCreateNewWalletEvent)
            {
                toastAndExit()
            }
        }
        else
        {
            finish()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        UnityCore.instance.removeObserver(this)
    }
}
