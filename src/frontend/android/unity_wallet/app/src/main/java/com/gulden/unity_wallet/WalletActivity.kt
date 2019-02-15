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
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.gulden.barcodereader.BarcodeCaptureActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.main_activity_fragments.*
import com.gulden.unity_wallet.main_activity_fragments.SendFragment.OnFragmentInteractionListener
import com.gulden.unity_wallet.util.AppBaseActivity
import com.gulden.uriRecipient
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch

inline fun androidx.fragment.app.FragmentManager.inTransaction(func: androidx.fragment.app.FragmentTransaction.() -> androidx.fragment.app.FragmentTransaction) {
    beginTransaction().func().commit()
}

fun AppCompatActivity.addFragment(fragment: androidx.fragment.app.Fragment, frameId: Int){
    supportFragmentManager.inTransaction { add(frameId, fragment) }
}

fun AppCompatActivity.replaceFragment(fragment: Any, frameId: Int) {
    supportFragmentManager.inTransaction{replace(frameId, fragment as androidx.fragment.app.Fragment)}
}

class WalletActivity : UnityCore.Observer, AppBaseActivity(), OnFragmentInteractionListener,
        ReceiveFragment.OnFragmentInteractionListener, MutationFragment.OnFragmentInteractionListener,
        SettingsFragment.OnFragmentInteractionListener, LocalCurrencyFragment.OnFragmentInteractionListener,
        SharedPreferences.OnSharedPreferenceChangeListener
{
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

        setSyncProgress(UnityCore.instance.progressPercent)
        UnityCore.instance.addObserver(this, fun (callback:() -> Unit) { runOnUiThread { callback() }})
        setWalletBalance(UnityCore.instance.balanceAmount)

    }

    override fun onStop() {
        super.onStop()

        UnityCore.instance.removeObserver(this)
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

    private fun gotoSendPage()
    {
        if (sendFragment == null)
            sendFragment = SendFragment()
        replaceFragment(sendFragment!!, R.id.mainLayout)
    }

    fun gotoReceivePage()
    {
        if (receiveFragment == null)
            receiveFragment = ReceiveFragment()
        replaceFragment(receiveFragment!!, R.id.mainLayout)
    }

    private fun gotoTransactionPage()
    {
        if (transactionFragment == null)
            transactionFragment = MutationFragment()
        replaceFragment(transactionFragment!!, R.id.mainLayout)
    }

    private fun gotoSettingsPage()
    {
        if (settingsFragment == null)
            settingsFragment = SettingsFragment()
        replaceFragment(settingsFragment!!, R.id.mainLayout)
    }

    fun showLocalCurrenciesPage()
    {
        if (localCurrencies == null)
            localCurrencies = LocalCurrencyFragment()
        replaceFragment(localCurrencies!!, R.id.mainLayout)
    }

    private var sendFragment : SendFragment ?= null
    private var receiveFragment : ReceiveFragment ?= null
    private var transactionFragment : MutationFragment ?= null
    private var settingsFragment : SettingsFragment ?= null
    private var localCurrencies : LocalCurrencyFragment ?= null

    private val mOnNavigationItemSelectedListener = BottomNavigationView.OnNavigationItemSelectedListener { item ->
         when (item.itemId) {
            R.id.navigation_send -> { gotoSendPage(); return@OnNavigationItemSelectedListener true }
            R.id.navigation_receive -> { gotoReceivePage(); return@OnNavigationItemSelectedListener true }
            R.id.navigation_transactions -> { gotoTransactionPage(); return@OnNavigationItemSelectedListener true }
            R.id.navigation_settings -> { gotoSettingsPage(); return@OnNavigationItemSelectedListener true }
        }
        false
    }


    private fun setSyncProgress(percent: Float)
    {
        syncProgress.progress = (syncProgress.max * (percent/100)).toInt()
        if (percent < 100.0)
            syncProgress.visibility = View.VISIBLE
        else
            syncProgress.visibility = View.INVISIBLE
    }

    private fun setWalletBalance(balance : Long)
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

    @Suppress("UNUSED_PARAMETER")
    fun handleQRScanButtonClick(view : View? = null) {
        val intent = Intent(applicationContext, BarcodeCaptureActivity::class.java)
        startActivityForResult(intent, BARCODE_READER_REQUEST_CODE)
    }

    @Suppress("UNUSED_PARAMETER")
    fun gotoBuyActivity(view : View? = null)
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
                    val recipient = uriRecipient(barcode.displayValue)
                    if (recipient.valid) {
                        var fragment = SendCoinsFragment.newInstance(recipient)
                        var bsBehaviour = BottomSheetBehavior.from(fragment as View)
                        bsBehaviour.setPeekHeight(500)
                        bsBehaviour.setFitToContents(true)
                        bsBehaviour.state = BottomSheetBehavior.STATE_EXPANDED
                        fragment.show(supportFragmentManager, SendCoinsFragment::class.java.simpleName)
                    }
                }
            }
        } else {
            super.onActivityResult(requestCode, resultCode, data)
        }
    }

    companion object {
        private const val BARCODE_READER_REQUEST_CODE = 1
    }
}
