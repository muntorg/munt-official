// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.view.View
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.vision.barcode.Barcode
import com.gulden.barcodereader.BarcodeCaptureActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.ui.EnterRecoveryPhraseActivity
import com.gulden.unity_wallet.ui.ShowRecoveryPhraseActivity

class IntroActivity : Activity()
{
    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
        (application as ActivityManager).introActivity = this;
    }

    fun showActivityForIntent(intent : Intent)
    {
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        startActivity(intent)

        (application as ActivityManager).introActivity = null
        finish()
    }

    fun gotoWelcomeActivity()
    {
        showActivityForIntent(Intent(this, WelcomeActivity::class.java))
    }

    fun gotoMainActivity()
    {
        showActivityForIntent(Intent(this, MainActivity::class.java))
    }
}
