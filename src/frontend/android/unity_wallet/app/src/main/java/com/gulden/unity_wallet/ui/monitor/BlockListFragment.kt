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
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProviders
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.LinearLayoutManager
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.block_list_fragment.*
import kotlinx.android.synthetic.main.block_list_fragment.view.*
import kotlinx.coroutines.*
import kotlin.coroutines.CoroutineContext

class BlockListFragment : Fragment(), CoroutineScope {
    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View? {

        val adapter = BlockListAdapter()

        val viewModel = ViewModelProviders.of(this).get(BlockListViewModel::class.java)

        // observe changes
        viewModel.getBlocks().observe(this, Observer { blocks ->
            if (blocks.isEmpty()) {
                block_list_group.displayedChild = 0
            } else {
                block_list_group.displayedChild = 1
                adapter.submitList(blocks)
            }
        })

        // periodically update data
        this.launch {
            while (isActive) {
                val data = withContext(Dispatchers.IO) {
                    GuldenUnifiedBackend.getLastSPVBlockinfos()
                }
                viewModel.setBlocks(data)
                delay(3000)
            }
        }

        val view = inflater.inflate(R.layout.block_list_fragment, container, false)

        view.block_list.let { recycler ->
            recycler.layoutManager = LinearLayoutManager(context)
            recycler.adapter = adapter
            recycler.addItemDecoration(DividerItemDecoration(context, DividerItemDecoration.VERTICAL));
        }

        return view
    }
}
