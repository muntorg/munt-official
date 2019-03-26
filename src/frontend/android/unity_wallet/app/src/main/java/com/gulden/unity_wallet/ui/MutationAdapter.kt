// Copyright (c) 2018-2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
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
import com.gulden.jniunifiedbackend.TransactionStatus
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.formatNativeAndLocal
import kotlinx.android.synthetic.main.mutation_list_item.view.*
import kotlinx.android.synthetic.main.mutation_list_item_with_header.view.*
import org.jetbrains.anko.textColor
import java.util.*

class MutationAdapter(context: Context, private var dataSource: ArrayList<MutationRecord>) : BaseAdapter() {
    private val inflater: LayoutInflater = context.getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater
    private var rate = 0.0
    private var mContext = context

    //NB! This may cause a minor cosmetic glitch if user is on transaction screen when year crosses over (years just won't display so nothing too terrible)
    //This is preferable to the performance penalty of constantly creating calendar instances
    private val currentYear = Calendar.getInstance().get(Calendar.YEAR);

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
        val date = if (java.util.Date(mutationRecord.timestamp * 1000L).year + 1900 != currentYear)
        {
            java.text.SimpleDateFormat("dd MMM. yyyy").format(java.util.Date(mutationRecord.timestamp * 1000L))
        }
        else
        {
            java.text.SimpleDateFormat("MMMM dd").format(java.util.Date(mutationRecord.timestamp * 1000L))
        }
        var prevDate = ""
        if (position != 0) {
            val prevMutationRecord = getItem(position-1) as MutationRecord
            prevDate = if (java.util.Date(prevMutationRecord.timestamp * 1000L).year + 1900 != currentYear)
            {
                java.text.SimpleDateFormat("dd MMM. yyyy").format(java.util.Date(prevMutationRecord.timestamp * 1000L))
            }
            else
            {
                java.text.SimpleDateFormat("MMMM dd").format(java.util.Date(prevMutationRecord.timestamp * 1000L))
            }
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
        rowView.textViewAmount.text = formatNativeAndLocal(mutationRecord.change,rate)
        rowView.textViewAmount.textColor = ContextCompat.getColor(rowView.context, if (mutationRecord.change >= 0) R.color.change_positive else R.color.change_negative)

        if (mutationRecord.status == TransactionStatus.CONFIRMED)
        {
            rowView.textViewStatus.text = ""
        }
        else if (mutationRecord.status == TransactionStatus.CONFIRMING)
        {
            rowView.textViewStatus.text = mContext.resources.getQuantityString(R.plurals.transaction_confirmation_text, mutationRecord.depth, mutationRecord.depth);
        }
        else if (mutationRecord.status == TransactionStatus.CONFLICTED)
        {
            rowView.textViewStatus.text = mContext.resources.getString(R.string.transaction_confirmation_text_conflicted)
        }
        else if (mutationRecord.status == TransactionStatus.ABANDONED)
        {
            rowView.textViewStatus.text = mContext.resources.getString(R.string.transaction_confirmation_text_abandoned)
        }
        else if (mutationRecord.status == TransactionStatus.UNCONFIRMED)
        {
            rowView.textViewStatus.text = mContext.resources.getString(R.string.transaction_confirmation_text_pending)
        }
        return rowView
    }
}
