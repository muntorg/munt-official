package com.gulden.unity_wallet

import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v7.app.AppCompatActivity

import kotlinx.android.synthetic.main.activity_transaction_info.*

class TransactionInfoActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_transaction_info)
        setSupportActionBar(toolbar)

    }

    companion object {
        public val EXTRA_TRANSACTION = "transaction"
    }
}
