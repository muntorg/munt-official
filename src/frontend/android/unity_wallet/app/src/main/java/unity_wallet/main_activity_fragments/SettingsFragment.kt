// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

package unity_wallet.main_activity_fragments

import android.os.Bundle
import androidx.preference.Preference
import androidx.preference.SwitchPreference
import unity_wallet.Authentication
import unity_wallet.R
import unity_wallet.WalletActivity
import unity_wallet.localCurrency


class SettingsFragment : androidx.preference.PreferenceFragmentCompat()
{
    override fun onCreatePreferences(savedInstance: Bundle?, rootKey: String?)
    {
        setPreferencesFromResource(R.xml.fragment_settings, rootKey)

        val pref = findPreference<SwitchPreference>("preference_hide_balance")
        pref?.setOnPreferenceChangeListener { preference, newValue ->
            val switch = preference as SwitchPreference
            val hide = newValue as Boolean

            // require authentication to disable hide balance
            if (!hide) {
                Authentication.instance.authenticate(context!!, null,null) {
                    switch.isChecked = false
                }
                return@setOnPreferenceChangeListener false
            }
            else
                return@setOnPreferenceChangeListener true
        }
    }

    override fun onResume()
    {
        super.onResume()

        try
        {
            val localCurrencyPreference: Preference? = findPreference("preference_local_currency")
            localCurrencyPreference?.summary = localCurrency.code
        }
        catch(e : Exception)
        {
            //TODO: Look into why we have a stack trace with KotlinNullPointerException here.
        }
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean
    {
        when (preference.key){
            "preference_local_currency" ->  (activity as WalletActivity).pushCurrencyPage()
            "preference_show_wallet_settings" ->  (activity as WalletActivity).pushWalletSettingsPage()
        }
        return preference.let { super.onPreferenceTreeClick(it) }
    }
}
