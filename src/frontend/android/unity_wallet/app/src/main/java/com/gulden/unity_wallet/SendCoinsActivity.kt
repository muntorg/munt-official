// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.app.Dialog
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.view.View
import com.gulden.jniunifiedbackend.AddressRecord
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.UriRecipient

import kotlinx.android.synthetic.main.activity_send_coins.*
import android.content.Context
import android.os.Build
import android.support.design.widget.Snackbar
import android.support.v4.app.DialogFragment
import android.support.v4.text.HtmlCompat.FROM_HTML_MODE_LEGACY
import android.support.v7.app.AlertDialog
import android.text.Html
import android.widget.EditText
import android.view.ViewGroup
import android.view.LayoutInflater
import com.gulden.unity_wallet.R.layout.text_input_address_label
import com.gulden.unity_wallet.currency.Currencies
import com.gulden.unity_wallet.currency.fetchCurrencyRate
import com.gulden.unity_wallet.currency.localCurrency
import kotlinx.android.synthetic.main.text_input_address_label.view.*
import kotlinx.coroutines.*
import org.apache.commons.validator.routines.IBANValidator
import kotlin.coroutines.CoroutineContext
import kotlin.text.*


class SendCoinsConfirmDialog : DialogFragment() {

    private lateinit var mListener: ConfirmDialogListener

    interface ConfirmDialogListener {
        fun onConfirmDialogPositive(dialog: DialogFragment)
        fun onConfirmDialogNegative(dialog: DialogFragment)
    }

    override fun onAttach(context: Context) {
        super.onAttach(context)
        // Verify that the host activity implements the callback interface
        try {
            mListener = context as ConfirmDialogListener
        } catch (e: ClassCastException) {
            throw ClassCastException((context.toString() +
                    " must implement ConfirmDialogListener"))
        }
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return activity?.let {

            // create styled message from resource template and arguments bundle
            val message = getString(R.string.send_coins_confirm_template,
                    arguments?.getString("nlg"),
                    arguments?.getString("to"))

            val styledMessage =
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                        Html.fromHtml(message, FROM_HTML_MODE_LEGACY)
                    } else {
                        Html.fromHtml(message)
                    }

            val builder = AlertDialog.Builder(it)
            builder.setTitle("Send Gulden?")
                    .setMessage(styledMessage)
                    .setPositiveButton("Send") { _, _ ->
                        mListener.onConfirmDialogPositive(this)
                    }
                    .setNegativeButton("Cancel") { _, _ ->
                        mListener.onConfirmDialogNegative(this)
                    }
            builder.create()
        } ?: throw IllegalStateException("Activity cannot be null")
    }
}

