// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com) & Willem de Jonge (willem@isnapp.nl)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

package unity_wallet

import android.app.*
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ProcessLifecycleOwner
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.graphics.Color
import android.os.Build
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.preference.PreferenceManager
import unity_wallet.jniunifiedbackend.ILibraryController
import unity_wallet.jniunifiedbackend.MutationRecord
import unity_wallet.jniunifiedbackend.TransactionRecord
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlin.coroutines.CoroutineContext
import kotlin.system.exitProcess
import android.os.Handler
import android.os.Looper


private const val TAG = "activity-manager"

class ActivityManager : Application(), LifecycleObserver, UnityCore.Observer, SharedPreferences.OnSharedPreferenceChangeListener, CoroutineScope {

    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    private var lastAudibleNotification = 0L

    private val handler = Handler(Looper.getMainLooper())

    override fun onCreate()
    {
        super.onCreate()

        AppContext.instance = baseContext

        val assetFD = assets?.openFd("staticfiltercp")
        UnityCore.instance.configure(
            UnityConfig(dataDir = applicationContext.applicationInfo.dataDir, apkPath = applicationContext.packageResourcePath, staticFilterOffset = assetFD?.startOffset!!, staticFilterLength = assetFD.length, testnet = Constants.TEST)
        )

        UnityCore.instance.addObserver(this, fun (callback:() -> Unit) { handler.post { callback() }})

        val preferences = PreferenceManager.getDefaultSharedPreferences(this)
        preferences.registerOnSharedPreferenceChangeListener(this)

        setupBackgroundSync(this)

        ProcessLifecycleOwner.get().lifecycle.addObserver(this)
    }

    override fun onCoreShutdown() {
        ProcessLifecycleOwner.get().lifecycle.removeObserver(this)

        //fixme: In future see if we can restart the core here
        exitProcess(-1)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        when (key) {
            "preference_background_sync" -> {
                setupBackgroundSync(this)
            }
        }
    }

    // O upwards requires notification channels
    private var notificationChannel : NotificationChannel? = null
    private fun getNotificationChannelID() : String
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
        {
            if (notificationChannel == null)
            {
                val mNotificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

                // The id of the channel.
                val id = "coin_transactions_notification_channel"

                // The user-visible name of the channel.
                val name = getString(R.string.notification_transaction_channel_name)

                // The user-visible description of the channel.
                val description = getString(R.string.notification_transaction_channel_description)

                // Default importance, adjustable by user in preferences
                val importance = NotificationManager.IMPORTANCE_HIGH

                notificationChannel = NotificationChannel(id, name, importance)

                // Sets the notification light color for notifications posted to this, if the device supports this feature.
                notificationChannel?.enableLights(true)
                notificationChannel?.lightColor = Color.GREEN

                // Sets vibration for notifications
                notificationChannel?.enableVibration(true)
                notificationChannel?.vibrationPattern = longArrayOf(0, 1000, 500, 1000)

                notificationChannel?.description = description
                mNotificationManager.createNotificationChannel(notificationChannel!!)
            }
            if (notificationChannel != null)
                return notificationChannel?.id!!
        }
        return ""
    }

    override fun onNewMutation(mutation: MutationRecord, selfCommitted: Boolean) {
        val preferences = PreferenceManager.getDefaultSharedPreferences(this)
        // only notify of mutations that are not initiated by our own payments, have a net change effect != 0
        // and when notifications are enabled in preferences
        if (preferences.getBoolean("preference_notify_transaction_activity", true)
                && !selfCommitted
                && mutation.change != 0L) {
            val notificationIntent = Intent(this, WalletActivity::class.java)
            val pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, PendingIntent.FLAG_MUTABLE)

            val title = getString(if (mutation.change > 0) R.string.notify_received else R.string.notify_sent)
            val notification = with(NotificationCompat.Builder(this)) {
                setSmallIcon(R.drawable.ic_logo)
                setContentTitle(title)
                setTicker(title)
                setContentText(formatNative(mutation.change))
                //setPublicVersion()
                //setTimeoutAfter()
                if (getNotificationChannelID() != "")
                    setChannelId(getNotificationChannelID())
                setContentIntent(pendingIntent)
                setOngoing(false)
                setAutoCancel(true)
                setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                setDefaults(Notification.DEFAULT_ALL)
                //setLargeIcon(Bitmap.createScaledBitmap(R.drawable.ic_g_logo, 128, 128, false))
                val now = System.currentTimeMillis()
                if (now - lastAudibleNotification > Config.AUDIBLE_NOTIFICATIONS_INTERVAL)
                    lastAudibleNotification = now
                else
                    setOnlyAlertOnce(true)
                build()
            }
            val notificationManager = applicationContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            notificationManager.notify(1, notification)
        }
    }

    override fun updatedTransaction(transaction: TransactionRecord) {
        Log.i(TAG, "updatedTransaction: $transaction")
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_CREATE)
    fun processCreated()
    {
        UnityCore.instance.startCore()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun allActivitiesStopped()
    {
        val deferred = UnityCore.instance.walletReady
        if (deferred.isCompleted && !deferred.isCancelled) {
            ILibraryController.PersistAndPruneForSPV()
            ILibraryController.LockWallet()
            //TODO: This lock call should be powered via core events and not directly
            Authentication.instance.lock()
        }
    }
}
