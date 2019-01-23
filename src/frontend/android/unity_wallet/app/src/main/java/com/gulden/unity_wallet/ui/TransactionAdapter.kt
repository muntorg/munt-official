// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter
import com.gulden.jniunifiedbackend.TransactionRecord
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.transaction_list_item.view.*
import kotlinx.android.synthetic.main.transaction_list_item_with_header.view.*

class TransactionAdapter(private val context: Context, private val dataSource: ArrayList<TransactionRecord>) : BaseAdapter() {
    private val inflater: LayoutInflater = context.getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater

    override fun getCount(): Int {
        return dataSource.size
    }

    override fun getItem(position: Int): Any {
        return dataSource[position]
    }

    override fun getItemId(position: Int): Long {
        return position.toLong()
    }

    override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {

        val transactionRecord = getItem(position) as TransactionRecord
        var date = java.text.SimpleDateFormat("dd MMMM").format(java.util.Date(transactionRecord.timestamp * 1000L))
        var prevDate = ""
        if (position != 0) {
            val prevTransactionRecord = getItem(position-1) as TransactionRecord
            prevDate = java.text.SimpleDateFormat("dd MMMM").format(java.util.Date(prevTransactionRecord.timestamp * 1000L))
        }


        var rowView : View
        if (date != prevDate)
        {
            rowView = inflater.inflate(R.layout.transaction_list_item_with_header, parent, false)
            rowView.transactionItemHeading.text = date
        }
        else
        {
            rowView = inflater.inflate(R.layout.transaction_list_item, parent, false)
        }

        rowView.textViewTime.text = java.text.SimpleDateFormat("HH:mm").format(java.util.Date(transactionRecord.timestamp * 1000L))
        rowView.textViewAmount.text = (" %.2f").format(transactionRecord.amount.toDouble() / 100000000)
        return rowView
    }
}

