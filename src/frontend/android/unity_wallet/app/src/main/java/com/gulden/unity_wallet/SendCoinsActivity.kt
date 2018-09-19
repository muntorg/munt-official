package com.gulden.unity_wallet

import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v7.app.AppCompatActivity

import kotlinx.android.synthetic.main.activity_send_coins.*

class SendCoinsActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_send_coins)
        setSupportActionBar(toolbar)

        send_coins_receiving_static_address.text = intent.getStringExtra(EXTRA_RECIPIENT_ADDRESS)
        send_coins_receiving_static_label.text = intent.getStringExtra(EXTRA_RECIPIENT_LABEL)

        fab.setOnClickListener { view ->
            Snackbar.make(view, "Replace with your own action", Snackbar.LENGTH_LONG)
                    .setAction("Action", null).show()
        }
    }

    companion object {
        public val EXTRA_RECIPIENT_ADDRESS = "address"
        public val EXTRA_RECIPIENT_LABEL = "label"
        public val EXTRA_RECIPIENT_AMOUNT = "amount"
    }
}
