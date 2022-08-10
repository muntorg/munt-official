// Copyright (c) 2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.widgets

import android.content.Context
import android.util.AttributeSet
import android.widget.ViewSwitcher
import com.gulden.unity_wallet.Authentication

class LockView(context: Context?, attrs: AttributeSet?) : ViewSwitcher(context, attrs), Authentication.LockingObserver {

    override fun onFinishInflate() {
        super.onFinishInflate()
        val lockedView = getChildAt(0)
        lockedView.setOnClickListener {
            Authentication.instance.unlock(context!!, null, null)
        }
    }

    private fun displayLockingState() {
        displayedChild = if (Authentication.instance.isLocked()) 0 else 1
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        displayLockingState()
        Authentication.instance.addObserver(this)
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        Authentication.instance.removeObserver(this)
    }

    override fun onUnlock() {
        displayLockingState()
    }

    override fun onLock() {
        displayLockingState()
    }

}
