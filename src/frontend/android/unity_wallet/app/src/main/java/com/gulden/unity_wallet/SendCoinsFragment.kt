// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.app.Activity
import android.app.Dialog
import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.gulden.jniunifiedbackend.AddressRecord
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.UriRecipient
import com.gulden.unity_wallet.Config.Companion.PRECISION_SHORT
import com.gulden.unity_wallet.R.layout.text_input_address_label
import kotlinx.android.synthetic.main.numeric_keypad.view.*
import kotlinx.android.synthetic.main.text_input_address_label.view.*
import kotlinx.coroutines.*
import org.apache.commons.validator.routines.IBANValidator
import org.jetbrains.anko.alert
import org.jetbrains.anko.appcompat.v7.Appcompat
import kotlin.coroutines.CoroutineContext


class SendCoinsFragment : BottomSheetDialogFragment(), CoroutineScope
{
    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    private var nocksJob: Job? = null
    private var orderResult: NocksOrderResult? = null
    private var localRate: Double = 0.0
    private lateinit var recipient: UriRecipient
    private var foreignCurrency = localCurrency
    private var isIBAN = false
    private enum class EntryMode {
        Native,
        Local
    }
    private var entryMode = EntryMode.Native
    private var amountEditStr: String = "0"
        set(value) {
            field = value
            updateDisplayAmount()
        }

    private val amount: Double
        get() = amountEditStr.toDoubleOrZero()

    private val foreignAmount: Double
        get() {
            return when (entryMode) {
                EntryMode.Local -> amountEditStr.toDoubleOrZero()
                EntryMode.Native -> amountEditStr.toDoubleOrZero() * localRate
            }
        }

    private val recipientDisplayAddress: String
        get () {
            return if (recipient.label.isEmpty()) recipient.address else "${recipient.label} (${recipient.address})"
        }


    companion object {
        const val EXTRA_RECIPIENT = "recipient"
        fun newInstance(recipient: UriRecipient) = SendCoinsFragment().apply {
            arguments = Bundle().apply {
                putParcelable(EXTRA_RECIPIENT, recipient)
            }
        }
    }

    private lateinit var fragmentActivity : Activity

