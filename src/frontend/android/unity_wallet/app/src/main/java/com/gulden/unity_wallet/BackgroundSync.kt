// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.app.*
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.support.v4.app.NotificationCompat
import android.util.Log
import android.content.BroadcastReceiver
import android.support.v7.preference.PreferenceManager
import android.support.v4.content.ContextCompat

private val TAG = "backgroundsync"

fun setupBackgroundSync(context: Context) {

    // get syncType from preferences
    val sharedPreferences = PreferenceManager.getDefaultSharedPreferences(context)
    val syncType = sharedPreferences.getString("preference_background_sync", context.getString(R.string.background_sync_default))!!

    val serviceIntent = Intent(context, SyncService::class.java)

    Log.i(TAG, "Starting background sync: " + syncType)

    when (syncType) {
        "BACKGROUND_SYNC_OFF" -> {
            context.stopService(serviceIntent)
        }

        "BACKGROUND_SYNC_DAILY" -> {
            context.stopService(serviceIntent)
        }

        "BACKGROUND_SYNC_CONTINUOUS" -> {
            ContextCompat.startForegroundService(context, serviceIntent)
        }
    }
}


private var NOTIFICATION_ID_FOREGROUND_SERVICE = 2

class SyncService : Service(), UnityCore.Observer
{
    override fun onBind(intent: Intent?): IBinder? {
        TODO("not implemented") //To change body of created functions use File | Settings | File Templates.
    }

    override fun syncProgressChanged(percent: Float): Boolean {
        Log.i(TAG, "sync progress = " + percent)
        if (builder != null) {
            val b: NotificationCompat.Builder = builder!!
            builder!!.setContentText("progress " + percent)
            val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            notificationManager.notify(NOTIFICATION_ID_FOREGROUND_SERVICE, b.build())
        }
        return true
    }

    private var builder: NotificationCompat.Builder? = null

    val channelID = "com.gulden.unity_wallet.service.channel"

    fun startInForeground()
    {

        val notificationIntent = Intent(this, WalletActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
        {
            val notificationChannel = NotificationChannel(channelID, "Gulden service", NotificationManager.IMPORTANCE_LOW)
            (getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager).createNotificationChannel(notificationChannel)
            notificationChannel.enableLights(false)
            notificationChannel.setShowBadge(false)
        }


        builder = NotificationCompat.Builder(this, channelID)
                .setContentTitle("Gulden")
                .setTicker("Gulden")
                .setContentText("Gulden")
                .setSmallIcon(R.drawable.ic_g_logo)
                .setContentIntent(pendingIntent)
                .setOngoing(true)

        val notification = builder?.build()
        startForeground(NOTIFICATION_ID_FOREGROUND_SERVICE, notification)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int
    {
        startInForeground()
        return super.onStartCommand(intent, flags, startId)
    }

    override fun onCreate() {
        super.onCreate()

        UnityCore.instance.addObserver(this)
        UnityCore.instance.startCore()
    }
    override fun onDestroy() {
        super.onDestroy()

        UnityCore.instance.removeObserver(this)
    }
}

class BootReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        if (Intent.ACTION_BOOT_COMPLETED == intent.action) {
            setupBackgroundSync(context)
        }
    }

}