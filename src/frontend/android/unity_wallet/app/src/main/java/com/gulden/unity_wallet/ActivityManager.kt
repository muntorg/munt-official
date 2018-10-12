// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.app.Application
import android.content.ComponentName
import android.content.Intent
import android.content.ServiceConnection
import android.os.IBinder
import android.support.v7.app.AppCompatActivity

class ActivityManager : Application(), UnityService.UnityServiceSignalHandler
{
    var mainActivity : MainActivity ?= null;
    var introActivity : IntroActivity ?= null;
    override fun syncProgressChanged(percent: Float): Boolean
    {
        mainActivity?.runOnUiThread{ mainActivity?.setSyncProgress(percent); }
        return true;
    }
    override fun walletBalanceChanged(balance: Long): Boolean
    {
        mainActivity?.runOnUiThread{ mainActivity?.setWalletBalance(balance); }
        return true;
    }
    override fun coreUIInit() : Boolean
    {
        mainActivity?.runOnUiThread{ mainActivity?.coreUIInit(); }
        return true;
    }
    override fun haveExistingWallet() : Boolean
    {
        introActivity?.runOnUiThread{ introActivity?.gotoMainActivity(); }
        return true;
    }
    override fun createNewWallet() : Boolean
    {
        introActivity?.runOnUiThread{ introActivity?.gotoWelcomeActivity(); }
        return true;
    }

    private var bound = false
    private lateinit var myService : UnityService
    override fun onCreate()
    {
        super.onCreate()

        val service = Intent(this, UnityService::class.java)
        if (!UnityService.IS_SERVICE_RUNNING) {
            service.action = UnityService.START_FOREGROUND_ACTION
            UnityService.IS_SERVICE_RUNNING = true
            startService(service)
        }

        // Bind to service
        val intent = Intent(this, UnityService::class.java)
        bindService(intent, serviceConnection, AppCompatActivity.BIND_AUTO_CREATE)
    }

    //TODO: when to unbind service?
    /*
    override fun onStop() {
        super.onStop()

        // Unbind from service
        if (bound) {
            myService.signalHandler = null
            unbindService(serviceConnection);
            bound = false;
        }
    }*/

    private val serviceConnection = object : ServiceConnection
    {

        override fun onServiceConnected(className: ComponentName, service: IBinder) {
            // cast the IBinder and get MyService instance
            val binder = service as UnityService.LocalBinder
            myService = binder.service
            bound = true
            myService.signalHandler = (this@ActivityManager) // register
        }

        override fun onServiceDisconnected(arg0: ComponentName) {
            bound = false
        }
    }
}