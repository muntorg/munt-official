// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.main_activity_fragments

import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.core.content.ContextCompat
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.gulden.jniunifiedbackend.*
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.SendCoinsFragment
import com.gulden.unity_wallet.ui.AddressBookAdapter
import com.gulden.uriRecipient
import kotlinx.android.synthetic.main.fragment_send.*
import org.apache.commons.validator.routines.IBANValidator
import android.text.Html
import android.text.SpannableString
import com.gulden.ellipsizeString
import com.gulden.unity_wallet.Authentication
import org.jetbrains.anko.sdk27.coroutines.onClick


class SendFragment : Fragment()
{

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        return inflater.inflate(R.layout.fragment_send, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        clipboardButton.setOnClickListener {
            val intent = Intent(context, SendCoinsFragment::class.java)
            val text = clipboardText()
            val recipient = when {
                IBANValidator.getInstance().isValid(text) ->
                    UriRecipient(false, text, "", "")
                GuldenUnifiedBackend.IsValidRecipient(UriRecord("gulden", text, HashMap<String,String>())).valid ->
                    GuldenUnifiedBackend.IsValidRecipient(UriRecord("gulden", text, HashMap<String,String>()))
                uriRecipient(text).valid ->
                    uriRecipient(text)
                else ->
                    null
            }
            if (recipient != null) {
                SendCoinsFragment.newInstance(recipient).show(activity!!.supportFragmentManager, SendCoinsFragment::class.java.simpleName)
            }
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
            SendCoinsFragment.newInstance(recipient).show(activity!!.supportFragmentManager, SendCoinsFragment::class.java.simpleName)
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
            throw RuntimeException("$context must implement OnFragmentInteractionListener")
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

    private fun clipboardText(): String
    {
        val clipboard = ContextCompat.getSystemService(context!!, ClipboardManager::class.java)
        return (clipboard?.primaryClip?.getItemAt(0)?.coerceToText(context)).toString()
    }

    private fun setClipButtonText(text : String)
    {
        val styledText = SpannableString(Html.fromHtml(getString(R.string.send_fragment_clipboard_label) + "<br/>" + "<small> <font color='"+resources.getColor(R.color.lightText)+"'>" + ellipsizeString(text, 18) + "</font> </small>"))
        clipboardButton.text = styledText
    }

    private fun checkClipboardEnable()
    {
        // Enable clipboard button if it contains a valid IBAN, Gulden address or Uri
        val text = clipboardText()
        when
        {
            IBANValidator.getInstance().isValid(text) || GuldenUnifiedBackend.IsValidRecipient(UriRecord("gulden", text, HashMap<String,String>())).valid ->
            {
                clipboardButton.isEnabled = true
                setClipButtonText(text)
            }
            uriRecipient(text).valid ->
            {
                clipboardButton.isEnabled = true
                setClipButtonText(uriRecipient(text).address)
            }
            else ->
            {
                clipboardButton.text = getString(R.string.send_fragment_clipboard_label)
                clipboardButton.isEnabled = false
            }
        }
    }
}
