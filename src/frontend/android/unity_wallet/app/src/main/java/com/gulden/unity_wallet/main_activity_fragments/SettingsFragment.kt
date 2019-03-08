// Copyright (c) 2018-2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.main_activity_fragments

import android.os.Bundle
import androidx.preference.Preference
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.WalletActivity
import com.gulden.unity_wallet.localCurrency


class SettingsFragment : androidx.preference.PreferenceFragmentCompat()
{
    override fun onCreatePreferences(savedInstance: Bundle?, rootKey: String?)
    {
        setPreferencesFromResource(R.xml.fragment_settings, rootKey)
    }

    override fun onResume()
    {
        super.onResume()

        val localCurrencyPreference : Preference = findPreference("preference_local_currency")
        localCurrencyPreference.summary = localCurrency.code
    }

    override fun onPreferenceTreeClick(preference: Preference?): Boolean
    {
        when (preference?.key){
            "preference_local_currency" ->  (activity as WalletActivity).gotoCurrencyPage()
            "preference_show_wallet_settings" ->  (activity as WalletActivity).gotoWalletSettingsPage()
        }
        return super.onPreferenceTreeClick(preference)
    }
}
