// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.monitor

import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import com.google.android.material.bottomnavigation.BottomNavigationView
import com.gulden.unity_wallet.*
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.coroutines.*
import kotlin.coroutines.CoroutineContext

class NetworkMonitorActivity : UnityCore.Observer, AppCompatActivity(), CoroutineScope
{
    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    private val coreObserverProxy = CoreObserverProxy(this, this)

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_network_monitor)

        navigation.setOnNavigationItemSelectedListener(mOnNavigationItemSelectedListener)

        syncProgress.max = 1000000

        if (peersFragment == null)
            peersFragment = PeerListFragment()
        addFragment(peersFragment!!, R.id.networkMonitorMainLayout)
    }

    override fun onDestroy() {
        super.onDestroy()

        coroutineContext[Job]!!.cancel()
    }

    override fun onStart() {
        super.onStart()

        syncProgress.progress = 0
        UnityCore.instance.addObserver(coreObserverProxy)
    }

    override fun onStop() {
        super.onStop()
        UnityCore.instance.removeObserver(coreObserverProxy)
    }

    fun onBackButtonPushed(view : View) {
        finish()
    }

    private fun gotoPeersPage()
    {
        if (peersFragment == null)
            peersFragment = PeerListFragment()
        replaceFragment(peersFragment!!, R.id.networkMonitorMainLayout)
    }

    fun gotoBlocksPage()
    {
        if (blocksFragment == null)
            blocksFragment = BlockListFragment()
        replaceFragment(blocksFragment!!, R.id.networkMonitorMainLayout)
    }

    private fun gotoProcessingPage()
    {
        if (processingFragment == null)
            processingFragment = ProcessingFragment()
        replaceFragment(processingFragment!!, R.id.networkMonitorMainLayout)
    }

    private var blocksFragment : BlockListFragment ?= null
    private var peersFragment : PeerListFragment?= null
    private var processingFragment : ProcessingFragment?= null


    private val mOnNavigationItemSelectedListener = BottomNavigationView.OnNavigationItemSelectedListener { item ->
        when (item.itemId) {
            R.id.navigation_peers -> { gotoPeersPage(); return@OnNavigationItemSelectedListener true }
            R.id.navigation_blocks -> { gotoBlocksPage(); return@OnNavigationItemSelectedListener true }
            R.id.navigation_processing -> { gotoProcessingPage(); return@OnNavigationItemSelectedListener true }
        }
        false
    }


    override fun syncProgressChanged(percent: Float): Boolean {
        setSyncProgress(percent)
        return true
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
