// Copyright (c) 2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.widgets

import android.content.Context
import android.content.SharedPreferences
import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import android.util.AttributeSet
import android.view.Gravity
import android.widget.Toast
import android.widget.ViewSwitcher
import androidx.preference.PreferenceManager
import com.gulden.unity_wallet.Authentication
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.UnityCore
import org.jetbrains.anko.runOnUiThread

class HideBalanceView(context: Context?, attrs: AttributeSet?) : ViewSwitcher(context, attrs), Authentication.LockingObserver, UnityCore.Observer, OnSharedPreferenceChangeListener {

    private var syncToastShowed = false // used to show it just once (unless triggered explicitly by tapping)
    private val preferences = PreferenceManager.getDefaultSharedPreferences(context)
    private val isHideBalanceSet: Boolean
            get() {
              return preferences.getBoolean("preference_hide_balance", true)
            }

    override fun onFinishInflate() {
        super.onFinishInflate()
        val viewWhenHidden = getChildAt(0)
        viewWhenHidden.setOnClickListener {
            if (Authentication.instance.isLocked() && isHideBalanceSet)
                Authentication.instance.unlock(context!!, null, null)
            else
                // not locked or hide balance pref is disabled so balance is not showing because not synced
                showSyncToast()
        }

        preferences.registerOnSharedPreferenceChangeListener(this)
    }

    private fun showSyncToast() {
        try
        {
            val toast = Toast.makeText(context, context.getString(R.string.show_balance_when_synced), Toast.LENGTH_LONG)
            toast.setGravity(Gravity.TOP, 0, 0)
            toast.show()
        }
        catch (e : Exception)
        {

        }
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        if (key == "preference_hide_balance")
            updateViewState()
    }

    private fun updateViewState() {
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        val isSynced = UnityCore.instance.progressPercent >= 100.0
        val isLocked = Authentication.instance.isLocked()

        val showBalance = isSynced && !(isLocked && isHideBalanceSet)
        displayedChild = if (showBalance) 1 else 0

        // when balance not shown due to not being synced show toast (just once)
        if (!isSynced && !(isLocked && isHideBalanceSet) && !syncToastShowed)
        {
            showSyncToast()
            syncToastShowed = true
        }
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
