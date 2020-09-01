// Copyright (c) 2018 The Novo developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the Novo software license, see the accompanying
// file COPYING

package com.novo.unity_wallet.ui.monitor

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import com.novo.jniunifiedbackend.BlockInfoRecord

class BlockListViewModel : ViewModel() {
    private lateinit var blocks: MutableLiveData<List<BlockInfoRecord>>

    fun getBlocks(): LiveData<List<BlockInfoRecord>> {
        if (!::blocks.isInitialized) {
            blocks = MutableLiveData()
        }
        return blocks
    }

    fun setBlocks(blocks_: List<BlockInfoRecord>) {
        if (!::blocks.isInitialized) {
            blocks = MutableLiveData()
        }
        blocks.value = blocks_
    }
}
