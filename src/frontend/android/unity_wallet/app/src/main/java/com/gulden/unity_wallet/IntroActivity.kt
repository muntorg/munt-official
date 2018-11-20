// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za) & Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.app.Activity
import android.content.Intent
import android.os.Bundle

class IntroActivity : Activity(), UnityCore.Observer
{
    override fun createNewWallet(): Boolean {
        gotoActivity(WelcomeActivity::class.java)
        return true
    }

    override fun haveExistingWallet(): Boolean {
        gotoActivity(WalletActivity::class.java)
        return true
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val core = UnityCore.instance

        core.addObserver(this)
        core.startCore()

        if (core.isCoreReady())
            gotoActivity(WalletActivity::class.java)
    }

    override fun onDestroy() {
        super.onDestroy()

        UnityCore.instance.removeObserver(this)
    }

    fun gotoActivity(cls: Class<*> )
    {
        runOnUiThread {
            val intent = Intent(this, cls)
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
            startActivity(intent)

            finish()
        }
    }
}
