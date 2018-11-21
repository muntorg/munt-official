package com.gulden.unity_wallet

import android.util.Log

private val TAG = "backgroundsync"

fun setupBackgroundSync(syncType: String) {
    // TODO: tear down whatever background sync that has been setup

    Log.i(TAG, "Starting background sync: " + syncType)

    when (syncType) {
        "BACKGROUND_SYNC_OFF" -> {
            // nothing to be done here
        }

        "BACKGROUND_SYNC_DAILY" -> {
            TODO(reason = "BACKGROUND_SYNC_DAILY")
        }

        "BACKGROUND_SYNC_CONTINUOUS" -> {
            TODO(reason = "BACKGROUND_SYNC_CONTINUOUS not implemented")
        }
    }
}