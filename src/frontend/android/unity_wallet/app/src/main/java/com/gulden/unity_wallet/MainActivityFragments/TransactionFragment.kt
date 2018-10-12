// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.MainActivityFragments

import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.support.v4.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.ui.TransactionAdapter
import kotlinx.android.synthetic.main.fragment_transaction.*
import android.content.Intent
import com.gulden.jniunifiedbackend.TransactionRecord
import com.gulden.unity_wallet.TransactionInfoActivity


class TransactionFragment : Fragment() {
    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        return inflater.inflate(R.layout.fragment_transaction, container, false);
    }

    override fun onActivityCreated(savedInstanceState: Bundle?)
    {
        super.onActivityCreated(savedInstanceState)

        transactionList?.emptyView = emptyTransactionListView

        val transactions = GuldenUnifiedBackend.getTransactionHistory();

        val adapter = TransactionAdapter(this.context!!, transactions)
        transactionList.adapter = adapter;

        transactionList.setOnItemClickListener { parent, _, position, _ ->
            val transaction = parent.adapter.getItem(position) as TransactionRecord
            val intent = Intent(this.context, TransactionInfoActivity::class.java)
            intent.putExtra(TransactionInfoActivity.EXTRA_TRANSACTION, transaction);
            startActivity(intent);
        };
    }

    override fun onAttach(context: Context)
    {
        super.onAttach(context)
        if (context is OnFragmentInteractionListener)
        {
            listener = context
        }
        else
        {
            throw RuntimeException(context.toString() + " must implement OnFragmentInteractionListener")
        }
    }

    override fun onDetach() {
        super.onDetach()
        listener = null
    }

    private var listener: OnFragmentInteractionListener? = null
    interface OnFragmentInteractionListener
    {
        fun onFragmentInteraction(uri: Uri)
    }
}
