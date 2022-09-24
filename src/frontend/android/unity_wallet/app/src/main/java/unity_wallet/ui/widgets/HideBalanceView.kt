// Copyright (c) 2019-2022 The Centure developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

package unity_wallet.ui.widgets

import android.content.Context
import android.content.SharedPreferences
import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import android.util.AttributeSet
import android.view.Gravity
import android.widget.Toast
import android.widget.ViewSwitcher
import androidx.preference.PreferenceManager
import unity_wallet.Authentication
import unity_wallet.R
import unity_wallet.UnityCore
import unity_wallet.util.invokeNowOrOnSuccessfulCompletion
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import android.os.Handler
import android.os.Looper
import kotlin.coroutines.CoroutineContext

class HideBalanceView(context: Context?, attrs: AttributeSet?) : ViewSwitcher(context, attrs), Authentication.LockingObserver, UnityCore.Observer, OnSharedPreferenceChangeListener, CoroutineScope {

    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    private val handler = Handler(Looper.getMainLooper())
    private var syncToastShowed = false // used to show it just once (unless triggered explicitly by tapping)
    private var syncToastLastShowed = 0L
    private val SYNC_TOAST_RATE_LIMIT_MS = 10000 // show toast only if previously shown at least this long ago
    private val preferences = PreferenceManager.getDefaultSharedPreferences(context!!)
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
        val now = System.currentTimeMillis()
        if (now - syncToastLastShowed > SYNC_TOAST_RATE_LIMIT_MS) {
            val toast = Toast.makeText(context, context.getString(R.string.show_balance_when_synced), Toast.LENGTH_LONG)
            toast.setGravity(Gravity.TOP, 0 , context.resources.getDimensionPixelSize(R.dimen.top_toast_offset))
            toast.show()
            syncToastLastShowed = now
        }
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        if (key == "preference_hide_balance")
            updateViewState()
    }

    private fun updateViewState() {
        UnityCore.instance.walletReady.invokeNowOrOnSuccessfulCompletion(this) {
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
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        updateViewState()
        Authentication.instance.addObserver(this)
        UnityCore.instance.addObserver(this, fun (callback:() -> Unit) { handler.post { callback() }})
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

    override fun syncProgressChanged(percent: Float) {
        updateViewState()
    }

}
