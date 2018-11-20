// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za) & Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.app.Application
import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.OnLifecycleEvent
import android.arch.lifecycle.ProcessLifecycleOwner
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend

class ActivityManager : Application(), LifecycleObserver, UnityCore.Observer
{
    override fun onCreate()
    {
        super.onCreate()

        UnityCore.instance.configure(
                UnityConfig(dataDir = applicationContext.applicationInfo.dataDir, testnet = Constants.TEST)
        )

        UnityCore.instance.addObserver(this)
    }

    override fun onCoreReady(): Boolean {
        ProcessLifecycleOwner.get().lifecycle.addObserver(this)
        return true
    }

    override fun onCoreShutdown(): Boolean {
        ProcessLifecycleOwner.get().lifecycle.removeObserver(this)
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
