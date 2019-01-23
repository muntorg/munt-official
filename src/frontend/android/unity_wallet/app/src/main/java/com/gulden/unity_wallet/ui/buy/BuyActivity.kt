// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.buy

import android.os.Bundle
import androidx.fragment.app.Fragment
import androidx.appcompat.app.AppCompatActivity
import android.view.MenuItem
import android.view.View
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.activity_transaction_info.*


class BuyActivity : AppCompatActivity()
{

    private var fragment: BuyFragment? = null

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_buy)

        // Begin the transaction
        val ft = supportFragmentManager.beginTransaction()
        // Replace the contents of the container with the new fragment
        fragment = BuyFragment.newInstance(intent.extras!!.getString(ARG_BUY_ADDRESS))
        ft.replace(R.id.buy_fragment_placeholder, fragment!! as androidx.fragment.app.Fragment)
        // Complete the changes added above
        ft.commit()
    }

    fun onErrorRefresh(view: View)
    {
        fragment?.LoadBuyPage()
    }

    fun onCancelPurchase(view: View)
    {
        //fixme: force close browser here just in case.
        fragment?.clearURL()
        finish()
    }

    fun onAcceptPurchase(view: View)
    {
        fragment?.acceptPurchase()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean
    {
        when (item.itemId)
        {
            android.R.id.home ->
            {
                finish()
                return true
            }
        }

        return super.onOptionsItemSelected(item)
    }

    companion object
    {
        val ARG_BUY_ADDRESS = "buy_address"
    }
}
