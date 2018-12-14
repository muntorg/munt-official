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
import com.gulden.unity_wallet.UnityCore
import kotlinx.android.synthetic.main.peer_list_fragment.*
import kotlinx.android.synthetic.main.peer_list_fragment.view.*
import kotlinx.coroutines.*
import kotlin.coroutines.CoroutineContext

class PeerListFragment : Fragment(), CoroutineScope {
    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View? {

        val adapter = PeerListAdapter()

        val viewModel = ViewModelProviders.of(this).get(PeerListViewModel::class.java)

        // observe peer changes
        viewModel.getPeers().observe(this, Observer { peers ->
            if (peers.isEmpty()) {
                peer_list_group.displayedChild = 1
            } else {
                peer_list_group.displayedChild = 2
                adapter.submitList(peers)
            }
        })

        // periodically update peers
        this.launch {
            while (isActive) {
                val peers = withContext(Dispatchers.IO) {
                    GuldenUnifiedBackend.getPeers()
                }
                viewModel.setPeers(peers)
                delay(3000)
            }
        }

        val view = inflater.inflate(R.layout.peer_list_fragment, container, false)

        view.peer_list.let { recycler ->
            recycler.layoutManager = LinearLayoutManager(context)
            recycler.adapter = adapter
            recycler.addItemDecoration(DividerItemDecoration(context, DividerItemDecoration.VERTICAL));
        }

        return view
    }
}
