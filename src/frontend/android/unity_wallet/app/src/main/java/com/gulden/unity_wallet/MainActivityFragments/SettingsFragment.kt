// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.MainActivityFragments

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.preference.Preference
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.SendCoinsActivity
import com.gulden.unity_wallet.WalletActivity
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
                //TODO: Implement
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
                //TODO: Implement
            }
            "preference_select_local_currency" ->
            {
                //TODO: Implement
            }
            "preference_notify_transaction_activity" ->
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
}
