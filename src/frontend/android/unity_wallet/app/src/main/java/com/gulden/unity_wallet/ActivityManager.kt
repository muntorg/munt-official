// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za) & Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.app.Application
import android.app.Notification
import android.app.NotificationManager
import android.app.PendingIntent
import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.OnLifecycleEvent
import android.arch.lifecycle.ProcessLifecycleOwner
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.support.v4.app.NotificationCompat
import android.support.v7.preference.PreferenceManager
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.TransactionRecord
import com.gulden.jniunifiedbackend.TransactionType

class ActivityManager : Application(), LifecycleObserver, UnityCore.Observer, SharedPreferences.OnSharedPreferenceChangeListener
{
    override fun onCreate()
    {
        super.onCreate()

        UnityCore.instance.configure(
                UnityConfig(dataDir = applicationContext.applicationInfo.dataDir, testnet = Constants.TEST)
        )

        UnityCore.instance.addObserver(this)

        val preferences = PreferenceManager.getDefaultSharedPreferences(this)
        preferences.registerOnSharedPreferenceChangeListener(this)
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
                val syncType = sharedPreferences!!.getString(key, getString(R.string.background_sync_default))
                setupBackgroundSync(this, syncType)
            }
        }
    }

    override fun incomingTransaction(transaction: TransactionRecord): Boolean {
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
