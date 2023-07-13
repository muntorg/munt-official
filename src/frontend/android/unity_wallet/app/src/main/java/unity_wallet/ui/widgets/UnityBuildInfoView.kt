// Copyright (c) 2019-2022 The Centure developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

package unity_wallet.ui.widgets

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.TextView
import unity_wallet.BuildConfig
import unity_wallet.UnityCore

class UnityBuildInfoView(context: Context?, attrs: AttributeSet?) : androidx.appcompat.widget.AppCompatTextView(context!!, attrs) {
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
