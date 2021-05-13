// Copyright (c) 2018 The Florin developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Florin software license, see the accompanying
// file COPYING

package com.florin.unity_wallet.util

import android.app.Activity
import android.content.Intent
import com.florin.unity_wallet.WalletActivity

fun gotoWalletActivity(activity: Activity)
{
    val intent = Intent(activity, WalletActivity::class.java)
    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
    activity.startActivity(intent)
    activity.finish()
}
