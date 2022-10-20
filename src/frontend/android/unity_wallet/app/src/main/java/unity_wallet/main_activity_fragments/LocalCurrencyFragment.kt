// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

package unity_wallet.main_activity_fragments

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import unity_wallet.Currencies
import unity_wallet.R
import unity_wallet.WalletActivity
import unity_wallet.fetchAllCurrencyRates
import unity_wallet.ui.LocalCurrenciesAdapter
import unity_wallet.util.AppBaseActivity
import kotlinx.android.synthetic.main.fragment_local_currencies.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch


class LocalCurrencyFragment : androidx.fragment.app.Fragment() {

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        return inflater.inflate(R.layout.fragment_local_currencies, container, false)
    }

    override fun onResume()
    {
        super.onResume()
        (activity as WalletActivity).showSettingsTitle(getString(R.string.title_select_currency))
    }

    override fun onStop()
    {
        super.onStop()
        (activity as WalletActivity).hideSettingsTitle()
    }

    override fun onActivityCreated(savedInstanceState: Bundle?)
    {
        super.onActivityCreated(savedInstanceState)

        // Get conversion rates
        (this.activity as AppBaseActivity).launch(Dispatchers.Main) {
            try {
                (currenciesList.adapter as LocalCurrenciesAdapter).updateAllRates(fetchAllCurrencyRates())
            } catch (e: Throwable) {
                // silently ignore failure of getting rate here
            }
        }

        val adapter = LocalCurrenciesAdapter(this.requireContext(), Currencies.knownCurrencies)
        currenciesList.adapter = adapter
        currenciesList.setOnItemClickListener { parent, _, position, _ ->
            adapter.setSelectedPosition(position)
            adapter.notifyDataSetChanged()
        }
    }

}
