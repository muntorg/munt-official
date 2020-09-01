// Copyright (c) 2019 The Novo developers
// Authored by: Willem de Jonge (willem@isnapp.nl), Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Novo software license, see the accompanying
// file COPYING

package com.novo.unity_wallet.ui.widgets

import android.content.Context
import android.util.AttributeSet
import android.widget.ViewSwitcher
import com.novo.jniunifiedbackend.ILibraryController
import com.novo.unity_wallet.Authentication
import com.novo.unity_wallet.R
import kotlinx.android.synthetic.main.pref_view_recovery.view.*

class AuthenticatedRecoveryView(context: Context?, attrs: AttributeSet?) : ViewSwitcher(context, attrs) {

    override fun onFinishInflate() {
        super.onFinishInflate()
        val lockedView = getChildAt(0)
        lockedView.setOnClickListener {
            Authentication.instance.authenticate(context!!, null, context?.getString(R.string.show_recovery_msg)) {
                displayedChild = 1
                //TODO: Reintroduce showing birth time here if/when we decide we want it in future
                recoveryPhrase.text = ILibraryController.GetRecoveryPhrase()?.trimEnd('0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ' ')
            }
        }
        recoveryPhrase.setOnClickListener {
            displayedChild = 0
        }
    }
}
