// Copyright (c) 2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.widgets

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.TextView
import com.gulden.unity_wallet.BuildConfig
import com.gulden.unity_wallet.UnityCore

class UnityBuildInfoView(context: Context?, attrs: AttributeSet?) : TextView(context, attrs) {
    init {
        @Suppress("ConstantConditionIf")
        if (BuildConfig.TESTNET) {
            text = "Unity core ${UnityCore.instance.buildInfo}"
        }
        else {
            visibility = View.GONE
        }
    }
}
