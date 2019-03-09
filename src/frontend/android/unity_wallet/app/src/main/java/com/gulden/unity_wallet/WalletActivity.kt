// Copyright (c) 2018-2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
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
import com.gulden.unity_wallet.ui.monitor.NetworkMonitorActivity
import com.gulden.unity_wallet.util.AppBaseActivity
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.FragmentManager.POP_BACK_STACK_INCLUSIVE
import com.gulden.unity_wallet.util.getAndroidVersion
import com.gulden.unity_wallet.util.getDeviceName
import org.jetbrains.anko.contentView
import org.jetbrains.anko.design.snackbar
import kotlin.concurrent.thread


inline fun androidx.fragment.app.FragmentManager.inTransaction(func: androidx.fragment.app.FragmentTransaction.() -> androidx.fragment.app.FragmentTransaction) {
    beginTransaction().func().commit()
}

fun AppCompatActivity.addFragment(fragment: androidx.fragment.app.Fragment, frameId: Int){
    supportFragmentManager.inTransaction { add(frameId, fragment) }
}

fun AppCompatActivity.removeFragment(fragment: Any, popFromBackstack : Boolean, name : String) {
    supportFragmentManager.inTransaction{remove(fragment as androidx.fragment.app.Fragment)}
    if (popFromBackstack)
        supportFragmentManager.popBackStackImmediate(name, POP_BACK_STACK_INCLUSIVE)
}

fun AppCompatActivity.replaceFragment(fragment: Any, frameId: Int) {
    supportFragmentManager.inTransaction{replace(frameId, fragment as androidx.fragment.app.Fragment)}
}

fun AppCompatActivity.replaceFragmentWithBackstack(fragment: Any, frameId: Int, name : String) {
    supportFragmentManager.inTransaction{replace(frameId, fragment as androidx.fragment.app.Fragment).addToBackStack(name)}
}

class WalletActivity : UnityCore.Observer, AppBaseActivity(),
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

        topLayoutBarSettingsBackButton.setOnClickListener { onBackPressed() }
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
            i.data = Uri.parse("mailto:")
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
        clearSettingsPages()
    }

    private fun gotoReceivePage()
    {
        if (receiveFragment == null)
            receiveFragment = ReceiveFragment()
        replaceFragment(receiveFragment!!, R.id.mainLayout)
        clearSettingsPages()
    }

    private fun gotoTransactionPage()
    {
        if (transactionFragment == null)
            transactionFragment = MutationFragment()
        replaceFragment(transactionFragment!!, R.id.mainLayout)
        clearSettingsPages()
    }

    private fun clearSettingsPages()
    {
        clearNestedSettingsPages()
        if (settingsFragment != null) { removeFragment(settingsFragment!!, false, ""); settingsFragment = null }
    }

    private fun clearNestedSettingsPages()
    {
        if (walletSettingsFragment != null) { removeFragment(walletSettingsFragment!!,true,  "walletsettings"); walletSettingsFragment = null }
        if (currencySettingsFragment != null) { removeFragment(currencySettingsFragment!!, true, "currencysettings"); currencySettingsFragment = null }
    }

    private fun gotoSettingsPage()
    {
        clearNestedSettingsPages()
        if (settingsFragment == null)
            settingsFragment = SettingsFragment()
        replaceFragment(settingsFragment!!, R.id.mainLayout)
    }

    fun gotoWalletSettingsPage()
    {
        clearNestedSettingsPages()
        if (walletSettingsFragment == null)
            walletSettingsFragment = WalletSettingsFragment()

        replaceFragmentWithBackstack(walletSettingsFragment!!, R.id.mainLayout, "walletsettings")
    }

    fun gotoCurrencyPage()
    {
        clearNestedSettingsPages()
        if (currencySettingsFragment == null)
            currencySettingsFragment = LocalCurrencyFragment()

        replaceFragmentWithBackstack(currencySettingsFragment!!, R.id.mainLayout, "currencysettings")
    }

    private var sendFragment : SendFragment ?= null
    private var receiveFragment : ReceiveFragment ?= null
    private var transactionFragment : MutationFragment ?= null
    private var settingsFragment : SettingsFragment ?= null
    private var walletSettingsFragment : WalletSettingsFragment ?= null
    private var currencySettingsFragment : LocalCurrencyFragment ?= null

    private val mOnNavigationItemSelectedListener = BottomNavigationView.OnNavigationItemSelectedListener { item ->
         when (item.itemId) {
            R.id.navigation_send -> { gotoSendPage(); return@OnNavigationItemSelectedListener true }
            R.id.navigation_receive -> { gotoReceivePage(); return@OnNavigationItemSelectedListener true }
            R.id.navigation_transactions -> { gotoTransactionPage(); return@OnNavigationItemSelectedListener true }
            R.id.navigation_settings -> { gotoSettingsPage(); return@OnNavigationItemSelectedListener true }
        }
        false
    }

    fun performLink(linkURI: String)
    {
        Authentication.instance.authenticate(this, null, getString(R.string.link_wallet_auth_desc)) { password ->
            // ReplaceWalletLinkedFromURI can be long running, so run it in a thread that isn't the UI thread.
            thread(start = true)
            {
                if (!GuldenUnifiedBackend.ReplaceWalletLinkedFromURI(linkURI, password.joinToString("")))
                {
                    runOnUiThread {
                        AlertDialog.Builder(this)
                                .setTitle(getString(R.string.no_guldensync_warning_title))
                                .setMessage(getString(R.string.no_guldensync_warning))
                                .setPositiveButton(getString(R.string.button_ok)) {
                                    dialogInterface, i -> dialogInterface.dismiss()
                                }.setCancelable(true).create().show()
                    }
                }
                else
                {
                    runOnUiThread {
                        this.contentView?.snackbar(getString(R.string.rescan_started))
                        gotoReceivePage()
                    }
                }
            }
        }
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

    fun showSettingsTitle(title : String)
    {
        topLayoutBarSettingsHeader.visibility = View.VISIBLE
        topLayoutBar.visibility = View.GONE
        topLayoutBarSettingsTitle.text = title
    }

    fun hideSettingsTitle()
    {
        topLayoutBarSettingsHeader.visibility = View.GONE
        topLayoutBar.visibility = View.VISIBLE
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
