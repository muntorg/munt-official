// Copyright (c) 2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.widgets

import android.content.Context
import android.content.SharedPreferences
import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import android.util.AttributeSet
import android.widget.Toast
import android.widget.ViewSwitcher
import androidx.preference.PreferenceManager
import com.gulden.unity_wallet.Authentication
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.UnityCore
import org.jetbrains.anko.runOnUiThread

class HideBalanceView(context: Context?, attrs: AttributeSet?) : ViewSwitcher(context, attrs), Authentication.LockingObserver, UnityCore.Observer, OnSharedPreferenceChangeListener {

    override fun onFinishInflate() {
        super.onFinishInflate()
        val viewWhenHidden = getChildAt(0)
        viewWhenHidden.setOnClickListener {
            val isSynced = UnityCore.instance.progressPercent >= 100.0

            if (isSynced)
                Authentication.instance.unlock(context!!, null, null)
            else
                Toast.makeText(context, context.getString(R.string.show_balance_when_synced), Toast.LENGTH_LONG).show()
        }

        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        preferences.registerOnSharedPreferenceChangeListener(this)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        if (key == "preference_hide_balance")
            updateViewState()
    }

    private fun updateViewState() {
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        val isHideBalanceSet = preferences.getBoolean("preference_hide_balance", true)
        val isSynced = UnityCore.instance.progressPercent >= 100.0
        val isLocked = Authentication.instance.isLocked()

        val showBalance = isSynced && !(isLocked && isHideBalanceSet)
        displayedChild = if (showBalance) 1 else 0
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        updateViewState()
        Authentication.instance.addObserver(this)
        UnityCore.instance.addObserver(this, fun (callback:() -> Unit) { context.runOnUiThread { callback() }})
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        Authentication.instance.removeObserver(this)
        UnityCore.instance.removeObserver(this)
    }

    override fun onUnlock() {
        updateViewState()
    }

    override fun onLock() {
        updateViewState()
    }

    override fun syncProgressChanged(percent: Float): Boolean {
        updateViewState()
        return true
    }

}
