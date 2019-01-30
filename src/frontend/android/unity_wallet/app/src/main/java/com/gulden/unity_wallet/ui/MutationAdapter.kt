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
import com.gulden.jniunifiedbackend.MutationRecord
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.mutation_list_item.view.*
import kotlinx.android.synthetic.main.mutation_list_item_with_header.view.*
import java.text.DecimalFormat

class MutationAdapter(private val context: Context, private var dataSource: ArrayList<MutationRecord>) : BaseAdapter() {
    private val inflater: LayoutInflater = context.getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater

    fun updateDataSource(newDataSource: ArrayList<MutationRecord>) {
        dataSource = newDataSource
        notifyDataSetChanged()
    }

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

        val mutationRecord = getItem(position) as MutationRecord
        var date = java.text.SimpleDateFormat("dd MMMM").format(java.util.Date(mutationRecord.timestamp * 1000L))
        var prevDate = ""
        if (position != 0) {
            val prevMutationRecord = getItem(position-1) as MutationRecord
            prevDate = java.text.SimpleDateFormat("dd MMMM").format(java.util.Date(prevMutationRecord.timestamp * 1000L))
        }


        var rowView : View
        if (date != prevDate)
        {
            rowView = inflater.inflate(R.layout.mutation_list_item_with_header, parent, false)
            rowView.transactionItemHeading.text = date
        }
        else
        {
            rowView = inflater.inflate(R.layout.mutation_list_item, parent, false)
        }

        rowView.textViewTime.text = java.text.SimpleDateFormat("HH:mm").format(java.util.Date(mutationRecord.timestamp * 1000L))
        rowView.textViewAmount.text = "  " + (DecimalFormat("+#,##0.00;-#").format(mutationRecord.change.toDouble() / 100000000))
        return rowView
    }
}
