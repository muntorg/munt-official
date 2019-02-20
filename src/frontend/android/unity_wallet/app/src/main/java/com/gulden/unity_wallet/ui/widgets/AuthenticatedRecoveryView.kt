// Copyright (c) 2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.widgets

import android.content.Context
import android.util.AttributeSet
import android.widget.ViewSwitcher
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.Authentication
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.pref_view_recovery.view.*

class AuthenticatedRecoveryView(context: Context?, attrs: AttributeSet?) : ViewSwitcher(context, attrs) {

    override fun onFinishInflate() {
        super.onFinishInflate()
        val lockedView = getChildAt(0)
        lockedView.setOnClickListener {
            Authentication.instance.authenticate(context!!, null, context?.getString(R.string.show_recovery_msg)) {
                displayedChild = 1
                recoveryPhrase.text = GuldenUnifiedBackend.GetRecoveryPhrase()
            }
        }
        recoveryPhrase.setOnClickListener {
            displayedChild = 0
        }
    }
}
