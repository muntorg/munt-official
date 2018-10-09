// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.support.design.widget.BottomNavigationView
import android.support.v4.app.Fragment
import android.support.v4.app.FragmentManager
import android.support.v4.app.FragmentTransaction
import android.support.v7.app.AppCompatActivity
import android.view.View
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.vision.barcode.Barcode
import com.gulden.barcodereader.BarcodeCaptureActivity
import com.gulden.jniunifiedbackend.*
import com.gulden.unity_wallet.MainActivityFragments.ReceiveFragment
import com.gulden.unity_wallet.MainActivityFragments.SendFragment
import com.gulden.unity_wallet.MainActivityFragments.SendFragment.OnFragmentInteractionListener
import com.gulden.unity_wallet.MainActivityFragments.SettingsFragment
import com.gulden.unity_wallet.MainActivityFragments.TransactionFragment
import kotlinx.android.synthetic.main.activity_main.*
import java.util.*
import android.content.ComponentName
import android.os.IBinder
import android.content.ServiceConnection







inline fun FragmentManager.inTransaction(func: FragmentTransaction.() -> FragmentTransaction) {
    beginTransaction().func().commit()
}

fun AppCompatActivity.addFragment(fragment: Fragment, frameId: Int){
    supportFragmentManager.inTransaction { add(frameId, fragment) }
}

fun AppCompatActivity.replaceFragment(fragment: Any, frameId: Int) {
    supportFragmentManager.inTransaction{replace(frameId, fragment as Fragment)}
}

class MainActivity : AppCompatActivity(), UnityService.UnityServiceSignalHandler, OnFragmentInteractionListener, ReceiveFragment.OnFragmentInteractionListener, TransactionFragment.OnFragmentInteractionListener, SettingsFragment.OnFragmentInteractionListener  {

    override fun onFragmentInteraction(uri: Uri) {
        TODO("not implemented")
    }

    private val sendFragment : SendFragment = SendFragment();
    private val receiveFragment : ReceiveFragment = ReceiveFragment();
    private val transactionFragment : TransactionFragment = TransactionFragment();
    private val settingsFragment : SettingsFragment = SettingsFragment();

    private val mOnNavigationItemSelectedListener = BottomNavigationView.OnNavigationItemSelectedListener { item ->
        when (item.itemId) {
            R.id.navigation_send -> {
                replaceFragment(sendFragment, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_receive -> {
                replaceFragment(receiveFragment, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_transactions -> {
                replaceFragment(transactionFragment, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_settings -> {
                replaceFragment(settingsFragment, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
        }
        false
    }

    override fun syncProgressChanged(percent: Float): Boolean
    {
        this@MainActivity.runOnUiThread{
            syncProgress.progress = (1000000 * (percent/100)).toInt();
        }
        return true;
    }

    override fun walletBalanceChanged(balance: Long): Boolean
    {
        this@MainActivity.runOnUiThread{
            walletBalance.text = (balance.toFloat() / 100000000).toString()
            walletBalanceLogo.visibility = View.VISIBLE;
            walletBalance.visibility = View.VISIBLE;
            walletLogo.visibility = View.GONE;
        }
        return true;
    }

    override fun coreUIInit() : Boolean
    {
        this@MainActivity.runOnUiThread{
            addFragment(sendFragment, R.id.mainLayout)
        }
        return true;
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val service = Intent(this@MainActivity, UnityService::class.java)
        if (!UnityService.IS_SERVICE_RUNNING) {
            service.action = UnityService.START_FOREGROUND_ACTION
            UnityService.IS_SERVICE_RUNNING = true
            startService(service)
        }

        navigation.setOnNavigationItemSelectedListener(mOnNavigationItemSelectedListener)

        syncProgress.max = 1000000;
        syncProgress.progress = 0;
    }


    private var bound = false
    private lateinit var myService : UnityService
    override fun onStart() {
        super.onStart()

        // Bind to service
        val intent = Intent(this, UnityService::class.java)
        bindService(intent, serviceConnection, BIND_AUTO_CREATE)
    }

    override fun onStop() {
        super.onStop()

        // Unbind from service
        if (bound) {
            myService.signalHandler = null
            unbindService(serviceConnection);
            bound = false;
        }
    }

    override fun onDestroy() {
        super.onDestroy()
    }

    override fun onRestart() {
        super.onRestart()
    }
    override fun onPause() {
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
    }

    fun handleQRScanButtonClick(view : View) {
        val intent = Intent(applicationContext, BarcodeCaptureActivity::class.java)
        startActivityForResult(intent, BARCODE_READER_REQUEST_CODE)
    }

    fun Uri.getParameters(): HashMap<String, String> {
        val items : HashMap<String, String> = HashMap<String, String>();
        if (isOpaque())
            return items;

        val query = encodedQuery ?: return items;

        var start = 0
        do {
            val nextAmpersand = query.indexOf('&', start)
            val end = if (nextAmpersand != -1) nextAmpersand else query.length

            var separator = query.indexOf('=', start)
            if (separator > end || separator == -1) {
                separator = end
            }

            if (separator == end) {
                items[Uri.decode(query.substring(start, separator))] = "";
            } else {
                items[Uri.decode(query.substring(start, separator))] = Uri.decode(query.substring(separator + 1, end));
            }

            // Move start to end of name.
            if (nextAmpersand != -1) {
                start = nextAmpersand + 1
            } else {
                break
            }
        } while (true)
        return items
    }
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == BARCODE_READER_REQUEST_CODE) {
            if (resultCode == CommonStatusCodes.SUCCESS)
            {
                if (data != null) {
                    val barcode = data.getParcelableExtra<Barcode>(BarcodeCaptureActivity.BarcodeObject)

                    val parsedQRCodeURI = Uri.parse(barcode.displayValue);
                    var address : String = "";
                    address += parsedQRCodeURI?.authority;
                    address += parsedQRCodeURI?.path;
                    val parsedQRCodeURIRecord = UriRecord(parsedQRCodeURI.scheme, address , parsedQRCodeURI.getParameters())
                    val recipient = GuldenUnifiedBackend.IsValidRecipient(parsedQRCodeURIRecord);
                    if (recipient.valid) {
                        val intent = Intent(applicationContext, SendCoinsActivity::class.java)
                        intent.putExtra(SendCoinsActivity.EXTRA_RECIPIENT, recipient);
                        startActivityForResult(intent, SEND_COINS_RETURN_CODE)
                    }
                }
            }
        } else {
            super.onActivityResult(requestCode, resultCode, data)
        }
    }

    private val serviceConnection = object : ServiceConnection {

        override fun onServiceConnected(className: ComponentName, service: IBinder) {
            // cast the IBinder and get MyService instance
            val binder = service as UnityService.LocalBinder
            myService = binder.service
            bound = true
            myService.signalHandler = this@MainActivity // register
        }

        override fun onServiceDisconnected(arg0: ComponentName) {
            bound = false
        }
    }

    public fun setFocusOnAddress(view : View)
    {
        receiveFragment?.setFocusOnAddress();
    }

    companion object {
        private val BARCODE_READER_REQUEST_CODE = 1
        public val SEND_COINS_RETURN_CODE = 2
        public val BUY_RETURN_CODE = 3
    }
}
