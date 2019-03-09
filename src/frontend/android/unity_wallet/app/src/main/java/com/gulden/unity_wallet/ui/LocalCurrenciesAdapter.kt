// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui

import android.content.Context
import android.preference.PreferenceManager
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter
import com.gulden.unity_wallet.*
import com.gulden.unity_wallet.Currency
import kotlinx.android.synthetic.main.local_currency_list_item.view.*
import java.util.*

class LocalCurrenciesAdapter(context: Context, private val dataSource: TreeMap<String, Currency>) : BaseAdapter() {
    private val inflater: LayoutInflater = context.getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater

    override fun getCount(): Int {
        return dataSource.size
    }

    override fun getItem(position: Int) : Currency?
    {
        return dataSource.values.toTypedArray()[position]
    }

    override fun getItemId(position: Int): Long {
        return position.toLong()
    }

    override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {

        val currencyRecord = getItem(position) as Currency
        val rowView = inflater.inflate(R.layout.local_currency_list_item, parent, false)
        rowView.textViewCurrencyName.text = currencyRecord.name
        rowView.textViewCurrencyCode.text = currencyRecord.code
        rowView.imageViewCurrencySelected.visibility = if (currencyRecord.code == localCurrency.code)
        {
            View.VISIBLE
        }
        else
        {
            View.GONE
        }
        return rowView
    }

    fun setSelectedPosition(position: Int) = PreferenceManager.getDefaultSharedPreferences(AppContext.instance).edit().putString("preference_local_currency", getItem(position)?.code).apply()
}

