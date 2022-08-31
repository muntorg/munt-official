// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package unity_wallet

import android.content.Intent
import android.net.Uri
import android.widget.Toast
import unity_wallet.util.AppBaseActivity

// URI handlers need unity core active to function - however as they may be called when app is closed it is necessary to manually start the core in some cases
// All URI handlers therefore come via this transparent activity
// Which takes care of detecting whether core is already active or not and then handles the URI appropriately

class URIHandlerActivity : AppBaseActivity()
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
                val recipient = createRecipient(intentUri.toString())
                SendCoinsFragment.newInstance(recipient, true).show(supportFragmentManager, SendCoinsFragment::class.java.simpleName)
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

    private fun isValidCoinUri(uri: Uri?): Boolean {
        uri?.run {
            scheme?.run {
                lowercase().run {
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
        if (Intent.ACTION_VIEW == intent.action && isValidCoinUri(intentUri)) {
            handleURIAndClose()
        } else {
            finish()
        }
    }

}
