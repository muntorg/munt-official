// Copyright (c) 2018-2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.monitor

import android.os.Bundle
import android.view.View
import com.google.android.material.bottomnavigation.BottomNavigationView
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.UnityCore
import com.gulden.unity_wallet.util.AppBaseActivity
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.coroutines.Job

class NetworkMonitorActivity : UnityCore.Observer, AppBaseActivity()
{
    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_network_monitor)

        navigation.setOnNavigationItemSelectedListener(mOnNavigationItemSelectedListener)

        syncProgress.max = 1000000
    }

    override fun onDestroy() {
        super.onDestroy()

        coroutineContext[Job]!!.cancel()
    }

    override fun onStart() {
        super.onStart()

        UnityCore.instance.addObserver(this, fun (callback:() -> Unit) { runOnUiThread { callback() }})

        if (supportFragmentManager.fragments.isEmpty()) {
            addFragment(PeerListFragment(), R.id.networkMonitorMainLayout)
        }
    }

    override fun onStop() {
        super.onStop()
        UnityCore.instance.removeObserver(this)
    }

    override fun onWalletReady() {
        setSyncProgress(UnityCore.instance.progressPercent)
    }

    fun onBackButtonPushed(view : View) {
        finish()
    }

    private val mOnNavigationItemSelectedListener = BottomNavigationView.OnNavigationItemSelectedListener { item ->
        val page = when (item.itemId) {
            R.id.navigation_peers -> PeerListFragment()
            R.id.navigation_blocks -> BlockListFragment()
            R.id.navigation_processing -> ProcessingFragment()
            else -> return@OnNavigationItemSelectedListener false
        }
        gotoFragment(page, R.id.networkMonitorMainLayout)
        true
    }


    override fun syncProgressChanged(percent: Float) {
        setSyncProgress(percent)
    }

    private fun setSyncProgress(percent: Float)
    {
        syncProgress.progress = (syncProgress.max * (percent/100)).toInt()
        if (percent < 100.0)
            syncProgress.visibility = View.VISIBLE
        else
            syncProgress.visibility = View.INVISIBLE
    }
}
