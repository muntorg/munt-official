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
import com.google.android.material.bottomnavigation.BottomNavigationView
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.main_activity_fragments.*
import com.gulden.unity_wallet.main_activity_fragments.SendFragment.OnFragmentInteractionListener
import com.gulden.unity_wallet.ui.monitor.NetworkMonitorActivity
import com.gulden.unity_wallet.util.AppBaseActivity
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import android.widget.Toast
import com.gulden.unity_wallet.util.getAndroidVersion
import com.gulden.unity_wallet.util.getDeviceName


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
        LocalCurrencyFragment.OnFragmentInteractionListener,
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

    fun onRequestAdvancedSettings(view: View? = null)
    {
        val intent = Intent(this, NetworkMonitorActivity::class.java)
        startActivity(intent)
    }

    fun onRequestSupport(view: View? = null)
    {
        try
        {
            val i = Intent(Intent.ACTION_SENDTO)
            i.type = "message/rfc822"
            i.setData(Uri.parse("mailto:"));
            i.putExtra(Intent.EXTRA_EMAIL, arrayOf("support@gulden.com"))
            i.putExtra(Intent.EXTRA_SUBJECT, "Support request")
            i.putExtra(Intent.EXTRA_TEXT, getDeviceName() + " / " + getAndroidVersion() + " / " +  getString(R.string.about_text_app_name) + System.getProperty("line.separator") )
            startActivity(Intent.createChooser(i, "Send mail..."))
        }
        catch (ex: android.content.ActivityNotFoundException)
        {
            Toast.makeText(this, "No email app installed.", Toast.LENGTH_SHORT).show()
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
        //Always create a new settings fragment (In case we are in a nested menu and want to go back to top level)
        settingsFragment = SettingsFragment()
        replaceFragment(settingsFragment!!, R.id.mainLayout)
    }

    private var sendFragment : SendFragment ?= null
    private var receiveFragment : ReceiveFragment ?= null
    private var transactionFragment : MutationFragment ?= null
    private var settingsFragment : SettingsFragment ?= null

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
}
