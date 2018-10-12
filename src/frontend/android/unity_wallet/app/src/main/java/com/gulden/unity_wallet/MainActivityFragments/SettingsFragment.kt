// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.MainActivityFragments

import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.support.v7.preference.Preference
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.R


class SettingsFragment : android.support.v7.preference.PreferenceFragmentCompat()
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
                val phraseView = findPreference("recovery_view_preference")
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
