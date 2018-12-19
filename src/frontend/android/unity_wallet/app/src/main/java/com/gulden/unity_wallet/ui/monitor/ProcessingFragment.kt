// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.monitor

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.gulden.jniunifiedbackend.GuldenMonitorListener
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.processing_fragment.*
import kotlinx.android.synthetic.main.processing_fragment.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import org.jetbrains.anko.support.v4.runOnUiThread
import kotlin.coroutines.CoroutineContext

class ProcessingFragment : Fragment(), CoroutineScope {
    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.processing_fragment, container, false)

        val stats = GuldenUnifiedBackend.getMonitoringStats()
        with (stats) {
            view.processing_group_probable.text = probableHeight.toString()
            view.processing_group_height.text = partialHeight.toString()
            view.processing_group_processed.text = processedSPVHeight.toString()
            view.processing_group_offset.text = partialOffset.toString()
            view.processing_group_length.text = (partialHeight - partialOffset).toString()
            view.processing_group_prune.text = prunedHeight.toString()
        }

        return view
    }

    override fun onResume() {
        super.onResume()

        GuldenUnifiedBackend.RegisterMonitorListener(monitoringListener)
    }

    override fun onPause() {
        GuldenUnifiedBackend.UnregisterMonitorListener(monitoringListener)

        super.onPause()
    }

    private val monitoringListener = object: GuldenMonitorListener() {
        override fun onPruned(height: Int) {
            runOnUiThread {
                this@ProcessingFragment.processing_group_prune?.text = height.toString()
            }
        }

        override fun onProcessedSPVBlocks(height: Int) {
            runOnUiThread {
                this@ProcessingFragment.processing_group_processed?.text = height.toString()
            }
        }

        override fun onPartialChain(height: Int, probableHeight: Int, offset: Int) {
            runOnUiThread {
                this@ProcessingFragment.processing_group_probable?.text = probableHeight.toString()
                this@ProcessingFragment.processing_group_height?.text = height.toString()
                this@ProcessingFragment.processing_group_offset?.text = offset.toString()
                this@ProcessingFragment.processing_group_length?.text = (height - offset).toString()
            }
        }
    }
}
