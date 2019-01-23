/*
 * Copyright 2013-2015 the original author or authors.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

package com.gulden.unity_wallet.ui.monitor

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentPagerAdapter
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.network_monitor_onepane.*

/**
 * @author Andreas Schildbach
 */
class NetworkMonitorActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.network_monitor_onepane)

        network_monitor_pager.let { pager ->
            network_monitor_pager_tabs.addTabLabels(
                    R.string.network_monitor_peer_list_title,
                    R.string.network_monitor_block_list_title,
                    R.string.network_monitor_processing_title
                    )

            pager.adapter = PagerAdapter(supportFragmentManager)
            pager.setOnPageChangeListener(network_monitor_pager_tabs)
            pager.pageMargin = 2
            pager.setPageMarginDrawable(R.color.bg_less_bright)
        }
    }

    private class PagerAdapter(fm: FragmentManager) : FragmentPagerAdapter(fm) {

        override fun getCount(): Int {
            return 3
        }

        override fun getItem(position: Int): Fragment {
            return when (position) {
                0 -> PeerListFragment()
                1 -> BlockListFragment()
                2 -> ProcessingFragment()
                else -> {
                    throw Exception("No such fragment page!")
                }
            }
        }
    }
}
