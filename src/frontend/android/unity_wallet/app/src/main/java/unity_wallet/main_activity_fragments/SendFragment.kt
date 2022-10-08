// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

package unity_wallet.main_activity_fragments

import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
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
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.android.volley.*
import com.android.volley.toolbox.StringRequest
import com.android.volley.toolbox.Volley
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.vision.barcode.Barcode
import barcodereader.BarcodeCaptureActivity
import unity_wallet.jniunifiedbackend.AddressRecord
import unity_wallet.jniunifiedbackend.ILibraryController
import unity_wallet.jniunifiedbackend.IWalletController
import unity_wallet.jniunifiedbackend.UriRecipient
import unity_wallet.*
import unity_wallet.ui.AddressBookAdapter
import unity_wallet.util.AppBaseFragment
import unity_wallet.util.SwipeToDeleteCallback
import kotlinx.android.synthetic.main.add_address_entry.view.*
import kotlinx.android.synthetic.main.fragment_send.*
import android.os.Handler
import android.os.Looper
import org.json.JSONObject


class SendFragment : AppBaseFragment(), UnityCore.Observer {

    private val handler = Handler(Looper.getMainLooper())

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        return inflater.inflate(unity_wallet.R.layout.fragment_send, container, false)
    }

    private fun handleAddToAddressBookButtonClick(view : View)
    {
        val builder = AlertDialog.Builder(this.requireContext())
        builder.setTitle(getString(unity_wallet.R.string.dialog_title_add_address))
        val layoutInflater : LayoutInflater = this.requireContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater
        val viewInflated : View = layoutInflater.inflate(unity_wallet.R.layout.add_address_entry, view.rootView as ViewGroup, false)
        val inputAddress = viewInflated.findViewById(unity_wallet.R.id.address) as EditText
        val inputLabel = viewInflated.findViewById(unity_wallet.R.id.addressName) as EditText
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
                val imm = this.requireContext().getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                imm.showSoftInput(viewInflated.address, InputMethodManager.SHOW_IMPLICIT)
            }
        }
        dialog.show()
    }

    override fun onWalletReady() {
        clipboardButton.setOnClickListener {
            val text = clipboardText()
            try {
                SendCoinsFragment.newInstance(createRecipient(text), false).show(requireActivity().supportFragmentManager, SendCoinsFragment::class.java.simpleName)
            }
            catch (e: InvalidRecipientException) {
                errorMessage(getString(unity_wallet.R.string.clipboard_no_valid_address))
            }
        }

        imageViewAddToAddressBook.setOnClickListener {
            handleAddToAddressBookButtonClick(requireView())
        }

        qrButton.setOnClickListener {
            val intent = Intent(context, BarcodeCaptureActivity::class.java)
            intent.putExtra(BarcodeCaptureActivity.AutoFocus, true)
            startActivityForResult(intent, BARCODE_READER_REQUEST_CODE)
        }

        sellButton.setOnClickListener {
            // Send a post request to blockhut with our wallet/address info; and then launch the site if we get a positive response.
            val MyRequestQueue = Volley.newRequestQueue(context)
            val failURL = "https://munt.org/sell"
            val request = object : StringRequest(Request.Method.POST,"https://blockhut.com/buysession.php",
                Response.Listener { response ->
                    try
                    {
                        var jsonResponse = JSONObject(response);
                        if (jsonResponse.getInt("status_code") == 200)
                        {
                            var sessionID = jsonResponse.getString("sessionid")
                            val intent = Intent(Intent.ACTION_VIEW, Uri.parse("https://blockhut.com/sell.php?sessionid=%s".format(sessionID)))
                            startActivity(intent)
                        }
                        else
                        {
                            // Redirect user to the default fallback site
                            //fixme: Do something with the status message here
                            //var statusMessage = jsonResponse.getString("status_message")
                            val intent = Intent(failURL)
                            startActivity(intent)
                        }
                    }
                    catch (e:Exception)
                    {
                        // Redirect user to the default fallback site
                        //fixme: Do something with the error message here
                        val intent = Intent(failURL)
                        startActivity(intent)
                    }
                },
                Response.ErrorListener
                {
                   // If we are sure its a local connectivity issue, alert the user, otherwise send them to the default fallback site
                   if (it is NetworkError || it is AuthFailureError || it is NoConnectionError)
                   {
                       Toast.makeText(context, getString(unity_wallet.R.string.error_check_internet_connection), Toast.LENGTH_SHORT).show()
                   }
                   else
                   {
                       // Redirect user to the default fallback site
                       //fixme: Do something with the status message here
                       //var statusMessage = jsonResponse.getString("status_message")
                       val intent = Intent(failURL)
                       startActivity(intent)
                   }
                }
            )
            // Force values to be send at x-www-form-urlencoded
            {
                override fun getParams(): MutableMap<String, String> {
                    val params = HashMap<String,String>()
                    params["address"] = ILibraryController.GetReceiveAddress().toString()
                    params["uuid"] = IWalletController.GetUUID()
                    params["currency"] = "munt"
                    params["wallettype"] = "android"
                    return params
                }
                override fun getHeaders(): MutableMap<String, String> {
                    val params = HashMap<String, String>()
                    params.put("Content-Type","application/x-www-form-urlencoded")
                    return params
                }
            }

            // Volley request policy, only one time request to avoid duplicate transaction
            request.retryPolicy = DefaultRetryPolicy(
                    DefaultRetryPolicy.DEFAULT_TIMEOUT_MS,
                    1, // DefaultRetryPolicy.DEFAULT_MAX_RETRIES = 2
                    1f // DefaultRetryPolicy.DEFAULT_BACKOFF_MULT
            )

            // Add the volley post request to the request queue
            MyRequestQueue.add(request)
        }

        ClipboardManager.OnPrimaryClipChangedListener { checkClipboardEnable() }

        val addresses = ILibraryController.getAddressBookRecords()
        val adapter = AddressBookAdapter(addresses) { position, address ->
            val recipient = UriRecipient(true, address.address, address.name, address.desc, 0)
            SendCoinsFragment.newInstance(recipient, false).show(requireActivity().supportFragmentManager, SendCoinsFragment::class.java.simpleName)
        }

        addressBookList.adapter = adapter
        addressBookList.layoutManager = LinearLayoutManager(context)

        val swipeHandler = object : SwipeToDeleteCallback(requireContext()) {
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
        val styledText = SpannableString(Html.fromHtml(getString(unity_wallet.R.string.send_fragment_clipboard_label) + "<br/>" + "<small> <font color='"+resources.getColor(unity_wallet.R.color.text_button_secondary)+"'>" + ellipsizeString(text, 18) + "</font> </small>"))
        clipboardButton.text = styledText
    }

    private fun checkClipboardEnable()
    {
        // Enable clipboard button if it contains a valid IBAN, Munt address or Uri
        val text = clipboardText()
        try {
            setClipButtonText(createRecipient(text).address)
        }
        catch (e: Throwable) {
            clipboardButton.text = getString(unity_wallet.R.string.send_fragment_clipboard_label)
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == BARCODE_READER_REQUEST_CODE) {
            if (resultCode == CommonStatusCodes.SUCCESS)
            {
                if (data != null) {
                    val barcode = data.getParcelableExtra<Barcode>(BarcodeCaptureActivity.BarcodeObject)
                    val qrContent = barcode?.displayValue
                    val recipient = try {
                        createRecipient(qrContent!!)
                    }
                    catch (e: InvalidRecipientException) {
                        errorMessage(getString(unity_wallet.R.string.not_coin_qr, qrContent))
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
        handler.post {
            val adapter = addressBookList.adapter as AddressBookAdapter
            adapter.updateDataSource(newAddresses)
            updateEmptyViewState()
        }
    }

    companion object {
        private const val BARCODE_READER_REQUEST_CODE = 1
    }
}
