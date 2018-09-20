package com.gulden.unity_wallet

import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v7.app.AppCompatActivity
import android.view.View
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.UriRecipient
import com.gulden.jniunifiedbackend.UriRecord
import com.gulden.unity_wallet.R.id.send_coins_amount

import kotlinx.android.synthetic.main.activity_send_coins.*
import kotlinx.android.synthetic.main.numeric_keypad.*

class SendCoinsActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_send_coins)
        setSupportActionBar(toolbar)

        var recipient : UriRecipient = intent.getParcelableExtra(EXTRA_RECIPIENT);
        send_coins_amount.setText(recipient.amount);
        send_coins_receiving_static_address.text = recipient.address
        send_coins_receiving_static_label.text = recipient.label

        fab.setOnClickListener {
            view -> run {
                if (send_coins_amount.text.length > 0) {
                    var paymentRequest : UriRecipient = UriRecipient(true, recipient.address, recipient.label, send_coins_amount.text.toString())
                    if (GuldenUnifiedBackend.performPaymentToRecipient(paymentRequest)) {
                        finish()
                    }
                    else {
                        Snackbar.make(view, "Payment failed", Snackbar.LENGTH_LONG).setAction("Action", null).show()
                    }
                }
                else {
                    Snackbar.make(view, "Enter an amount to pay", Snackbar.LENGTH_LONG).setAction("Action", null).show()
                }
            }
        }
    }

    fun handleKeypadButtonClick(view : View) {
        when (view.id)
        {
            R.id.button_0 -> send_coins_amount.setText(send_coins_amount.text.toString() + "0");
            R.id.button_1 -> send_coins_amount.setText(send_coins_amount.text.toString() + "1");
            R.id.button_2 -> send_coins_amount.setText(send_coins_amount.text.toString() + "2");
            R.id.button_3 -> send_coins_amount.setText(send_coins_amount.text.toString() + "3");
            R.id.button_4 -> send_coins_amount.setText(send_coins_amount.text.toString() + "4");
            R.id.button_5 -> send_coins_amount.setText(send_coins_amount.text.toString() + "5");
            R.id.button_6 -> send_coins_amount.setText(send_coins_amount.text.toString() + "6");
            R.id.button_7 -> send_coins_amount.setText(send_coins_amount.text.toString() + "7");
            R.id.button_8 -> send_coins_amount.setText(send_coins_amount.text.toString() + "8");
            R.id.button_9 -> send_coins_amount.setText(send_coins_amount.text.toString() + "9");
            R.id.button_backspace -> send_coins_amount.text.dropLast(1);
            R.id.button_decimal -> {
                if (!send_coins_amount.text.contains(".")) {
                    send_coins_amount.setText(send_coins_amount.text.toString() + ".")
                }
            };
        }
    }

    companion object {
        public val EXTRA_RECIPIENT = "recipient"
    }
}