class SendCoinsActivity : AppCompatActivity(), CoroutineScope,
        SendCoinsConfirmDialog.ConfirmDialogListener
{
    override fun onConfirmDialogPositive(dialog: DialogFragment) {
            val paymentRequest = UriRecipient(true, recipient.address, recipient.label, activeAmount.text.toString())
            if (GuldenUnifiedBackend.performPaymentToRecipient(paymentRequest)) {
                finish()
            }
            else {
                val view =
                Snackbar.make(findViewById<View>(android.R.id.content),
                        "Payment failed", Snackbar.LENGTH_LONG)
                        .setAction("Action", null)
                        .show()
            }
    }

    override fun onConfirmDialogNegative(dialog: DialogFragment) {}

    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()
    private var nocksJob: Job? = null
    private lateinit var activeAmount: EditText
    private var localRate: Double = 0.0
    private lateinit var recipient: UriRecipient
    private var foreignCurrency = localCurrency
    private var isIBAN = false
    private val amount: Double
        get() {
            var a = send_coins_amount.text.toString().toDoubleOrNull()
            if (a == null)
                a = 0.0
            return a
        }
    private val foreignAmount: Double
        get() {
            var a = send_coins_local_amount.text.toString().toDoubleOrNull()
            if (a == null)
                a = 0.0
            return a
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_send_coins)
        setSupportActionBar(toolbar)

        recipient = intent.getParcelableExtra(EXTRA_RECIPIENT)
        activeAmount = send_coins_amount
        activeAmount.setText(recipient.amount)
        send_coins_receiving_static_address.text = recipient.address

        setAddressLabel(recipient.label)

        send_coins_send_btn.setOnClickListener { view ->
            run {
                if (activeAmount.text.length <= 0) {
                    Snackbar.make(view, "Enter an amount to pay", Snackbar.LENGTH_LONG)
                            .setAction("Action", null)
                            .show()
                    return@run
                }

                val dialog = SendCoinsConfirmDialog()

                // pass arguments to dialog for user message composition
                val bundle = Bundle()
                bundle.putString("nlg", String.format("%.${Config.PRECISION_SHORT}f", amount))
                bundle.putString("to", if (recipient.label.isEmpty()) recipient.address else "${recipient.label} (${recipient.address})")
                dialog.arguments = bundle

                dialog.show(supportFragmentManager, "SendCoinsConfirmFragment")
            }
        }

        send_coins_amount.setOnFocusChangeListener { v, hasFocus ->
            if (hasFocus) activeAmount = send_coins_amount
        }

        send_coins_local_amount.setOnFocusChangeListener { v, hasFocus ->
            if (hasFocus) activeAmount = send_coins_local_amount
        }

        if (IBANValidator.getInstance().isValid(recipient.address)) {
            foreignCurrency = Currencies.knownCurrencies["EUR"]!!
            isIBAN = true
        }
        else {
            foreignCurrency = localCurrency
            isIBAN = false
        }

        setupRate()
    }

    override fun onDestroy() {
        super.onDestroy()

        coroutineContext[Job]!!.cancel()
    }

    fun setupRate()
    {
        this.launch( Dispatchers.Main) {
            try {
                localRate = fetchCurrencyRate(foreignCurrency.code)
                send_coins_local_label.text = foreignCurrency.short
                send_coins_local_group.visibility = View.VISIBLE

                updateConversion()

                if (isIBAN)
                    send_coins_local_amount.requestFocus()
            }
            catch (e: Throwable) {
                send_coins_local_group.visibility = View.GONE
            }
        }
    }

    fun updateConversion()
    {
        if (localRate <= 0.0)
            return

        if (activeAmount == send_coins_amount) {
            // update local from Gulden
            send_coins_local_amount.setText(
                    if (amount != 0.0)
                        String.format("%.${foreignCurrency.precision}f", localRate * amount)
                    else
                        ""
            )
        }
        else {
            // update Gulden from local
            send_coins_amount.setText(
                    if (foreignAmount != 0.0)
                        String.format("%.${Config.PRECISION_SHORT}f", foreignAmount / localRate)
                    else
                        ""
            )
        }
    }

    private fun updateNocksEstimate() {
        nocksJob?.cancel()
        send_coins_nocks_estimate.text = " "
        if (isIBAN && foreignAmount != 0.0) {
            val prevJob = nocksJob
            nocksJob = this.launch(Dispatchers.Main) {
                try {
                    send_coins_nocks_estimate.text = "..."

                    // delay a bit so quick typing will make a limited number of requests
                    // (this job will be canceled by the next key typed
                    delay(700)

                    prevJob?.join()

                    val quote = nocksQuote(send_coins_local_amount.text.toString())
                    val nlg = String.format("%.${Config.PRECISION_SHORT}f", quote.amountNLG.toDouble())
                    send_coins_nocks_estimate.text = getString(R.string.send_coins_nocks_estimate_template, nlg)
                }
                catch (_: CancellationException) {
                    // silently pass job cancelation
                }
                catch (e: Throwable) {
                    send_coins_nocks_estimate.text = "Could not fetch transaction quote"
                }
            }
        }
    }

    fun setAddressLabel(label : String)
    {
        send_coins_receiving_static_label.text = label
        setAddressHasLabel(label.isNotEmpty())
    }

    fun setAddressHasLabel(hasLabel : Boolean)
    {
        if (hasLabel)
        {
            send_coins_receiving_static_label.visibility = View.VISIBLE
            labelRemoveFromAddressBook.visibility = View.VISIBLE
            labelAddToAddressBook.visibility = View.GONE
        }
        else
        {
            send_coins_receiving_static_label.visibility = View.GONE
            labelRemoveFromAddressBook.visibility = View.GONE
            labelAddToAddressBook.visibility = View.VISIBLE
        }
    }

    fun appendNumberToAmount(number : String)
    {
        if (activeAmount.text.toString() == "0")
            activeAmount.setText(number)
        else
            activeAmount.setText(activeAmount.text.toString() + number)
    }

    fun handleKeypadButtonClick(view : View)
    {
        when (view.id)
        {
            R.id.button_1 -> appendNumberToAmount("1")
            R.id.button_2 -> appendNumberToAmount("2")
            R.id.button_3 -> appendNumberToAmount("3")
            R.id.button_4 -> appendNumberToAmount("4")
            R.id.button_5 -> appendNumberToAmount("5")
            R.id.button_6 -> appendNumberToAmount("6")
            R.id.button_7 -> appendNumberToAmount("7")
            R.id.button_8 -> appendNumberToAmount("8")
            R.id.button_9 -> appendNumberToAmount("9")
            R.id.button_0 -> {
                if (activeAmount.text.isEmpty())
                    activeAmount.setText(activeAmount.text.toString() + "0.")
                else if (activeAmount.text.toString() != "0")
                    activeAmount.setText(activeAmount.text.toString() + "0")
            }
            R.id.button_backspace -> {
                if (activeAmount.text.toString() == "0.")
                    activeAmount.setText("")
                else
                    activeAmount.setText(activeAmount.text.dropLast(1))
            }
            R.id.button_decimal -> {
                if (!activeAmount.text.contains("."))
                {
                    if (activeAmount.text.isEmpty())
                        activeAmount.setText("0.")
                    else
                        activeAmount.setText(activeAmount.text.toString() + ".")
                }
            }
        }
        updateConversion()
        updateNocksEstimate()
    }

    fun handleAddToAddressBookClick(view : View)
    {
        val builder = AlertDialog.Builder(this)
        builder.setTitle("Add address")
        val layoutInflater : LayoutInflater = getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater
        val viewInflated : View = layoutInflater.inflate(text_input_address_label, view.rootView as ViewGroup, false)
        viewInflated.labelAddAddressAddress.text = send_coins_receiving_static_address.text
        val input = viewInflated.findViewById(R.id.input) as EditText
        builder.setView(viewInflated)
        builder.setPositiveButton(android.R.string.ok) { dialog, _ ->
            dialog.dismiss()
            val label = input.text.toString()
            val record = AddressRecord(send_coins_receiving_static_address.text.toString(), "Send", label)
            GuldenUnifiedBackend.addAddressBookRecord(record)
            setAddressLabel(label)
        }
        builder.setNegativeButton(android.R.string.cancel) { dialog, _ -> dialog.cancel() }
        builder.show()
    }

    fun handleRemoveFromAddressBookClick(view : View)
    {
        val record = AddressRecord(send_coins_receiving_static_address.text.toString(), "Send", send_coins_receiving_static_label.text.toString())
        GuldenUnifiedBackend.deleteAddressBookRecord(record)
        setAddressLabel("")
    }

    companion object
    {
        val EXTRA_RECIPIENT = "recipient"
    }
}
