// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.app.Activity
import android.content.Intent
import android.os.Bundle

class IntroActivity : Activity()
{
    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
        (application as ActivityManager).introActivity = this;
    }

    override fun onStop()
    {
        super.onStop()
        (application as ActivityManager).introActivity = null;
    }

    fun showActivityForIntent(intent : Intent)
    {
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        startActivity(intent)

        finish()
    }

    fun gotoWelcomeActivity()
    {
        showActivityForIntent(Intent(this, WelcomeActivity::class.java))
    }

    fun gotoWalletActivity()
    {
        showActivityForIntent(Intent(this, WalletActivity::class.java))
    }
}
