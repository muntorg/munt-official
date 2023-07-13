// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

package unity_wallet.ui.monitor

import android.os.Bundle
import android.view.View
import com.google.android.material.bottomnavigation.BottomNavigationView
import unity_wallet.R
import unity_wallet.UnityCore
import unity_wallet.util.AppBaseActivity
import kotlinx.android.synthetic.main.activity_main.navigation
import kotlinx.android.synthetic.main.activity_main.syncProgress
import kotlinx.android.synthetic.main.activity_network_monitor.*
import kotlinx.coroutines.Job

class NetworkMonitorActivity : UnityCore.Observer, AppBaseActivity()
{
    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_network_monitor)

        navigation.setOnNavigationItemSelectedListener(mOnNavigationItemSelectedListener)

        syncProgress.max = 1000000

        networkMonitorBackButton.setOnClickListener{  onBackButtonPushed(it) }
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

    private fun onBackButtonPushed(view : View) {
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
