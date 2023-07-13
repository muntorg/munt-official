// Copyright (c) 2018-2022 The Centure developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

package unity_wallet.ui.monitor

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import unity_wallet.jniunifiedbackend.PeerRecord

class PeerListViewModel : ViewModel() {
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
