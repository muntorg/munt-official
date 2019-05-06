// Copyright (c) 2018-2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.TextView
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
import androidx.core.widget.TextViewCompat
import androidx.fragment.app.FragmentManager.POP_BACK_STACK_INCLUSIVE
import com.gulden.unity_wallet.ui.getDisplayDimensions
import com.gulden.unity_wallet.util.getAndroidVersion
import com.gulden.unity_wallet.util.getDeviceName
import kotlinx.coroutines.async
import org.jetbrains.anko.contentView
import org.jetbrains.anko.custom.async
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
    override fun syncProgressChanged(percent: Float) {
        setSyncProgress(percent)
    }

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_main)

        navigation.setOnNavigationItemSelectedListener(mOnNavigationItemSelectedListener)

        syncProgress.max = 1000000
        syncProgressTextual.text = ""

        // TODO fix fragment creation/addition to work with restored activity instance
        if (sendFragment == null)
            sendFragment = SendFragment()
        addFragment(sendFragment!!, R.id.mainLayout)

        val preferences = PreferenceManager.getDefaultSharedPreferences(this)
        preferences.registerOnSharedPreferenceChangeListener(this)

        topLayoutBarSettingsBackButton.setOnClickListener { onBackPressed() }
    }

    override fun onDestroy() {
        super.onDestroy()

        try
        {
            coroutineContext[Job]!!.cancel()
            val preferences = PreferenceManager.getDefaultSharedPreferences(this)
            preferences.unregisterOnSharedPreferenceChangeListener(this)
        }
        catch (e : Exception)
        {

        }
    }

    override fun onStart() {
        super.onStart()
        UnityCore.instance.addObserver(this@WalletActivity, fun (callback:() -> Unit) { runOnUiThread { callback() }})
    }

    override fun onWalletReady() {
        setSyncProgress(UnityCore.instance.progressPercent)
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

    fun onRequestLicense(view: View? = null)
    {
        startActivity(Intent(this, LicenseActivity::class.java))
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
        val textualProgress = if (percent <= 0.0) getString(R.string.label_sync_progress_connecting) else getString(R.string.label_sync_progress_syncing).format(percent)
        syncProgress.progress = (syncProgress.max * (percent/100)).toInt()
        syncProgressTextual.text = textualProgress
        if (percent < 100.0)
        {
            syncProgressTextual.visibility = View.VISIBLE
            syncProgress.visibility = View.VISIBLE
        }
        else
        {
            syncProgressTextual.visibility = View.INVISIBLE
            syncProgress.visibility = View.INVISIBLE
        }
    }

    private fun setWalletBalance(balance : Long)
    {
        val coins = balance.toDouble() / Config.COIN
        walletBalance.text = String.format("G %.2f", coins)
        walletBalance.visibility = View.VISIBLE

        this.launch( Dispatchers.Main) {
            try {
                val rate = fetchCurrencyRate(localCurrency.code)
                walletBalanceLocal.text = String.format(" (${localCurrency.short} %.${localCurrency.precision}f)",
                        coins * rate)
                walletBalanceLocal.visibility = View.VISIBLE

                // set auto size text if native and local amounts will not fity on a single line
                // this will only kick in in very very rare circumstances
                // like huge balance (> G 10M and very limited device with largest font setting)
                // (before the sync text was moved out of balance display it was a lot easier to trigger)
                val outSize = getDisplayDimensions(this@WalletActivity)
                balanceSection.measure(outSize.x, outSize.y)
                val preferredWidth = balanceSection.measuredWidth
                TextViewCompat.setAutoSizeTextTypeWithDefaults(
                        walletBalanceLocal,
                        if (preferredWidth > outSize.x)  TextView.AUTO_SIZE_TEXT_TYPE_UNIFORM else TextView.AUTO_SIZE_TEXT_TYPE_NONE)
                balanceSection.invalidate()
            }
            catch (e: Throwable) {
                walletBalanceLocal.text = ""
                walletBalanceLocal.visibility = View.GONE
            }
        }
    }

    override fun walletBalanceChanged(balance: Long) {
        setWalletBalance(balance)
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
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(Config.PURCHASE_TEMPLATE.format(GuldenUnifiedBackend.GetReceiveAddress().toString())))
        if (intent.resolveActivity(packageManager) != null)
        {
            startActivity(intent)
        }
    }
}
