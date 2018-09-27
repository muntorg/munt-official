package com.gulden.unity_wallet

import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import android.view.View
import com.gulden.jniunifiedbackend.BalanceRecord
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.GuldenUnifiedFrontend
import kotlinx.android.synthetic.main.activity_main.*
import kotlin.concurrent.thread
import android.Manifest.permission.FOREGROUND_SERVICE
import android.graphics.Bitmap
import android.support.v4.app.NotificationCompat
import android.app.PendingIntent




var NOTIFICATION_ID_FOREGROUND_SERVICE = 2;

class UnityService : Service()
{

    // All signal are broadcast to main program via this handler.
    interface UnityServiceSignalHandler
    {
        fun syncProgressChanged(percent : Float) : Boolean
        fun walletBalanceChanged(balance : Long) : Boolean
    }
    public var signalHandler: UnityServiceSignalHandler? = null


    // Handle signals from core library and convert them to service signals where necessary.
    private val coreLibrarySignalHandler = object : GuldenUnifiedFrontend() {
        override fun notifySPVProgress(startHeight: Int, progessHeight: Int, expectedHeight: Int): Boolean {
            if (expectedHeight - startHeight == 0)
            {
                signalHandler?.syncProgressChanged(0f);
            }
            else
            {
                signalHandler?.syncProgressChanged((java.lang.Float.valueOf(progessHeight.toFloat()) - startHeight) / (expectedHeight - startHeight) * 100f);
            }
            return true;
        }

        override fun notifyBalanceChange(newBalance: BalanceRecord): Boolean
        {
            signalHandler?.walletBalanceChanged(newBalance.availableIncludingLocked + newBalance.immatureIncludingLocked + newBalance.unconfirmedIncludingLocked);
            return true;
        }


    }


    // Binder given to clients
    private val binder = LocalBinder()


    // Class used for the client Binder.
    inner class LocalBinder : Binder()
    {
        internal// Return this instance of MyService so clients can call public methods
        val service: UnityService get() = this@UnityService
    }

    override fun onCreate()
    {
        super.onCreate()
    }

    fun loadLibrary()
    {
        thread(true)
        {
            System.loadLibrary("gulden_unity_jni")
            GuldenUnifiedBackend.InitUnityLib(applicationContext.getApplicationInfo().dataDir, coreLibrarySignalHandler)
            libraryLoaded = true;
        }
    }

    fun unloadLibrary()
    {
        if (libraryLoaded)
        {
            GuldenUnifiedBackend.TerminateUnityLib()
            libraryLoaded = false;
        }
    }

    fun startInForeground()
    {
        loadLibrary()

        val notificationIntent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0)

        val notification = NotificationCompat.Builder(this)
                .setContentTitle("Gulden")
                .setTicker("Gulden")
                .setContentText("Gulden")
                .setSmallIcon(R.drawable.ic_g_logo)
                //.setLargeIcon(Bitmap.createScaledBitmap(R.drawable.ic_g_logo, 128, 128, false))
                .setContentIntent(pendingIntent)
                .setOngoing(true)
                .build()
        startForeground(NOTIFICATION_ID_FOREGROUND_SERVICE, notification)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int
    {
        if (intent?.getAction().equals(START_FOREGROUND_ACTION))
        {
            startInForeground();
        }

        return super.onStartCommand(intent, flags, startId)
    }

    override fun onDestroy()
    {
        unloadLibrary();
        super.onDestroy()
    }

    override fun onTaskRemoved(rootIntent: Intent)
    {
        if (closeOnAppExit)
        {
            unloadLibrary();
            stopSelf()
        }
    }

    override fun onBind(intent: Intent?): IBinder?
    {
        return binder;
    }

    var libraryLoaded = false;
    var closeOnAppExit = true;
    companion object
    {
        public var IS_SERVICE_RUNNING = false
        public var START_FOREGROUND_ACTION = "com.gulden.UnityService.startforegroundaction"
    }
}
