// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceManager
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.vision.barcode.Barcode
import com.google.android.material.bottomnavigation.BottomNavigationView
import com.gulden.barcodereader.BarcodeCaptureActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.MainActivityFragments.ReceiveFragment
import com.gulden.unity_wallet.MainActivityFragments.SendFragment
import com.gulden.unity_wallet.MainActivityFragments.SendFragment.OnFragmentInteractionListener
import com.gulden.unity_wallet.MainActivityFragments.SettingsFragment
import com.gulden.unity_wallet.MainActivityFragments.TransactionFragment
import com.gulden.unity_wallet.currency.fetchCurrencyRate
import com.gulden.unity_wallet.currency.localCurrency
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.coroutines.*
import kotlin.coroutines.CoroutineContext

inline fun androidx.fragment.app.FragmentManager.inTransaction(func: androidx.fragment.app.FragmentTransaction.() -> androidx.fragment.app.FragmentTransaction) {
    beginTransaction().func().commit()
}

fun AppCompatActivity.addFragment(fragment: androidx.fragment.app.Fragment, frameId: Int){
    supportFragmentManager.inTransaction { add(frameId, fragment) }
}

fun AppCompatActivity.replaceFragment(fragment: Any, frameId: Int) {
    supportFragmentManager.inTransaction{replace(frameId, fragment as androidx.fragment.app.Fragment)}
}

class WalletActivity : UnityCore.Observer, AppCompatActivity(), OnFragmentInteractionListener,
        ReceiveFragment.OnFragmentInteractionListener, TransactionFragment.OnFragmentInteractionListener,
        SettingsFragment.OnFragmentInteractionListener, CoroutineScope,
        SharedPreferences.OnSharedPreferenceChangeListener
{
    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    private val coreObserverProxy = CoreObserverProxy(this, this)

    override fun syncProgressChanged(percent: Float): Boolean {
        setSyncProgress(percent)
        return true
    }

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        navigation.setOnNavigationItemSelectedListener(mOnNavigationItemSelectedListener)

        syncProgress.max = 1000000

        if (sendFragment == null)
            sendFragment = SendFragment()
        addFragment(sendFragment!!, R.id.mainLayout)

        val preferences = PreferenceManager.getDefaultSharedPreferences(this)
        preferences.registerOnSharedPreferenceChangeListener(this)
    }

    override fun onDestroy() {
        super.onDestroy()

        coroutineContext[Job]!!.cancel()

        val preferences = PreferenceManager.getDefaultSharedPreferences(this)
        preferences.unregisterOnSharedPreferenceChangeListener(this)
    }

    override fun onStart() {
        super.onStart()

        syncProgress.progress = 0
        UnityCore.instance.addObserver(coreObserverProxy)

        setWalletBalance(UnityCore.instance.balanceAmount)

    }

    override fun onStop() {
        super.onStop()

        UnityCore.instance.removeObserver(coreObserverProxy)
    }

    override fun onFragmentInteraction(uri: Uri)
    {

    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        when (key) {
            "preference_local_currency" -> {
                setWalletBalance(UnityCore.instance.balanceAmount)
            }
        }
    }

    private var sendFragment : SendFragment ?= null
    private var receiveFragment : ReceiveFragment ?= null
    private var transactionFragment : TransactionFragment ?= null
    private var settingsFragment : SettingsFragment ?= null

    private val mOnNavigationItemSelectedListener = BottomNavigationView.OnNavigationItemSelectedListener { item ->
         when (item.itemId) {
            R.id.navigation_send -> {
                if (sendFragment == null)
                    sendFragment = SendFragment()
                replaceFragment(sendFragment!!, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_receive -> {
                if (receiveFragment == null)
                    receiveFragment = ReceiveFragment()
                replaceFragment(receiveFragment!!, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_transactions -> {
                if (transactionFragment == null)
                    transactionFragment = TransactionFragment()
                replaceFragment(transactionFragment!!, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_settings -> {
                if (settingsFragment == null)
                    settingsFragment = SettingsFragment()
                replaceFragment(settingsFragment!!, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
        }
        false
    }


    fun setSyncProgress(percent: Float)
    {
        syncProgress.progress = (syncProgress.max * (percent/100)).toInt()
        if (percent < 100.0)
            syncProgress.visibility = View.VISIBLE
        else
            syncProgress.visibility = View.INVISIBLE
    }

    fun setWalletBalance(balance : Long)
    {
        val coins = balance.toDouble() / Config.COIN
        walletBalance.text = String.format("%.2f", coins)
        walletBalanceLogo.visibility = View.VISIBLE
        walletBalance.visibility = View.VISIBLE

        this.launch( Dispatchers.Main) {
            try {
                val rate = fetchCurrencyRate(localCurrency.code)
                walletBalanceLocal.text = String.format(" (${localCurrency.short} %.${localCurrency.precision}f)",
                        coins * rate)
                walletBalanceLocal.visibility = View.VISIBLE
            }
            catch (e: Throwable) {
                walletBalanceLocal.text = ""
                walletBalanceLocal.visibility = View.GONE
            }
        }
    }

    override fun walletBalanceChanged(balance: Long): Boolean {
        setWalletBalance(balance)
        return true
    }

    fun handleQRScanButtonClick(view : View) {
        val intent = Intent(applicationContext, BarcodeCaptureActivity::class.java)
        startActivityForResult(intent, BARCODE_READER_REQUEST_CODE)
    }

    fun gotoBuyActivity(_view : View)
    {
        gotoBuyActivity()
    }

    fun gotoBuyActivity()
    {
        val urlBuilder = Uri.Builder()
        urlBuilder.scheme("https")
        urlBuilder.path("gulden.com/purchase")
        urlBuilder.appendQueryParameter("receive_address", GuldenUnifiedBackend.GetReceiveAddress().toString())
        val intent = Intent(Intent.ACTION_VIEW, urlBuilder.build())
        if (intent.resolveActivity(packageManager) != null)
        {
            startActivity(intent)
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == BARCODE_READER_REQUEST_CODE) {
            if (resultCode == CommonStatusCodes.SUCCESS)
            {
                if (data != null) {
                    val barcode = data.getParcelableExtra<Barcode>(BarcodeCaptureActivity.BarcodeObject)
                    val recipient = uriRecicpient(barcode.displayValue)
                    if (recipient.valid) {
                        val intent = Intent(applicationContext, SendCoinsActivity::class.java)
                        intent.putExtra(SendCoinsActivity.EXTRA_RECIPIENT, recipient)
                        startActivityForResult(intent, SEND_COINS_RETURN_CODE)
                    }
                }
            }
        } else {
            super.onActivityResult(requestCode, resultCode, data)
        }
    }

    fun setFocusOnAddress(view : View)
    {
        receiveFragment?.setFocusOnAddress()
    }

    companion object {
        private val BARCODE_READER_REQUEST_CODE = 1
        val SEND_COINS_RETURN_CODE = 2
        val BUY_RETURN_CODE = 3
    }
}
