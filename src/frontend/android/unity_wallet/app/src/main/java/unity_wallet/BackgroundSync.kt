// Copyright (c) 2018-2022 The Centure developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

package unity_wallet

import android.app.*
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import android.util.Log
import android.content.BroadcastReceiver
import android.os.SystemClock.sleep
import androidx.preference.PreferenceManager
import androidx.core.content.ContextCompat
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.Worker
import androidx.work.WorkerParameters
import kotlinx.coroutines.*
import android.os.Handler
import android.os.Looper
import androidx.annotation.RequiresApi
import java.util.concurrent.TimeUnit
import kotlin.coroutines.CoroutineContext

private const val TAG = "background_sync"
const val APP_PERIODIC_SYNC = "APP_PERIODIC_SYNC"

fun setupBackgroundSync(context: Context) {

    // get syncType from preferences
    val sharedPreferences = PreferenceManager.getDefaultSharedPreferences(context)
    val syncType = sharedPreferences.getString("preference_background_sync", context.getString(R.string.background_sync_default))!!

    Log.i(TAG, "Starting background sync: $syncType")

    val serviceIntent = Intent(context, SyncService::class.java)

    context.stopService(serviceIntent)
    WorkManager.getInstance().cancelAllWorkByTag(APP_PERIODIC_SYNC)

    when (syncType) {
        "BACKGROUND_SYNC_OFF" -> {
        }

        "BACKGROUND_SYNC_DAILY" -> {
            val work = PeriodicWorkRequestBuilder<SyncWorker>(24, TimeUnit.HOURS)
                    .addTag(APP_PERIODIC_SYNC)
                    .build()
            WorkManager.getInstance().enqueue(work)
        }

        "BACKGROUND_SYNC_CONTINUOUS" -> {
            var tryForeground = true
            if (Build.MANUFACTURER == "samsung")
                tryForeground = false

            if (tryForeground) {
                try {
                    ContextCompat.startForegroundService(context, serviceIntent)
                } catch (e: Exception) {
                    tryForeground = false
                }
            }
            if(!tryForeground) {
                //fixme: (HIGH) - as of API 32 the above is "illegal" and leads to ForegroundServiceStartNotAllowedException
                //see: https://stackoverflow.com/questions/69604951/getting-android-app-foregroundservicestartnotallowedexception-in-android-12-sdk
                //
                //For now instead we just call the periodic sync a lot more frequently, however I don't know if this is the best solution, we should look at something else in future
                val work = PeriodicWorkRequestBuilder<SyncWorker>(10, TimeUnit.MINUTES)
                        .addTag(APP_PERIODIC_SYNC)
                        .build()
                WorkManager.getInstance().enqueue(work)
            }
        }
    }
}


private var NOTIFICATION_ID_FOREGROUND_SERVICE = 2

class SyncService : Service(), UnityCore.Observer
{
    override fun onBind(intent: Intent?): IBinder? {
        TODO("not implemented") //To change body of created functions use File | Settings | File Templates.
    }

    override fun syncProgressChanged(percent: Float) {
        Log.i(TAG, "sync progress = $percent")
        if (builder != null) {
            val b: NotificationCompat.Builder = builder!!
            builder!!.setContentText("progress $percent")
            val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            notificationManager.notify(NOTIFICATION_ID_FOREGROUND_SERVICE, b.build())
        }
    }

    private var builder: NotificationCompat.Builder? = null

    private val channelID = "unity_wallet.service.channel"

    private val handler = Handler(Looper.getMainLooper())

    private fun startInForeground()
    {

        val notificationIntent = Intent(this, WalletActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, PendingIntent.FLAG_MUTABLE)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
        {
            val notificationChannel = NotificationChannel(channelID, getString(R.string.notification_service_channel_name), NotificationManager.IMPORTANCE_LOW)
            (getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager).createNotificationChannel(notificationChannel)
            notificationChannel.description = getString(R.string.notification_service_channel_description)
            notificationChannel.enableLights(false)
            notificationChannel.setShowBadge(false)
        }


        builder = NotificationCompat.Builder(this, channelID)
                .setContentTitle("Munt")
                .setTicker("Munt")
                .setContentText("Munt")
                .setSmallIcon(R.drawable.ic_logo)
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

        UnityCore.instance.addObserver(this, fun (callback:() -> Unit) { handler.post { callback() }})
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

class SyncWorker(context : Context, params : WorkerParameters)
    : UnityCore.Observer, Worker(context, params), CoroutineScope {

    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    override fun syncProgressChanged(percent: Float) {
        Log.i(TAG, "periodic sync progress = $percent")
    }

    override fun doWork(): Result {
        Log.i(TAG, "Periodic sync started")

        UnityCore.instance.addObserver(this)
        UnityCore.instance.startCore()

        // wait for wallet ready (or get killed by the OS which ever comes first)
        try {
            runBlocking {
                UnityCore.instance.walletReady.await()
            }
        }
        catch (e: CancellationException) {
            return Result.retry()
        }

        // wait until sync is complete (or get killed by the OS which ever comes first)
        while (UnityCore.instance.progressPercent < 100.0) {
            sleep(5000)
        }

        Log.i(TAG, "Periodic sync finished")

        // Indicate success or failure with your return value:
        return Result.success()

        // (Returning RETRY tells WorkManager to try this task again
        // later; FAILURE says not to try again.)
    }
}
