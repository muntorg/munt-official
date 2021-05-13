// Copyright (c) 2018 The Florin developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the Florin software license, see the accompanying
// file COPYING

package com.florin.unity_wallet.ui.monitor

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import com.florin.jniunifiedbackend.BlockInfoRecord

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
