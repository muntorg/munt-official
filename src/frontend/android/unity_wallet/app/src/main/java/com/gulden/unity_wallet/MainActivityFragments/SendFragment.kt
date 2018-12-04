// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.MainActivityFragments

import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.support.v4.app.Fragment
import android.support.v4.content.ContextCompat
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.gulden.jniunifiedbackend.*
import com.gulden.unity_wallet.WalletActivity
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.SendCoinsActivity
import com.gulden.unity_wallet.ui.AddressBookAdapter
import kotlinx.android.synthetic.main.fragment_send.*
import org.apache.commons.validator.routines.IBANValidator


class SendFragment : Fragment()
{

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        return inflater.inflate(R.layout.fragment_send, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        clipboardButton.setOnClickListener {
            val intent = Intent(context, SendCoinsActivity::class.java)
            val recipient = UriRecipient(false, clipboardText(), "", "")
            intent.putExtra(SendCoinsActivity.EXTRA_RECIPIENT, recipient)
            startActivityForResult(intent, WalletActivity.SEND_COINS_RETURN_CODE)
        }

        ClipboardManager.OnPrimaryClipChangedListener { checkClipboardEnable() }
    }

    override fun onResume() {
        super.onResume()
        checkClipboardEnable()
    }

    override fun onActivityCreated(savedInstanceState: Bundle?)
    {
        super.onActivityCreated(savedInstanceState)

        addressBookList?.emptyView = emptyAddressBookView

        addressBookList.setOnItemClickListener { parent, _, position, _ ->
            val address = parent.adapter.getItem(position) as AddressRecord
            val recipient = UriRecipient(true, address.address, address.name, "0")
            val intent = Intent(this.context, SendCoinsActivity::class.java)
            intent.putExtra(SendCoinsActivity.EXTRA_RECIPIENT, recipient)
            startActivityForResult(intent, WalletActivity.SEND_COINS_RETURN_CODE)
        }

        // TODO: Only update if there has been a change, not always.
        val addresses = GuldenUnifiedBackend.getAddressBookRecords()
        val adapter = AddressBookAdapter(this.context!!, addresses)
        addressBookList.adapter = adapter
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

    fun clipboardText(): String
    {
        val clipboard = ContextCompat.getSystemService(context!!, ClipboardManager::class.java)
        return (clipboard?.primaryClip?.getItemAt(0)?.coerceToText(context)).toString()
    }

    fun checkClipboardEnable()
    {
        // Enable clipboard button if it contains a valid IBAN
        // TODO: enable button if there is a valid Gulden address (or URI) on the clipboard
        val address = clipboardText()
        clipboardButton.isEnabled = IBANValidator.getInstance().isValid(address)
    }
}
