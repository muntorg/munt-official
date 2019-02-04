// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za) & Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Intent
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity

class IntroActivity : AppCompatActivity(), UnityCore.Observer
{
    private val coreObserverProxy = CoreObserverProxy(this, this)

    override fun createNewWallet(): Boolean {
        gotoActivity(WelcomeActivity::class.java)
        return true
    }

    override fun haveExistingWallet(): Boolean {
        // after init there will be a coreReady event, after which the WalletActivity can start
        receivedExistingWalletEvent = true
        return true
    }

    override fun onCoreReady(): Boolean {
        if (receivedExistingWalletEvent)
            gotoActivity(WalletActivity::class.java)
        return true
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val core = UnityCore.instance

        core.addObserver(coreObserverProxy)
        core.startCore()

        // if core is already ready we are resuming a session and can go directly to the wallet
        // (there will be no further coreReady or haveExistingWallet event)
        if (core.isCoreReady())
            gotoActivity(WalletActivity::class.java)
    }

    override fun onDestroy() {
        super.onDestroy()

        UnityCore.instance.removeObserver(coreObserverProxy)
    }

    private fun gotoActivity(cls: Class<*> )
    {
            val intent = Intent(this, cls)
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
            startActivity(intent)

            finish()
    }

    private var receivedExistingWalletEvent: Boolean = false
}
