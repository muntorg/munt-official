// Copyright (c) 2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package unity_wallet.ui

import android.content.Context
import android.graphics.Point
import android.util.DisplayMetrics
import android.view.WindowManager

fun Dp2Px(context: Context, dp: Int): Int {
    return Math.round(dp * (context.resources.displayMetrics.xdpi / DisplayMetrics.DENSITY_DEFAULT))
}

fun Px2Dp(context: Context, px: Int): Int {
    return Math.round(px / (context.resources.displayMetrics.xdpi / DisplayMetrics.DENSITY_DEFAULT))
}

fun getDisplayDimensions(context: Context): Point {
    val wm = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
    val display = wm.defaultDisplay

    val metrics = DisplayMetrics()
    display.getMetrics(metrics)
    val screenWidth = metrics.widthPixels
    var screenHeight = metrics.heightPixels

    // find out if status bar has already been subtracted from screenHeight
    display.getRealMetrics(metrics)
    val physicalHeight = metrics.heightPixels
    val statusBarHeight = getStatusBarHeight(context)
    val navigationBarHeight = getNavigationBarHeight(context)
    val heightDelta = physicalHeight - screenHeight
    if (heightDelta == 0 || heightDelta == navigationBarHeight) {
        screenHeight -= statusBarHeight
    }

    return Point(screenWidth, screenHeight)
}

fun getStatusBarHeight(context: Context): Int {
    val resources = context.resources
    val resourceId = resources.getIdentifier("status_bar_height", "dimen", "android")
    return if (resourceId > 0) resources.getDimensionPixelSize(resourceId) else 0
}

fun getNavigationBarHeight(context: Context): Int {
    val resources = context.resources
    val resourceId = resources.getIdentifier("navigation_bar_height", "dimen", "android")
    return if (resourceId > 0) resources.getDimensionPixelSize(resourceId) else 0
}