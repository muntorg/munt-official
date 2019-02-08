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
import androidx.core.content.ContextCompat
import com.gulden.jniunifiedbackend.MutationRecord
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.formatNativeAndLocal
import kotlinx.android.synthetic.main.mutation_list_item.view.*
import kotlinx.android.synthetic.main.mutation_list_item_with_header.view.*
import org.jetbrains.anko.textColor

class MutationAdapter(private val context: Context, private var dataSource: ArrayList<MutationRecord>) : BaseAdapter() {
    private val inflater: LayoutInflater = context.getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater
    private var rate = 0.0

    fun updateRate(rate_: Double) {
        rate = rate_
        notifyDataSetChanged()
    }

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
        val date = java.text.SimpleDateFormat("dd MMMM").format(java.util.Date(mutationRecord.timestamp * 1000L))
        var prevDate = ""
        if (position != 0) {
            val prevMutationRecord = getItem(position-1) as MutationRecord
            prevDate = java.text.SimpleDateFormat("dd MMMM").format(java.util.Date(prevMutationRecord.timestamp * 1000L))
        }


        val rowView : View
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
        rowView.textViewAmount.text = formatNativeAndLocal(mutationRecord.change,rate,false)
        rowView.textViewAmount.textColor = ContextCompat.getColor(rowView.context,
                if (mutationRecord.change >= 0) R.color.change_postivive else R.color.change_negative)
        return rowView
    }
}
