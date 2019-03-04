package com.gulden.unity_wallet.util

import android.app.Activity
import android.content.Intent
import com.gulden.unity_wallet.WalletActivity

fun gotoWalletActivity(activity: Activity)
{
    val intent = Intent(activity, WalletActivity::class.java)
    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
    activity.startActivity(intent)
    activity.finish()
}
