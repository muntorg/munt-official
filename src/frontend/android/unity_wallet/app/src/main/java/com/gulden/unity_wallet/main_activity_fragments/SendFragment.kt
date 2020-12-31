// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.main_activity_fragments

import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.text.Editable
import android.text.Html
import android.text.SpannableString
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import androidx.appcompat.app.AlertDialog
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.vision.barcode.Barcode
import com.gulden.barcodereader.BarcodeCaptureActivity
import com.gulden.jniunifiedbackend.AddressRecord
import com.gulden.jniunifiedbackend.ILibraryController
import com.gulden.jniunifiedbackend.UriRecipient
import com.gulden.unity_wallet.*
import com.gulden.unity_wallet.ui.AddressBookAdapter
import com.gulden.unity_wallet.util.AppBaseFragment
import com.gulden.unity_wallet.util.SwipeToDeleteCallback
import kotlinx.android.synthetic.main.add_address_entry.view.*
import kotlinx.android.synthetic.main.fragment_send.*
import org.jetbrains.anko.support.v4.runOnUiThread


class SendFragment : AppBaseFragment(), UnityCore.Observer {

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        return inflater.inflate(R.layout.fragment_send, container, false)
    }

    private fun handleAddToAddressBookButtonClick(view : View)
    {
        val builder = AlertDialog.Builder(this.context!!)
        builder.setTitle(getString(R.string.dialog_title_add_address))
        val layoutInflater : LayoutInflater = this.context!!.getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater
        val viewInflated : View = layoutInflater.inflate(R.layout.add_address_entry, view.rootView as ViewGroup, false)
        val inputAddress = viewInflated.findViewById(R.id.address) as EditText
        val inputLabel = viewInflated.findViewById(R.id.addressName) as EditText
        builder.setView(viewInflated)
        builder.setPositiveButton(android.R.string.ok) { dialog, _ ->
            val address = inputAddress.text.toString()
            val label = inputLabel.text.toString()
            val record = AddressRecord(address, label, "", "Send")
            UnityCore.instance.addAddressBookRecord(record)
            dialog.dismiss()
        }

        builder.setNegativeButton(android.R.string.cancel) { dialog, _ -> dialog.cancel() }
        val dialog = builder.create()
        dialog.setOnShowListener {
            val okBtn = dialog.getButton(AlertDialog.BUTTON_POSITIVE)
            okBtn.isEnabled = false

            val textChangedListener = object : TextWatcher {
                override fun afterTextChanged(s: Editable?) {
                    s?.run {
                        val address = inputAddress.text.toString()
                        val label = inputLabel.text.toString()
                        var enable = false
                        if (address != "" && label != "")
                        {
                            try {
                                createRecipient(address)
                                enable = true
                            }
                            catch (e: InvalidRecipientException) {
                                // silently ignore, use can continue typing until a valid recipient is entered
                            }
                        }
                        okBtn.isEnabled = enable
                    }
                }
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
            }

            inputAddress.addTextChangedListener(textChangedListener)
            inputLabel.addTextChangedListener(textChangedListener)

            viewInflated.address.requestFocus()
            viewInflated.address.post {
                val imm = this.context!!.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                imm.showSoftInput(viewInflated.address, InputMethodManager.SHOW_IMPLICIT)
            }
        }
        dialog.show()
    }

    override fun onWalletReady() {
        clipboardButton.setOnClickListener {
            val text = clipboardText()
            try {
                SendCoinsFragment.newInstance(createRecipient(text), false).show(activity!!.supportFragmentManager, SendCoinsFragment::class.java.simpleName)
            }
            catch (e: InvalidRecipientException) {
                errorMessage(getString(R.string.clipboard_no_valid_address))
            }
        }

        imageViewAddToAddressBook.setOnClickListener {
            handleAddToAddressBookButtonClick(view!!)
        }

        qrButton.setOnClickListener {
            val intent = Intent(context, BarcodeCaptureActivity::class.java)
            intent.putExtra(BarcodeCaptureActivity.AutoFocus, true)
            startActivityForResult(intent, BARCODE_READER_REQUEST_CODE)
        }

        ClipboardManager.OnPrimaryClipChangedListener { checkClipboardEnable() }

        val addresses = ILibraryController.getAddressBookRecords()
        val adapter = AddressBookAdapter(addresses) { position, address ->
            val recipient = UriRecipient(true, address.address, address.name, address.desc, 0)
            SendCoinsFragment.newInstance(recipient, false).show(activity!!.supportFragmentManager, SendCoinsFragment::class.java.simpleName)
        }

        addressBookList.adapter = adapter
        addressBookList.layoutManager = LinearLayoutManager(context)

        val swipeHandler = object : SwipeToDeleteCallback(context!!) {
            override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {
                adapter.removeAt(viewHolder.adapterPosition)
            }
        }
        val itemTouchHelper = ItemTouchHelper(swipeHandler)
        itemTouchHelper.attachToRecyclerView(addressBookList)

        updateEmptyViewState()

        checkClipboardEnable()
    }

    private fun updateEmptyViewState()
    {
        val adapter = addressBookList.adapter
        adapter?.run {
            if (itemCount > 0) {
                addressBookList.visibility = View.VISIBLE
                emptyAddressBookView.visibility = View.GONE
            }
            else {
                addressBookList.visibility = View.GONE
                emptyAddressBookView.visibility = View.VISIBLE
            }
        }
    }

    override fun onAttach(context: Context)
    {
        UnityCore.instance.addObserver(this)

        super.onAttach(context)
    }

    override fun onDetach() {
        UnityCore.instance.removeObserver(this)

        super.onDetach()
    }

    private fun clipboardText(): String
    {
        try
        {
            val clipboard = ContextCompat.getSystemService(context!!, ClipboardManager::class.java)
            return (clipboard?.primaryClip?.getItemAt(0)?.coerceToText(context)).toString()
        }
        catch (e : Exception)
        {
            //TODO: We are receiving SecurityException here on some LG G3 android 6 devices, during performResume() - look into in more detail
            return ""
        }
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
        try {
            setClipButtonText(createRecipient(text).address)
        }
        catch (e: Throwable) {
            clipboardButton.text = getString(R.string.send_fragment_clipboard_label)
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == BARCODE_READER_REQUEST_CODE) {
            if (resultCode == CommonStatusCodes.SUCCESS)
            {
                if (data != null) {
                    val barcode = data.getParcelableExtra<Barcode>(BarcodeCaptureActivity.BarcodeObject)
                    val qrContent = barcode.displayValue
                    val recipient = try {
                        createRecipient(qrContent)
                    }
                    catch (e: InvalidRecipientException) {
                        errorMessage(getString(R.string.not_gulden_qr, qrContent))
                        return
                    }
                    SendCoinsFragment.newInstance(recipient, false).show(activity!!.supportFragmentManager, SendCoinsFragment::class.java.simpleName)
                }
            }
        } else {
            super.onActivityResult(requestCode, resultCode, data)
        }
    }

    override fun onAddressBookChanged() {
        val newAddresses = ILibraryController.getAddressBookRecords()
        runOnUiThread {
            val adapter = addressBookList.adapter as AddressBookAdapter
            adapter.updateDataSource(newAddresses)
            updateEmptyViewState()
        }
    }

    companion object {
        private const val BARCODE_READER_REQUEST_CODE = 1
    }
}
