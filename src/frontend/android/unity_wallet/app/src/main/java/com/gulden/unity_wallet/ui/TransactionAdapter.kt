package com.gulden.unity_wallet.ui

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter
import com.gulden.jniunifiedbackend.TransactionRecord
import com.gulden.jniunifiedbackend.TransactionType
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.transaction_list_item.view.*

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

        var rowView = inflater.inflate(R.layout.transaction_list_item, parent, false)
        val transactionRecord = getItem(position) as TransactionRecord

        var prefix = "+";
        if (transactionRecord.type == TransactionType.SEND)
            prefix = "-";

        rowView.textViewTime.text = java.text.SimpleDateFormat("HH:mm").format(java.util.Date(transactionRecord.timestamp * 1000L))
        rowView.textViewAmount.text = transactionRecord.amount.toString();
        return rowView;
    }
}