    private var mBehavior: BottomSheetBehavior<*>? = null
    private lateinit var mSendCoinsReceivingStaticAddress : TextView
    private lateinit var mSendCoinsNocksEstimate : TextView
    private lateinit var mSendCoinsReceivingStaticLabel : TextView
    private lateinit var mLabelRemoveFromAddressBook : TextView
    private lateinit var mLabelAddToAddressBook : TextView
    private lateinit var mMainlayout : View

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog
    {
        val dialog = super.onCreateDialog(savedInstanceState) as BottomSheetDialog
        mMainlayout = View.inflate(context, R.layout.fragment_send_coins, null)

        mSendCoinsReceivingStaticAddress = mMainlayout.findViewById(R.id.send_coins_receiving_static_address)
        mSendCoinsNocksEstimate = mMainlayout.findViewById(R.id.send_coins_nocks_estimate)
        mSendCoinsReceivingStaticLabel = mMainlayout.findViewById(R.id.send_coins_receiving_static_label)
        mLabelRemoveFromAddressBook = mMainlayout.findViewById(R.id.labelRemoveFromAddressBook)
        mLabelAddToAddressBook = mMainlayout.findViewById(R.id.labelAddToAddressBook)
        mSendCoinsReceivingStaticAddress = mMainlayout.findViewById(R.id.send_coins_receiving_static_address)

        mMainlayout.findViewById<View>(R.id.button_0).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_1).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_2).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_3).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_4).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_5).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_6).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_7).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_8).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_9).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_send).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_currency).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_decimal).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.button_backspace).setOnClickListener { view -> handleKeypadButtonClick(view) }
        mMainlayout.findViewById<View>(R.id.labelAddToAddressBook).setOnClickListener { view -> handleAddToAddressBookClick(view) }
        mMainlayout.findViewById<View>(R.id.labelRemoveFromAddressBook).setOnClickListener { view -> handleRemoveFromAddressBookClick(view) }


        mMainlayout.findViewById<Button>(R.id.button_currency).text = foreignCurrency.short

        dialog.setContentView(mMainlayout)

        mBehavior = BottomSheetBehavior.from(mMainlayout.parent as View)

        return dialog
    }

    override fun onStart()
    {
        super.onStart()

        mBehavior!!.skipCollapsed = true
        mBehavior!!.state = BottomSheetBehavior.STATE_EXPANDED
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        val view = super.onCreateView(inflater, container, savedInstanceState)


        fragmentActivity = super.getActivity()!!

        arguments?.getParcelable<UriRecipient>(EXTRA_RECIPIENT)?.let {
            recipient = it
        }

        if (recipient.amount.isNotEmpty())
            amountEditStr = recipient.amount
        updateDisplayAmount()

        mSendCoinsReceivingStaticAddress.text = recipient.address

        setAddressLabel(recipient.label)

        if (IBANValidator.getInstance().isValid(recipient.address)) {
            foreignCurrency = Currencies.knownCurrencies["EUR"]!!
            isIBAN = true
        }
        else {
            foreignCurrency = localCurrency
            isIBAN = false
        }

        setupRate()

        return view
    }

    private fun updateDisplayAmount() {
        var primaryStr = ""
        var secondaryStr = ""
        val amount = amountEditStr.toDoubleOrZero()
        when (entryMode) {
            EntryMode.Native -> {
                primaryStr = "G %s".format(amountEditStr)
                if (localRate > 0.0) {
                    secondaryStr = String.format("(%s %.${foreignCurrency.precision}f)", foreignCurrency.short, localRate * amount)
                }


            }
            EntryMode.Local -> {
                primaryStr = "%s %s".format(foreignCurrency.short, amountEditStr)
                if (localRate > 0.0) {
                    secondaryStr = String.format("(G %.${Config.PRECISION_SHORT}f)", amount / localRate)
                }
            }
        }

        (mMainlayout.findViewById<View>(R.id.send_coins_amount_primary) as TextView?)?.text = primaryStr
        (mMainlayout.findViewById<View>(R.id.send_coins_amount_secondary) as TextView?)?.text = secondaryStr
    }

    private fun performAuthenticatedPayment(d : Dialog, request : UriRecipient, message : String)
    {
        Authentication.instance.authenticate(this@SendCoinsFragment.activity!!,
                null, msg = message) {
            try {
                GuldenUnifiedBackend.performPaymentToRecipient(request)
                d.dismiss()
            }
            catch (exception: RuntimeException) {
                errorMessage(exception.message!!)
            }
        }
    }

    private fun confirmAndCommitGuldenPayment(view: View) {
        // create styled message from resource template and arguments bundle
        val nlgStr = String.format("%.${Config.PRECISION_SHORT}f", amount)
        val message = getString(R.string.send_coins_confirm_template, nlgStr, recipientDisplayAddress)

        // alert dialog for confirmation
        fragmentActivity.alert(Appcompat, message, "Send Gulden?") {

            // on confirmation compose recipient and execute payment
            positiveButton("Send") {
                val paymentRequest = UriRecipient(true, recipient.address, recipient.label, amountEditStr)
                performAuthenticatedPayment(dialog!!, paymentRequest, "%s\n\nG %s".format(paymentRequest.address, message))
            }

            negativeButton("Cancel") {}
        }.show()
    }

    private fun confirmAndCommitIBANPayment(view: View) {
        mMainlayout.button_send.isEnabled = false
        this.launch {
            try {
                // request order from Nocks
                val orderResult = nocksOrder(
                        amountEuro = foreignAmount,
                        destinationIBAN = recipient.address)

                // create styled message from resource template and arguments bundle
                val nlgStr = String.format("%.${Config.PRECISION_SHORT}f", orderResult.depositAmountNLG.toDouble())
                val message = getString(R.string.send_coins_iban_confirm_template,
                        String.format("%.${foreignCurrency.precision}f", foreignAmount),
                        nlgStr, recipientDisplayAddress)

                // alert dialog for confirmation
                fragmentActivity.alert(Appcompat, message, "Send Gulden to IBAN?") {

                    // on confirmation compose recipient and execute payment
                    positiveButton("Send") {
                        mMainlayout.button_send.isEnabled = true
                        val paymentRequest = UriRecipient(true, orderResult.depositAddress, recipient.label, wireFormatNative(orderResult.depositAmountNLG))
                        try {
                            performAuthenticatedPayment(dialog!!, paymentRequest, "%s\n\nG %s".format(paymentRequest.address, message))
                        }
                        catch (exception: RuntimeException) {
                            errorMessage(exception.message!!)
                        }

                    }

                    negativeButton("Cancel") {}
                }
                        .show()
                mMainlayout.button_send.isEnabled = true

            } catch (e: Throwable) {
                errorMessage("IBAN order failed")
                mMainlayout.button_send.isEnabled = true
            }
        }
    }

    private fun errorMessage(msg: String) {
        fragmentActivity.alert(Appcompat, msg, "") {
            positiveButton(getString(R.string.send_coins_error_acknowledge)) {}
        }.show()
    }

    override fun onDestroy() {
        super.onDestroy()

        coroutineContext[Job]!!.cancel()
    }

    private fun setupRate()
    {
        this.launch( Dispatchers.Main) {
            try {
                localRate = fetchCurrencyRate(foreignCurrency.code)

                if (isIBAN)
                    entryMode = EntryMode.Local
            }
            catch (e: Throwable) {
                entryMode = EntryMode.Native
            }
            updateDisplayAmount()
        }
    }

    private fun updateNocksEstimate() {
        nocksJob?.cancel()
        mSendCoinsNocksEstimate.text = " "
        if (isIBAN && foreignAmount != 0.0) {
            val prevJob = nocksJob
            nocksJob = this.launch(Dispatchers.Main) {
                try {
                    mSendCoinsNocksEstimate.text = "..."

                    // delay a bit so quick typing will make a limited number of requests
                    // (this job will be canceled by the next key typed
                    delay(700)

                    prevJob?.join()

                    val quote = nocksQuote(foreignAmount)
                    val nlg = String.format("%.${Config.PRECISION_SHORT}f", quote.amountNLG)
                    mSendCoinsNocksEstimate.text = getString(R.string.send_coins_nocks_estimate_template, nlg)
                }
                catch (_: CancellationException) {
                    // silently pass job cancellation
                }
                catch (e: Throwable) {
                    mSendCoinsNocksEstimate.text = "Could not fetch transaction quote"
                }
            }
        }
    }

    private fun setAddressLabel(label : String)
    {
        mSendCoinsReceivingStaticLabel.text = label
        setAddressHasLabel(label.isNotEmpty())
    }

    private fun setAddressHasLabel(hasLabel : Boolean)
    {
        if (hasLabel)
        {
            mSendCoinsReceivingStaticLabel.visibility = View.VISIBLE
            mLabelRemoveFromAddressBook.visibility = View.VISIBLE
            mLabelAddToAddressBook.visibility = View.GONE
        }
        else
        {
            mSendCoinsReceivingStaticLabel.visibility = View.GONE
            mLabelRemoveFromAddressBook.visibility = View.GONE
            mLabelAddToAddressBook.visibility = View.VISIBLE
        }
    }

    private fun allowedDecimals(): Int {
        return when(entryMode) {
            EntryMode.Native -> PRECISION_SHORT
            EntryMode.Local -> foreignCurrency.precision
        }
    }

    private fun numDecimals(amount:String): Int {
        val pos = amount.indexOfLast { it == '.' }
        return if (pos > 0) amount.length - pos - 1 else 0
    }

    private fun chopExcessDecimals() {
        if (numDecimals(amountEditStr) > allowedDecimals()) {
            amountEditStr = amountEditStr.substring(0, amountEditStr.length - (numDecimals(amountEditStr) - allowedDecimals()))
        }
    }

    private fun appendNumberToAmount(number : String) {
        if (amountEditStr == "0")
            amountEditStr = number
        else {
            if (!amountEditStr.contains(".") || numDecimals(amountEditStr) < allowedDecimals())
                amountEditStr += number
            else {
                if (numDecimals(amountEditStr) < allowedDecimals())
                    amountEditStr += number
                else {
                    amountEditStr = buildString {
                        append(amountEditStr)
                        deleteCharAt(lastIndexOf("."))
                        append(number)
                        insert(length - allowedDecimals(), ".")
                    }.trimStart('0')
                }
            }
        }
    }

    private fun handleKeypadButtonClick(view : View)
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
            R.id.button_0 ->
            {
                if (amountEditStr.isEmpty()) amountEditStr += "0."
                else if (amountEditStr != "0") amountEditStr += "0"
            }
            R.id.button_backspace ->
            {
                amountEditStr = if (amountEditStr.length > 1)
                    amountEditStr.dropLast(1)
                else
                    "0"
            }
            R.id.button_decimal ->
            {
                if (!amountEditStr.contains("."))
                {
                    if (amountEditStr.isEmpty()) amountEditStr = "0."
                    else amountEditStr = "$amountEditStr."
                }
            }
            R.id.button_currency ->
            {
                entryMode = when (entryMode) {
                    EntryMode.Local -> EntryMode.Native
                    EntryMode.Native -> EntryMode.Local
                }
                chopExcessDecimals()
                updateDisplayAmount()
            }
            R.id.button_send ->
            {
                run {
                    if (amountEditStr.isEmpty())
                    {
                        errorMessage("Enter an amount to pay")
                        return@run
                    }

                    when
                    {
                        isIBAN -> confirmAndCommitIBANPayment(view)
                        else -> confirmAndCommitGuldenPayment(view)
                    }
                }
            }

        }
        updateNocksEstimate()
    }

    private fun handleAddToAddressBookClick(view : View)
    {
        val builder = AlertDialog.Builder(fragmentActivity)
        builder.setTitle("Add address")
        val layoutInflater : LayoutInflater = fragmentActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater
        val viewInflated : View = layoutInflater.inflate(text_input_address_label, view.rootView as ViewGroup, false)
        viewInflated.labelAddAddressAddress.text = mSendCoinsReceivingStaticAddress.text
        val input = viewInflated.findViewById(R.id.input) as EditText
        builder.setView(viewInflated)
        builder.setPositiveButton(android.R.string.ok) { dialog, _ ->
            dialog.dismiss()
            val label = input.text.toString()
            val record = AddressRecord(mSendCoinsReceivingStaticAddress.text.toString(), "Send", label)
            UnityCore.instance.addAddressBookRecord(record)
            setAddressLabel(label)
        }
        builder.setNegativeButton(android.R.string.cancel) { dialog, _ -> dialog.cancel() }
        builder.show()
    }

    @Suppress("UNUSED_PARAMETER")
    fun handleRemoveFromAddressBookClick(view : View)
    {
        val record = AddressRecord(mSendCoinsReceivingStaticAddress.text.toString(), "Send", mSendCoinsReceivingStaticLabel.text.toString())
        UnityCore.instance.deleteAddressBookRecord(record)
        setAddressLabel("")
    }
}
