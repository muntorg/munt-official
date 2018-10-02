package com.gulden.unity_wallet.ui

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter
import com.gulden.jniunifiedbackend.AddressRecord
import com.gulden.jniunifiedbackend.TransactionRecord
import com.gulden.jniunifiedbackend.TransactionType
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.address_book_list_item.view.*
import kotlinx.android.synthetic.main.transaction_list_item.view.*
import kotlinx.android.synthetic.main.transaction_list_item_with_header.view.*

class AddressBookAdapter(private val context: Context, private val dataSource: ArrayList<AddressRecord>) : BaseAdapter() {
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

        val addressRecord = getItem(position) as AddressRecord
        val rowView = inflater.inflate(R.layout.address_book_list_item, parent, false)
        rowView.textViewLabel.text = addressRecord.name;
        return rowView;
    }
}

