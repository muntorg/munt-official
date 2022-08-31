// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package unity_wallet.ui.monitor

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.lifecycle.ViewModelProviders
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.LinearLayoutManager
import unity_wallet.jniunifiedbackend.MonitorListener
import unity_wallet.jniunifiedbackend.ILibraryController
import unity_wallet.R
import unity_wallet.UnityCore
import unity_wallet.util.AppBaseFragment
import kotlinx.android.synthetic.main.block_list_fragment.*
import kotlinx.android.synthetic.main.block_list_fragment.view.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class BlockListFragment : AppBaseFragment() {
    private lateinit var viewModel: BlockListViewModel

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View? {

        val adapter = BlockListAdapter()

        viewModel = ViewModelProviders.of(this).get(BlockListViewModel::class.java)

        // observe changes
        viewModel.getBlocks().observe(viewLifecycleOwner, { blocks ->
            if (blocks.isEmpty()) {
                block_list_group.displayedChild = 0
            } else {
                block_list_group.displayedChild = 1
                adapter.submitList(blocks)
            }
        })

        val view = inflater.inflate(R.layout.block_list_fragment, container, false)

        view.block_list.let { recycler ->
            recycler.layoutManager = LinearLayoutManager(context)
            recycler.adapter = adapter
            recycler.addItemDecoration(DividerItemDecoration(context, DividerItemDecoration.VERTICAL))
        }

        return view
    }

    override fun onWalletReady() {
        launch(Dispatchers.Main) {
            // update data once
            updateBlocks()
        }
    }

    private suspend fun updateBlocks() {
        val data = withContext(Dispatchers.IO) {
            ILibraryController.getLastSPVBlockInfos()
        }
        viewModel.setBlocks(data)
    }

    override fun onResume() {
        super.onResume()

        UnityCore.instance.addMonitorObserver(monitoringListener)
    }

    override fun onPause() {
        UnityCore.instance.removeMonitorObserver(monitoringListener)

        super.onPause()
    }

    private val monitoringListener = object: MonitorListener() {
        override fun onPruned(height: Int) {}

        override fun onProcessedSPVBlocks(height: Int) {}

        override fun onPartialChain(height: Int, probableHeight: Int, offset: Int) {
            this@BlockListFragment.launch(Dispatchers.Main) {
                updateBlocks()
            }
        }
    }

}
