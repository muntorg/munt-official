// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.novo.wallet.util

import android.app.Activity
import android.content.Intent
import com.novo.wallet.WalletActivity

fun gotoWalletActivity(activity: Activity)
{
    val intent = Intent(activity, WalletActivity::class.java)
    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
    activity.startActivity(intent)
    activity.finish()
}
