// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.MainActivityFragments

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.support.v4.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.gulden.jniunifiedbackend.*
import com.gulden.unity_wallet.WalletActivity
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.SendCoinsActivity
import com.gulden.unity_wallet.ui.AddressBookAdapter
import kotlinx.android.synthetic.main.fragment_send.*


class SendFragment : Fragment()
{
    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        return inflater.inflate(R.layout.fragment_send, container, false);
    }

    override fun onActivityCreated(savedInstanceState: Bundle?)
    {
        super.onActivityCreated(savedInstanceState)

        addressBookList?.emptyView = emptyAddressBookView;

        addressBookList.setOnItemClickListener { parent, _, position, _ ->
            val address = parent.adapter.getItem(position) as AddressRecord
            val recipient = UriRecipient(true, address.address, address.name, "0")
            val intent = Intent(this.context, SendCoinsActivity::class.java)
            intent.putExtra(SendCoinsActivity.EXTRA_RECIPIENT, recipient);
            startActivityForResult(intent, WalletActivity.SEND_COINS_RETURN_CODE)
        };

        // TODO: Only update if there has been a change, not always.
        val addresses = GuldenUnifiedBackend.getAddressBookRecords();
        val adapter = AddressBookAdapter(this.context!!, addresses)
        addressBookList.adapter = adapter;
    }

    override fun onAttachFragment(childFragment: Fragment?)
    {
        super.onAttachFragment(childFragment)
    }

    override fun onResume()
    {
        super.onResume()
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

    override fun onDetach()
    {
        super.onDetach()
        listener = null
    }

    private var listener: OnFragmentInteractionListener? = null
    interface OnFragmentInteractionListener
    {
        fun onFragmentInteraction(uri: Uri)
    }
}
