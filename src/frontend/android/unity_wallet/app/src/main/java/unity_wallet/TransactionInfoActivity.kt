// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

package unity_wallet

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.View
import android.widget.TextView
import androidx.core.content.ContextCompat
import unity_wallet.jniunifiedbackend.MonitorListener
import unity_wallet.jniunifiedbackend.ILibraryController
import unity_wallet.jniunifiedbackend.TransactionRecord
import unity_wallet.jniunifiedbackend.TransactionStatus
import unity_wallet.util.AppBaseActivity
import kotlinx.android.synthetic.main.activity_transaction_info.*
import kotlinx.android.synthetic.main.content_transaction_info.*
import kotlinx.android.synthetic.main.transaction_info_item.view.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class TransactionInfoActivity : AppBaseActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_transaction_info)
        setSupportActionBar(toolbar)
    }

    override fun onWalletReady() {
        fillTxInfo()
    }

    private fun fillTxInfo() {
        val txHash = intent.getStringExtra(EXTRA_TRANSACTION)
        try {
            val tx = ILibraryController.getTransaction(txHash)

            // transaction id
            transactionId.text = tx.txHash

            // view transaction in browser
            transactionId.setOnClickListener {
                val intent = Intent(Intent.ACTION_VIEW, Uri.parse(Config.BLOCK_EXPLORER_TX_TEMPLATE.format(tx.txHash)))
                startActivity(intent)
            }

            // status
            status.text = statusText(tx)

            // Amount instantly
            amount.text = formatNativeAndLocal(tx.amount, 0.0)
            setAmountAndColor(amount, tx.amount, 0.0, true)

            // inputs
            tx.inputs.forEach { output ->
                val v = layoutInflater.inflate(R.layout.transaction_info_item, null)
                if (output.address.isNotEmpty())
                    v.address.text = output.address
                else {
                    v.address.text = getString(R.string.tx_detail_address_unavailable)
                    v.address.setTextAppearance(R.style.TxDetailMissing)
                }
                if (output.isMine)
                    v.subscript.text = getString(R.string.tx_detail_wallet_address)
                else {
                    if (output.label.isNotEmpty())
                        v.subscript.text = output.label
                    else
                        v.subscript.text = getString(R.string.tx_detail_sending_address)
                }
                from_container.addView(v)
            }

            // Update amount and display tx output details after getting rate (or failure)
            this.launch(Dispatchers.Main) {
                var rate = 0.0
                try {
                    rate = fetchCurrencyRate(localCurrency.code)
                } catch (e: Throwable) {
                    // silently ignore failure of getting rate here
                }

                this@TransactionInfoActivity.setAmountAndColor(amount, tx.amount, rate, true)

                // internal transfer if all inputs and outputs are mine
                val internalTransfer = (tx.inputs.size == tx.inputs.count { it.isMine }) && (tx.outputs.size == tx.outputs.count { it.isMine })

                // outputs
                val signedByMe = tx.fee > 0
                tx.outputs.forEach { output ->
                    if (output.isMine && !signedByMe || internalTransfer) {
                        val v = layoutInflater.inflate(R.layout.transaction_info_item, null)
                        v.address.text = output.address
                        v.subscript.text = getString(R.string.tx_detail_wallet_address)
                        v.amount.setTextColor( ContextCompat.getColor(this@TransactionInfoActivity, R.color.change_positive))
                        v.amount.text = formatNativeAndLocal(output.amount, rate, useNativePrefix = true, nativeFirst = false)
                        to_container.addView(v)
                    }
                    else if (!output.isMine && signedByMe) {
                        val v = layoutInflater.inflate(R.layout.transaction_info_item, null)
                        v.address.text = output.address
                        if (output.label.isNotEmpty())
                            v.subscript.text = output.label
                        else
                            v.subscript.text = getString(R.string.tx_detail_payment_address)
                        v.amount.setTextColor( ContextCompat.getColor(this@TransactionInfoActivity, R.color.change_negative))
                        v.amount.text = formatNativeAndLocal(-output.amount, rate, useNativePrefix = true, nativeFirst = false)
                        to_container.addView(v)
                    }
                    //else {
                        // output.isMine && signedByMe, this is likely change so don't display
                        // !output.isMine && !signedByMe, this is likely change for the other side or something else we don't care about
                    //}
                }

                // fee
                if (signedByMe) { // transaction created by me, show fee
                    val v = layoutInflater.inflate(R.layout.transaction_info_item, null)
                    v.address.visibility = View.GONE
                    v.subscript.text = getString(R.string.tx_detail_network_fee)
                    this@TransactionInfoActivity.setAmountAndColor(v.amount, -tx.fee, rate,false)
                    to_container.addView(v)
                }
            }
        }
        catch (e: Throwable)
        {
            transactionId.text = getString(R.string.no_tx_details).format(txHash)
        }
    }

    private fun setAmountAndColor(textView: TextView, nativeAmount: Long, rate: Double, nativeFirst: Boolean = true) {
        textView.setTextColor( ContextCompat.getColor(this,
                if (nativeAmount > 0)
                    R.color.change_positive
                else
                    R.color.change_negative))
        textView.text = formatNativeAndLocal(nativeAmount, rate, true, nativeFirst)
    }

    private fun statusText(tx: TransactionRecord): String {
        return when (tx.status) {
            TransactionStatus.ABANDONED -> getString(R.string.tx_status_abandoned)
            TransactionStatus.CONFLICTED -> getString(R.string.tx_status_conflicted)
            TransactionStatus.CONFIRMING -> getString(R.string.tx_status_confirming)
                    .format(tx.depth, Constants.RECOMMENDED_CONFIRMATIONS)
            TransactionStatus.UNCONFIRMED -> getString(R.string.tx_status_unconfirmed)
            TransactionStatus.CONFIRMED -> getString(R.string.tx_status_confirmed)
                    .format(tx.height, java.text.SimpleDateFormat("HH:mm").format(java.util.Date(tx.blockTime * 1000L)))
            else -> getString(R.string.tx_status_unknown)
        }
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressed()
        return true
    }

    override fun onResume() {
        super.onResume()

        ILibraryController.RegisterMonitorListener(monitoringListener)
    }

    override fun onPause() {
        ILibraryController.UnregisterMonitorListener(monitoringListener)

        super.onPause()
    }

    private val monitoringListener = object: MonitorListener() {
        override fun onPruned(height: Int) {}

        override fun onProcessedSPVBlocks(height: Int) {}

        override fun onPartialChain(height: Int, probableHeight: Int, offset: Int) {
            runOnUiThread {
                val txHash = intent.getStringExtra(EXTRA_TRANSACTION)
                try {
                    val tx = ILibraryController.getTransaction(txHash)
                    status.text = statusText(tx)
                }
                catch (e: Throwable)
                {
                    // silently ignore
                }
            }
        }
    }

    companion object {
        const val EXTRA_TRANSACTION = "transaction"
    }
}
