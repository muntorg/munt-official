// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.Toast
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.UriRecipient
import com.gulden.jniunifiedbackend.UriRecord
import com.gulden.unity_wallet.util.AppBaseActivity
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.apache.commons.validator.routines.IBANValidator

// URI handlers need unity core active to function - however as they may be called when app is closed it is necessary to manually start the core in some cases
// All URI handlers therefore come via this transparent activity
// Which takes care of detecting whether core is already active or not and then handles the URI appropriately

class URIHandlerActivity : AppBaseActivity(), UnityCore.Observer
{
    private fun toastAndExit()
    {
        Toast.makeText(this, getString(R.string.toast_warn_uri_attempt_before_wallet_creation), Toast.LENGTH_SHORT).show()
        finish()
    }

    override fun onWalletCreate() {
        toastAndExit()
    }

    private fun handleURIAndClose()
    {
        if ((intentUri != null) && (scheme != null))
        {
            try
            {
                //TODO: Improve this, and consider moving more of the work into unity core
                var address = ""
                var amount = 0L
                var label = ""

                //If Uri has been parsed as hierarchial force it to reparse as non-hierarchial so that we can access any query portions correctly.
                if (!intentUri!!.isHierarchical)
                {
                    intentUri = Uri.parse(intentUri.toString().replaceFirst(":", "://"))
                }
                if (intentUri?.host != null) address = intentUri!!.host
                if (intentUri?.queryParameterNames?.contains("amount")!!) amount = intentUri!!.getQueryParameter("amount").toDoubleOrZero().toNative()
                if (intentUri?.queryParameterNames?.contains("label")!!) label = intentUri!!.getQueryParameter("label") else ""

                var recipient: UriRecipient? = null
                if (IBANValidator.getInstance().isValid(address))
                {
                    recipient = UriRecipient(false, address, label, amount)
                }
                else if (GuldenUnifiedBackend.IsValidRecipient(UriRecord("gulden", address, HashMap<String, String>())).valid)
                {
                    recipient = GuldenUnifiedBackend.IsValidRecipient(UriRecord("gulden", address, HashMap<String, String>()))
                    recipient = UriRecipient(recipient.valid, recipient.address, if (recipient.label != "") recipient.label else label, amount)
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
            catch (e : Exception)
            {
                //TODO: Improve error handling here
                finish()
            }
        }
    }

    private var intentUri : Uri? = null
    private var scheme : String? = null

    private fun isValidGuldenUri(uri: Uri?): Boolean {
        uri?.run {
            scheme?.run {
                toLowerCase().run {
                    return startsWith("gulden")
                            || startsWith("guldencoin")
                            || startsWith("iban")
                            || startsWith("sepa")
                }
            }
        }
        return false
    }

    override fun onWalletReady() {
        intentUri = intent.data
        scheme = intentUri?.scheme
        if (Intent.ACTION_VIEW == intent.action && isValidGuldenUri(intentUri)) {
            UnityCore.instance.addObserver(this@URIHandlerActivity, fun(callback: () -> Unit) { runOnUiThread { callback() } })
            handleURIAndClose()
        } else {
            finish()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        UnityCore.instance.removeObserver(this)
    }
}
