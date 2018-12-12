// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za) & Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.app.Application
import android.app.Notification
import android.app.NotificationManager
import android.app.PendingIntent
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ProcessLifecycleOwner
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import androidx.core.app.NotificationCompat
import androidx.preference.PreferenceManager
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.TransactionRecord
import com.gulden.jniunifiedbackend.TransactionType
import org.jetbrains.anko.runOnUiThread

class ActivityManager : Application(), LifecycleObserver, UnityCore.Observer, SharedPreferences.OnSharedPreferenceChangeListener
{
    override fun onCreate()
    {
        super.onCreate()

        AppContext.instance = baseContext

        UnityCore.instance.configure(
                UnityConfig(dataDir = applicationContext.applicationInfo.dataDir, testnet = Constants.TEST)
        )

        UnityCore.instance.addObserver(this)

        val preferences = PreferenceManager.getDefaultSharedPreferences(this)
        preferences.registerOnSharedPreferenceChangeListener(this)

        setupBackgroundSync(this)
    }

    override fun onCoreReady(): Boolean {
        ProcessLifecycleOwner.get().lifecycle.addObserver(this)
        return true
    }

    override fun onCoreShutdown(): Boolean {
        ProcessLifecycleOwner.get().lifecycle.removeObserver(this)
        return true
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        when (key) {
            "preference_background_sync" -> {
                setupBackgroundSync(this)
            }
        }
    }

    override fun incomingTransaction(transaction: TransactionRecord): Boolean {
        runOnUiThread {
            val preferences = PreferenceManager.getDefaultSharedPreferences(this)
            if (preferences.getBoolean("preference_notify_transaction_activity", true)) {
                val notificationIntent = Intent(this, WalletActivity::class.java)
                val pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0)

                var prefix = "+"
                if (transaction.type == TransactionType.SEND)
                    prefix = "-"

                val notification = NotificationCompat.Builder(this)
                        .setContentTitle("Incoming transaction")
                        .setTicker("Incoming transaction")
                        .setContentText((" "+prefix+"%.2f").format(transaction.amount.toDouble() / 100000000))
                        .setSmallIcon(R.drawable.ic_g_logo)
                        //.setLargeIcon(Bitmap.createScaledBitmap(R.drawable.ic_g_logo, 128, 128, false))
                        .setContentIntent(pendingIntent)
                        .setOngoing(false)
                        .setAutoCancel(true)
                        .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                        //.setPublicVersion()
                        //.setTimeoutAfter()
                        .setDefaults(Notification.DEFAULT_ALL)
                        .build()

                val notificationManager = applicationContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
                notificationManager.notify(1, notification)
            }
        }

        return true
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun allActivitiesStopped()
    {
        GuldenUnifiedBackend.PersistAndPruneForSPV();
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun firstActivityStarted()
    {
        GuldenUnifiedBackend.ResetUnifiedProgress()
    }

}
