// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.gulden.jniunifiedbackend.GuldenMonitorListener
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.TransactionRecord
import com.gulden.jniunifiedbackend.TransactionStatus
import kotlinx.android.synthetic.main.activity_transaction_info.*
import kotlinx.android.synthetic.main.content_transaction_info.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import org.jetbrains.anko.sdk27.coroutines.onClick
import java.text.DecimalFormat
import kotlin.coroutines.CoroutineContext

class TransactionInfoActivity : AppCompatActivity(), CoroutineScope {

    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_transaction_info)
        setSupportActionBar(toolbar)

        val txHash = intent.getStringExtra(EXTRA_TRANSACTION)
        try {
            val tx = GuldenUnifiedBackend.getTransaction(txHash)

            // transaction id
            transactionId.text = tx.txHash

            // view transaction in browser
            transactionId.onClick {
                val intent = Intent(Intent.ACTION_VIEW, Uri.withAppendedPath(Config.BLOCK_EXPLORER, "/tx/" + tx.txHash))
                startActivity(intent)
            }

            // status
            status.text = statusText(tx)

            // Amount instantly and update with rate conversion
            amount.text = formatAmount(tx.amount, 0.0)
            this.launch(Dispatchers.Main) {
                try {
                    amount.text = formatAmount(tx.amount, fetchCurrencyRate(localCurrency.code))
                } catch (e: Throwable) {
                    // silently ignore failure of getting rate here
                }
            }

            // To outpoints
            tx.sentOutputs.forEach { output ->
                val v = TextView(this)
                v.text = output.address
                from_container.addView(v)
            }

            // To outpoints
            tx.receivedOutputs.forEach { output ->
                val v = TextView(this)
                v.text = output.address
                to_container.addView(v)
            }
        }
        catch (e: Throwable)
        {
            transactionId.text = getString(R.string.no_tx_details).format(txHash)
        }
    }

    private fun formatAmount(amount: Long, rate: Double = 0.0): String {
        val native = "G " + (DecimalFormat("+#,##0.00;-#").format(amount.toDouble() / 100000000))

        val pattern = "+#,##0.%s;-#".format("0".repeat(localCurrency.precision))

        localCurrency.precision

        return when (rate) {
            0.0 -> native
            else -> {
                "%s (%s %s)".format(native, localCurrency.short, DecimalFormat(pattern).format(rate * amount.toDouble() / 100000000) )
            }
        }
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

        GuldenUnifiedBackend.RegisterMonitorListener(monitoringListener)
    }

    override fun onPause() {
        GuldenUnifiedBackend.UnregisterMonitorListener(monitoringListener)

        super.onPause()
    }

    private val monitoringListener = object: GuldenMonitorListener() {
        override fun onPruned(height: Int) {}

        override fun onProcessedSPVBlocks(height: Int) {}

        override fun onPartialChain(height: Int, probableHeight: Int, offset: Int) {
            runOnUiThread {
                val txHash = intent.getStringExtra(EXTRA_TRANSACTION)
                try {
                    val tx = GuldenUnifiedBackend.getTransaction(txHash)
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
        val EXTRA_TRANSACTION = "transaction"
    }
}
