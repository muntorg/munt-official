// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.util

import android.os.Build


fun getDeviceName(): String?
{
    val manufacturer = Build.MANUFACTURER
    val model = Build.MODEL
    return if (model.startsWith(manufacturer)) model else manufacturer + " " + model
}

fun getAndroidVersion(): String
{
    val version = Build.VERSION.RELEASE.toString()
    return if (version.startsWith("Android")) version else "Android " + version
}