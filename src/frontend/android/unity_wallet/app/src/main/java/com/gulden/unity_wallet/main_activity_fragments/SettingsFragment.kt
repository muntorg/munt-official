// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.main_activity_fragments

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.appcompat.app.AlertDialog
import androidx.preference.Preference
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.vision.barcode.Barcode
import com.gulden.barcodereader.BarcodeCaptureActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.WalletActivity
import com.gulden.unity_wallet.WelcomeActivity
import com.gulden.unity_wallet.ui.monitor.NetworkMonitorActivity
import org.jetbrains.anko.contentView
import org.jetbrains.anko.design.snackbar
import org.jetbrains.anko.support.v4.alert
import kotlin.concurrent.thread


class SettingsFragment : androidx.preference.PreferenceFragmentCompat()
{
    override fun onCreatePreferences(savedInstance: Bundle?, rootKey: String?)
    {
        setPreferencesFromResource(R.xml.fragment_settings, rootKey)
        if (GuldenUnifiedBackend.IsMnemonicWallet()) {
            preferenceScreen.removePreferenceRecursively("recovery_linked_preference")
            preferenceScreen.removePreferenceRecursively("preference_unlink_wallet")
        }
        else {
            preferenceScreen.removePreferenceRecursively("recovery_view_preference")
            preferenceScreen.removePreferenceRecursively("preference_remove_wallet")
        }
    }

    override fun onPreferenceTreeClick(preference: Preference?): Boolean
    {
        when (preference?.key){
            "preference_link_wallet" ->
            {
                val intent = Intent(context, BarcodeCaptureActivity::class.java)
                startActivityForResult(intent, SettingsFragment.REQUEST_CODE_SCAN_FOR_LINK)
            }
            "preference_change_pass_code" ->
            {
                //TODO: Implement
            }
            "preference_rescan_wallet" ->
            {
                alert(getString(R.string.rescan_confirm_msg), getString(R.string.rescan_confirm_title)) {

                    // on confirmation compose recipient and execute payment
                    positiveButton(getString(R.string.rescan_confirm_btn)) {
                        GuldenUnifiedBackend.DoRescan()
                        activity?.contentView?.snackbar(getString(R.string.rescan_started))
                    }
                    negativeButton(getString(R.string.cancel_btn)) {}
                }.show()
            }
            "preference_remove_wallet", "preference_unlink_wallet" ->
            {
                GuldenUnifiedBackend.EraseWalletSeedsAndAccounts()
                val intent = Intent(activity, WelcomeActivity::class.java)
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                startActivity(intent)
            }
            "preference_local_currency" ->
            {
                (activity as WalletActivity).showLocalCurrenciesPage()
            }
            "preference_monitor" ->
            {
                val intent = Intent(context, NetworkMonitorActivity::class.java)
                startActivity(intent)
            }
        }
        return super.onPreferenceTreeClick(preference)
    }


    private fun performLink(linkURI: String)
    {
        // ReplaceWalletLinkedFromURI can be long running, so run it in a thread that isn't the UI thread.
        thread(start = true)
        {
            if (!GuldenUnifiedBackend.ReplaceWalletLinkedFromURI(linkURI))
            {
                activity?.runOnUiThread(java.lang.Runnable {
                    AlertDialog.Builder(context!!).setTitle(getString(com.gulden.unity_wallet.R.string.no_guldensync_warning_title)).setMessage(getString(com.gulden.unity_wallet.R.string.no_guldensync_warning)).setPositiveButton(getString(com.gulden.unity_wallet.R.string.button_ok)) { dialogInterface, i -> dialogInterface.dismiss() }.setCancelable(true).create().show()
                })
            }
            else
            {
                activity?.runOnUiThread(java.lang.Runnable {
                    activity?.contentView?.snackbar(getString(R.string.rescan_started))
                    (activity as WalletActivity).gotoReceivePage()
                })
            }
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?)
    {
        if (requestCode == SettingsFragment.REQUEST_CODE_SCAN_FOR_LINK)
        {
            if (resultCode == CommonStatusCodes.SUCCESS)
            {
                if (data != null)
                {
                    val barcode = data.getParcelableExtra<Barcode>(BarcodeCaptureActivity.BarcodeObject)

                    if (!GuldenUnifiedBackend.IsValidLinkURI(barcode.displayValue))
                    {
                        AlertDialog.Builder(context!!).setTitle(getString(com.gulden.unity_wallet.R.string.no_guldensync_warning_title)).setMessage(getString(com.gulden.unity_wallet.R.string.no_guldensync_warning)).setPositiveButton(getString(com.gulden.unity_wallet.R.string.button_ok)) { dialogInterface, i -> dialogInterface.dismiss() }.setCancelable(true).create().show()
                    }

                    if (GuldenUnifiedBackend.HaveUnconfirmedFunds())
                    {
                        AlertDialog.Builder(context!!).setTitle(getString(com.gulden.unity_wallet.R.string.failed_guldensync_warning_title)).setMessage(getString(com.gulden.unity_wallet.R.string.failed_guldensync_unconfirmed_funds_message)).setPositiveButton(getString(com.gulden.unity_wallet.R.string.button_ok)) { dialogInterface, i -> dialogInterface.dismiss() }.setCancelable(true).create().show()
                        return
                    }

                    //TODO: Refuse to link if we are in the process of a sync.

                    if (GuldenUnifiedBackend.GetBalance() > 0)
                    {
                        AlertDialog.Builder(context!!).setTitle(getString(com.gulden.unity_wallet.R.string.guldensync_info_title)).setMessage(getString(com.gulden.unity_wallet.R.string.guldensync_info_message_non_empty_wallet)).setPositiveButton(getString(com.gulden.unity_wallet.R.string.button_ok)) { dialogInterface, i -> dialogInterface.dismiss(); performLink(barcode.displayValue) }.setNegativeButton(getString(com.gulden.unity_wallet.R.string.button_cancel)) { dialogInterface, i -> dialogInterface.dismiss() }.setCancelable(true).create().show()
                    }
                    else
                    {
                        AlertDialog.Builder(context!!).setTitle(getString(com.gulden.unity_wallet.R.string.guldensync_info_title)).setMessage(getString(com.gulden.unity_wallet.R.string.guldensync_info_message_empty_wallet)).setPositiveButton(getString(com.gulden.unity_wallet.R.string.button_ok)) { dialogInterface, i -> dialogInterface.dismiss(); performLink(barcode.displayValue)}.setNegativeButton(getString(com.gulden.unity_wallet.R.string.button_cancel)) { dialogInterface, i -> dialogInterface.dismiss() }.setCancelable(true).create().show()
                    }
                }
            }
        }
        else
        {
            super.onActivityResult(requestCode, resultCode, data)
        }
    }


    override fun onAttach(context: Context)
    {
        super.onAttach(context)
        if (context is OnFragmentInteractionListener)
        {
            listener = context
        }
        else
        {
            throw RuntimeException("$context must implement OnFragmentInteractionListener")
        }
    }

    override fun onDetach()
    {
        super.onDetach()
        listener = null
    }

    private var listener: OnFragmentInteractionListener? = null
    interface OnFragmentInteractionListener
    {
        fun onFragmentInteraction(uri: Uri)
    }

    companion object
    {
        private const val REQUEST_CODE_SCAN_FOR_LINK = 0
    }
}
