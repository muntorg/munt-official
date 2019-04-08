// Copyright (c) 2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui

import android.content.Context
import android.util.DisplayMetrics


fun Dp2Px(context: Context, dp: Int): Int {
    return Math.round(dp * (context.resources.displayMetrics.xdpi / DisplayMetrics.DENSITY_DEFAULT))
}

fun Px2Dp(context: Context, px: Int): Int {
    return Math.round(px / (context.resources.displayMetrics.xdpi / DisplayMetrics.DENSITY_DEFAULT))
}
