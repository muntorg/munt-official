// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.monitor

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import com.gulden.jniunifiedbackend.PeerRecord

class PeerListViewModel() : ViewModel() {
    private lateinit var peers: MutableLiveData<List<PeerRecord>>

    fun getPeers(): LiveData<List<PeerRecord>> {
        if (!::peers.isInitialized) {
            peers = MutableLiveData()
        }
        return peers
    }

    fun setPeers(_peers: List<PeerRecord>) {
        if (!::peers.isInitialized) {
            peers = MutableLiveData()
        }
        peers.value = _peers
    }
}
