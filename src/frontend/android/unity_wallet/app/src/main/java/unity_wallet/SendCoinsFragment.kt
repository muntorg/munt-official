// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

package unity_wallet

import android.app.Activity
import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.text.SpannableString
import android.text.style.ImageSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.InputMethodManager
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.constraintlayout.widget.ConstraintLayout
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.android.synthetic.main.fragment_send_coins.*
import unity_wallet.jniunifiedbackend.AddressRecord
import unity_wallet.jniunifiedbackend.ILibraryController
import unity_wallet.jniunifiedbackend.UriRecipient
import unity_wallet.Config.Companion.PRECISION_SHORT
import unity_wallet.R.layout.text_input_address_label
import unity_wallet.ui.getDisplayDimensions
import unity_wallet.util.invokeNowOrOnSuccessfulCompletion
import kotlinx.android.synthetic.main.fragment_send_coins.view.*
import kotlinx.android.synthetic.main.text_input_address_label.view.*
import kotlinx.coroutines.*
import kotlin.coroutines.CoroutineContext


class SendCoinsFragment : BottomSheetDialogFragment(), CoroutineScope
{
    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    private var localRate: Double = 0.0
    private lateinit var recipient: UriRecipient
    private var foreignCurrency = localCurrency
    private enum class EntryMode {
        Native,
        Local
    }
    private var entryMode = EntryMode.Native
        set(value)
        {
            field = value
            var nativeIcon = SpannableString(" ")
            nativeIcon.setSpan(ImageSpan(requireContext(), R.drawable.ic_logo_white), 0, 1, 0)
            when (entryMode)
            {
                EntryMode.Local -> mMainlayout.findViewById<TextView>(R.id.button_currency).text = nativeIcon;
                EntryMode.Native -> mMainlayout.findViewById<TextView>(R.id.button_currency).text = foreignCurrency.short
            }
        }
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
        const val EXTRA_FINISH_ACTIVITY_ON_CLOSE = "finish_on_close"
        fun newInstance(recipient: UriRecipient, finishActivityOnClose : Boolean) = SendCoinsFragment().apply {
            arguments = Bundle().apply {
                putParcelable(EXTRA_RECIPIENT, recipient)
                putBoolean(EXTRA_FINISH_ACTIVITY_ON_CLOSE, finishActivityOnClose)
            }
        }
    }

    private lateinit var fragmentActivity : Activity

    private var mBehavior: BottomSheetBehavior<*>? = null
    private lateinit var mSendCoinsReceivingStaticAddress : TextView
    private lateinit var mSendCoinsReceivingStaticLabel : TextView
    private lateinit var mLabelRemoveFromAddressBook : TextView
    private lateinit var mLabelAddToAddressBook : TextView
    private lateinit var mMainlayout : View

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog
    {
        val dialog = super.onCreateDialog(savedInstanceState) as BottomSheetDialog
        mMainlayout = View.inflate(context, R.layout.fragment_send_coins, null)

        // test layout using display dimens
        val outSize = getDisplayDimensions(requireContext())
        mMainlayout.measure(outSize.x, outSize.y)

        // height that the entire bottom sheet wants to be
        val preferredHeight = mMainlayout.measuredHeight

        // maximum height that we allow it to be, to be sure that the balance shows through (if not hidden)
        val allowedHeight = outSize.y - requireContext().resources.getDimensionPixelSize(R.dimen.top_space_send_coins)

        // programmatically adjust layout, only if needed
        if (preferredHeight > allowedHeight) {
            // for the preferredHeight numeric_keypad is square due to the ratio constraint, squeeze it to fit allowed height
            val newHeight = outSize.x - (preferredHeight - allowedHeight)

            // setup new layout params, stripping ratio constaint and adding newly calculatied height
            val params = mMainlayout.numeric_keypad_holder.layoutParams as ConstraintLayout.LayoutParams
            params.dimensionRatio = null
            params.height = newHeight
            params.matchConstraintMaxHeight = newHeight

            // apply new layout
            mMainlayout.numeric_keypad_holder.layoutParams = params
        }

        mSendCoinsReceivingStaticAddress = mMainlayout.findViewById(R.id.send_coins_receiving_static_address)
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
        mMainlayout.findViewById<View>(R.id.button_backspace).setOnLongClickListener { view -> handleKeypadButtonLongClick(view) }
        mMainlayout.findViewById<View>(R.id.labelAddToAddressBook).setOnClickListener { view -> handleAddToAddressBookClick(view) }
        mMainlayout.findViewById<View>(R.id.labelRemoveFromAddressBook).setOnClickListener { view -> handleRemoveFromAddressBookClick(view) }


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


        fragmentActivity = super.requireActivity()

        arguments?.getParcelable<UriRecipient>(EXTRA_RECIPIENT)?.let {
            recipient = it
        }

        if (recipient.amount != 0L)
            amountEditStr = formatNativeSimple(recipient.amount)
        updateDisplayAmount()

        mSendCoinsReceivingStaticAddress.text = recipient.address

        setAddressLabel(recipient.label)

        foreignCurrency = localCurrency

        setupRate()

        return view
    }

    private fun updateDisplayAmount() {
        var primaryStr = ""
        var secondaryStr = ""
        val amount = amountEditStr.toDoubleOrZero()
        val primaryTextView = (mMainlayout.findViewById<View>(R.id.send_coins_amount_primary) as TextView?)
        val secondaryTextView = (mMainlayout.findViewById<View>(R.id.send_coins_amount_secondary) as TextView?)
        when (entryMode) {
            EntryMode.Native -> {
                primaryStr = "%s".format(amountEditStr)
                if (localRate > 0.0) {
                    secondaryStr = String.format("(%s %.${foreignCurrency.precision}f)", foreignCurrency.short, localRate * amount)
                }
            }
            EntryMode.Local -> {
                primaryStr = "%s %s".format(foreignCurrency.short, amountEditStr)
                if (localRate > 0.0) {
                    secondaryStr = String.format("(%.${PRECISION_SHORT}f)", amount / localRate)
                }
            }
        }
        primaryTextView?.text = primaryStr
        secondaryTextView?.text = secondaryStr
    }

    private fun performAuthenticatedPayment(d : Dialog, request : UriRecipient, msg: String?, subtractFee: Boolean = false)
    {
        val amountStr = String.format("%.${PRECISION_SHORT}f", request.amount.toDouble() / 100000000)
        val message = msg ?: getString(R.string.send_coins_confirm_template, amountStr, recipientDisplayAddress)
        Authentication.instance.authenticate(this@SendCoinsFragment.requireActivity(),
                null, msg = message) {
            try {
                ILibraryController.performPaymentToRecipient(request, subtractFee)
                d.dismiss()
            }
            catch (exception: RuntimeException) {
                errorMessage(exception.message!!)
            }
        }
    }

    private fun confirmAndCommitCoinPayment()
    {
        val amountNativeStr = when (entryMode) {
            EntryMode.Local -> (amountEditStr.toDoubleOrZero() / localRate).toString()
            EntryMode.Native -> amountEditStr
        }.toDoubleOrZero()

        val amountNative = amountNativeStr.toNative()

        try {
            val paymentRequest = UriRecipient(true, recipient.address, recipient.label, recipient.desc, amountNative)
            val fee = ILibraryController.feeForRecipient(paymentRequest)
            val balance = ILibraryController.GetBalance()
            if (fee > balance) {
                errorMessage(getString(R.string.send_insufficient_balance))
                return
            }
            if (fee + amountNative > balance) {
                // alert dialog for confirmation of payment and reduction of amount since amount + fee exceeds balance
                MaterialAlertDialogBuilder(requireContext())
                        .setTitle(getString(R.string.send_all_instead_title))
                        .setMessage(getString(R.string.send_all_instead_msg))
                        // on confirmation compose recipient with reduced amount and execute payment with subtractFee fee from amount
                        .setPositiveButton(getString(R.string.send_all_btn)) { dialogInterface: DialogInterface, i: Int ->
                            val sendAllRequest = UriRecipient(true, recipient.address, recipient.label, recipient.desc, balance)
                            performAuthenticatedPayment(dialog!!, sendAllRequest, null,true)
                        }
                        .setNegativeButton(getString(R.string.cancel_btn)) { dialogInterface: DialogInterface, i: Int -> }
                        .show()
            } else {
                // create styled message from resource template and arguments bundle
                val amountStr = String.format("%.${PRECISION_SHORT}f", amountNativeStr)
                val message = getString(R.string.send_coins_confirm_template, amountStr, recipientDisplayAddress)

                // alert dialog for confirmation
                MaterialAlertDialogBuilder(requireContext())
                        .setTitle(getString(R.string.send_coins_title))
                        .setMessage(message)
                        // on confirmation compose recipient and execute payment
                        .setPositiveButton(getString(R.string.send_btn)) { dialogInterface: DialogInterface, i: Int ->
                            performAuthenticatedPayment(dialog!!, paymentRequest, null,false)
                        }
                        .setNegativeButton(getString(R.string.cancel_btn)) { dialogInterface: DialogInterface, i: Int -> }
                        .show()
            }
        } catch (exception: RuntimeException) {
            errorMessage(exception.message!!)
        }
    }

    private fun errorMessage(msg: String) {
        MaterialAlertDialogBuilder(requireContext())
                .setTitle(getString(R.string.send_coins_title))
                .setMessage(msg)
                // on confirmation compose recipient and execute payment
                .setPositiveButton(getString(R.string.send_coins_error_acknowledge)) { dialogInterface: DialogInterface, i: Int ->
                }
                .show()
    }

    override fun onDestroy() {
        super.onDestroy()

        coroutineContext[Job]!!.cancel()

        // Let the invisible URI activity know to close itself
        arguments?.getBoolean(EXTRA_FINISH_ACTIVITY_ON_CLOSE)?.let {
            if (it) {
                activity?.moveTaskToBack(true)
                activity?.finish()
            }
        }
    }

    private fun setupRate()
    {
        entryMode = EntryMode.Native

        this.launch( Dispatchers.Main) {
            try {
                localRate = fetchCurrencyRate(foreignCurrency.code)
            }
            catch (e: Throwable) {
                entryMode = EntryMode.Native
            }
            updateDisplayAmount()
        }
    }

    private fun setAddressLabel(label : String)
    {
        mSendCoinsReceivingStaticLabel.text = label

        if (label.isNotEmpty()) {
            mSendCoinsReceivingStaticLabel.visibility = View.VISIBLE
            mLabelRemoveFromAddressBook.visibility = View.INVISIBLE // use for layout already, when wallet is ready will become visible (or will be switched to add to address book)
            mLabelAddToAddressBook.visibility = View.GONE
        }
        else {
            mSendCoinsReceivingStaticLabel.visibility = View.GONE
            mLabelRemoveFromAddressBook.visibility = View.GONE
            mLabelAddToAddressBook.visibility = View.INVISIBLE // use for layout already, will become visible when wallet is ready
        }

        UnityCore.instance.walletReady.invokeNowOrOnSuccessfulCompletion(this) {
            if (label.isNotEmpty())
            {
                val isInAddressBook = ILibraryController.getAddressBookRecords().count { it.name.equals(other = label, ignoreCase = true) } > 0
                if (isInAddressBook) {
                    mLabelRemoveFromAddressBook.visibility = View.VISIBLE
                    mLabelAddToAddressBook.visibility = View.GONE
                }
                else {
                    mLabelRemoveFromAddressBook.visibility = View.GONE
                    mLabelAddToAddressBook.visibility = View.VISIBLE
                }
            }
            else
            {
                mLabelRemoveFromAddressBook.visibility = View.GONE
                mLabelAddToAddressBook.visibility = View.VISIBLE
            }
        }
    }

    private fun allowedDecimals(): Int {
        return when(entryMode) {
            EntryMode.Native -> PRECISION_SHORT
            EntryMode.Local -> foreignCurrency.precision
        }
    }

    private fun numFractionalDigits(amount:String): Int {
        val pos = amount.indexOfLast { it == '.' }
        return if (pos > 0) amount.length - pos - 1 else 0
    }

    private fun numWholeDigits(amount:String): Int {
        val pos = amount.indexOfFirst { it == '.' }
        return if (pos > 0) pos else amount.length
    }

    private fun chopExcessDecimals() {
        if (numFractionalDigits(amountEditStr) > allowedDecimals()) {
            amountEditStr = amountEditStr.substring(0, amountEditStr.length - (numFractionalDigits(amountEditStr) - allowedDecimals()))
        }
    }

    private fun appendNumberToAmount(number : String) {
        if (amountEditStr == "0")
        {
            amountEditStr = number
        }
        else
        {
            if (!amountEditStr.contains("."))
            {
                if (numWholeDigits(amountEditStr) < 8)
                {
                    amountEditStr += number
                }
            }
            else if (numFractionalDigits(amountEditStr) < allowedDecimals())
            {
                amountEditStr += number
            }
            else
            {
                if (numFractionalDigits(amountEditStr) < allowedDecimals())
                {
                    amountEditStr += number
                }
                else
                {
                    if (numWholeDigits(amountEditStr) < 8)
                    {
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
    }

    private fun handleKeypadButtonLongClick(view : View) : Boolean
    {
        when (view.id)
        {
            R.id.button_backspace ->
            {
                amountEditStr = "0"
            }
        }
        return true
    }

    private fun handleSendButton()
    {
        if (!UnityCore.instance.walletReady.isCompleted) {
            errorMessage(getString(R.string.core_not_ready_yet))
            return
        }

        run {
            if ((amountEditStr.isEmpty()) || (foreignAmount<=0 && amount<=0))
            {
                errorMessage("Enter an amount to pay")
                return@run
            }

            confirmAndCommitCoinPayment()
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
                else if (amountEditStr != "0" && amountEditStr != "0.00") appendNumberToAmount("0")
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
                    amountEditStr = if (amountEditStr.isEmpty()) "0."
                    else "$amountEditStr."
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
                handleSendButton()
            }

        }
    }

    private fun handleAddToAddressBookClick(view : View)
    {
        if (!UnityCore.instance.walletReady.isCompleted) {
            errorMessage(getString(R.string.core_not_ready_yet))
            return
        }

        val builder = AlertDialog.Builder(fragmentActivity)
        builder.setTitle(getString(R.string.dialog_title_add_address))
        val layoutInflater : LayoutInflater = fragmentActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater
        val viewInflated : View = layoutInflater.inflate(text_input_address_label, view.rootView as ViewGroup, false)
        viewInflated.labelAddAddressAddress.text = mSendCoinsReceivingStaticAddress.text
        val input = viewInflated.findViewById(R.id.addAddressInput) as EditText
        if (recipient.label.isNotEmpty()) {
            input.setText(recipient.label)
        }
        builder.setView(viewInflated)
        builder.setPositiveButton(android.R.string.ok) { dialog, _ ->
            dialog.dismiss()
            val label = input.text.toString()
            val record = AddressRecord(mSendCoinsReceivingStaticAddress.text.toString(), label, "", "Send")
            UnityCore.instance.addAddressBookRecord(record)
            setAddressLabel(label)
        }
        builder.setNegativeButton(android.R.string.cancel) { dialog, _ -> dialog.cancel() }
        val dialog = builder.create()
        dialog.setOnShowListener {
            viewInflated.addAddressInput.requestFocus()
            viewInflated.addAddressInput.post {
                val imm = fragmentActivity.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                imm.showSoftInput(viewInflated.addAddressInput, InputMethodManager.SHOW_IMPLICIT)
            }
        }
        dialog.show()
    }

    @Suppress("UNUSED_PARAMETER")
    fun handleRemoveFromAddressBookClick(view : View)
    {
        if (!UnityCore.instance.walletReady.isCompleted) {
            errorMessage(getString(R.string.core_not_ready_yet))
            return
        }

        val record = AddressRecord(mSendCoinsReceivingStaticAddress.text.toString(), mSendCoinsReceivingStaticLabel.text.toString(), "", "Send")
        UnityCore.instance.deleteAddressBookRecord(record)
        setAddressLabel("")
    }
}
