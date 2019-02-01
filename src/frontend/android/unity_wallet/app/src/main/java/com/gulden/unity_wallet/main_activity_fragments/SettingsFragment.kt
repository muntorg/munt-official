// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.main_activity_fragments

import android.content.Context
import android.content.DialogInterface
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


class SettingsFragment : androidx.preference.PreferenceFragmentCompat()
{
    override fun onCreatePreferences(savedInstance: Bundle?, rootKey: String?)
    {
        setPreferencesFromResource(R.layout.fragment_settings, rootKey)
    }

    override fun onPreferenceTreeClick(preference: Preference?): Boolean
    {
        when (preference?.key){
            "recovery_preference" ->
            {
                val phraseView : Preference = findPreference("recovery_view_preference")
                phraseView.title = GuldenUnifiedBackend.GetRecoveryPhrase()
            }
            "preference_link_wallet" ->
            {
                val intent = Intent(context, BarcodeCaptureActivity::class.java)
                startActivityForResult(intent, SettingsFragment.REQUEST_CODE_SCAN_FOR_LINK)
            }
            "preference_change_passcode" ->
            {
                //TODO: Implement
            }
            "preference_rescan_wallet" ->
            {
                GuldenUnifiedBackend.DoRescan()
            }
            "preference_remove_wallet" ->
            {
                GuldenUnifiedBackend.EraseWalletSeedsAndAccounts()
                val intent = Intent(activity, WelcomeActivity::class.java)
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                startActivity(intent)
            }
            "preference_select_local_currency" ->
            {
                //TODO: Implement
            }
            "preference_monitor" ->
            {
                val intent = Intent(context, NetworkMonitorActivity::class.java)
                startActivity(intent)
            }
        }
        return super.onPreferenceTreeClick(preference)
    }


    fun performLink(linkURI: String)
    {
        if (!GuldenUnifiedBackend.ReplaceWalletLinkedFromURI(linkURI))
        {
            AlertDialog.Builder(context!!).setTitle(getString(com.gulden.unity_wallet.R.string.no_guldensync_warning_title)).setMessage(getString(com.gulden.unity_wallet.R.string.no_guldensync_warning)).setPositiveButton(getString(com.gulden.unity_wallet.R.string.button_ok), DialogInterface.OnClickListener { dialogInterface, i -> dialogInterface.dismiss() }).setCancelable(true).create().show()
            return
        }
        (activity as WalletActivity).gotoReceivePage()
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
                        AlertDialog.Builder(context!!).setTitle(getString(com.gulden.unity_wallet.R.string.no_guldensync_warning_title)).setMessage(getString(com.gulden.unity_wallet.R.string.no_guldensync_warning)).setPositiveButton(getString(com.gulden.unity_wallet.R.string.button_ok), DialogInterface.OnClickListener { dialogInterface, i -> dialogInterface.dismiss() }).setCancelable(true).create().show()
                    }

                    if (GuldenUnifiedBackend.HaveUnconfirmedFunds())
                    {
                        AlertDialog.Builder(context!!).setTitle(getString(com.gulden.unity_wallet.R.string.failed_guldensync_warning_title)).setMessage(getString(com.gulden.unity_wallet.R.string.failed_guldensync_unconfirmed_funds_message)).setPositiveButton(getString(com.gulden.unity_wallet.R.string.button_ok), DialogInterface.OnClickListener { dialogInterface, i -> dialogInterface.dismiss() }).setCancelable(true).create().show()
                        return
                    }

                    //TODO: Refuse to link if we are in the process of a sync.

                    if (GuldenUnifiedBackend.GetBalance() > 0)
                    {
                        AlertDialog.Builder(context!!).setTitle(getString(com.gulden.unity_wallet.R.string.guldensync_info_title)).setMessage(getString(com.gulden.unity_wallet.R.string.guldensync_info_message_nonemptywallet)).setPositiveButton(getString(com.gulden.unity_wallet.R.string.button_ok) , DialogInterface.OnClickListener { dialogInterface, i -> dialogInterface.dismiss(); performLink(barcode.displayValue) }).setNegativeButton(getString(com.gulden.unity_wallet.R.string.button_cancel), DialogInterface.OnClickListener { dialogInterface, i -> dialogInterface.dismiss() }).setCancelable(true).create().show()
                    }
                    else
                    {
                        AlertDialog.Builder(context!!).setTitle(getString(com.gulden.unity_wallet.R.string.guldensync_info_title)).setMessage(getString(com.gulden.unity_wallet.R.string.guldensync_info_message_emptywallet)).setPositiveButton(getString(com.gulden.unity_wallet.R.string.button_ok) , DialogInterface.OnClickListener { dialogInterface, i -> dialogInterface.dismiss(); performLink(barcode.displayValue)} ).setNegativeButton(getString(com.gulden.unity_wallet.R.string.button_cancel), DialogInterface.OnClickListener { dialogInterface, i -> dialogInterface.dismiss() }).setCancelable(true).create().show()
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
            throw RuntimeException(context.toString() + " must implement OnFragmentInteractionListener")
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
        private val REQUEST_CODE_SCAN_FOR_LINK = 0
    }
}
