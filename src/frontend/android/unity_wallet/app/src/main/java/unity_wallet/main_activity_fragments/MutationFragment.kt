// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package unity_wallet.main_activity_fragments

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import jniunifiedbackend.ILibraryController
import jniunifiedbackend.MutationRecord
import jniunifiedbackend.TransactionRecord
import unity_wallet.*
import unity_wallet.ui.MutationAdapter
import unity_wallet.util.AppBaseFragment
import unity_wallet.util.invokeNowOrOnSuccessfulCompletion
import kotlinx.android.synthetic.main.fragment_mutation.*
import kotlinx.coroutines.*
import android.os.Handler
import android.os.Looper


class MutationFragment : AppBaseFragment(), UnityCore.Observer, CoroutineScope {

    private val handler = Handler(Looper.getMainLooper())

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        return inflater.inflate(R.layout.fragment_mutation, container, false)
    }

    override fun onActivityCreated(savedInstanceState: Bundle?)
    {
        super.onActivityCreated(savedInstanceState)

        // if wallet ready setup list immediately so list does not briefly flash empty content and
        // scroll position is kept on switching from and to this fragment
        UnityCore.instance.walletReady.invokeNowOrOnSuccessfulCompletion(this) {
            mutationList?.emptyView = emptyMutationListView
            val mutations = ILibraryController.getMutationHistory()

            val adapter = MutationAdapter(this@MutationFragment.context!!, mutations)
            mutationList.adapter = adapter

            mutationList.setOnItemClickListener { parent, _, position, _ ->
                val mutation = parent.adapter.getItem(position) as MutationRecord
                val intent = Intent(this@MutationFragment.context, TransactionInfoActivity::class.java)
                intent.putExtra(TransactionInfoActivity.EXTRA_TRANSACTION, mutation.txHash)
                startActivity(intent)
            }

            launch {
                try {
                    (mutationList.adapter as MutationAdapter).updateRate(fetchCurrencyRate(localCurrency.code))
                } catch (e: Throwable) {
                    // silently ignore failure of getting rate here
                }
            }
        }
    }

    override fun onAttach(context: Context)
    {
        UnityCore.instance.addObserver(this)

        super.onAttach(context)
    }

    override fun onDetach() {
        UnityCore.instance.removeObserver(this)

        super.onDetach()
    }

    override fun onNewMutation(mutation: MutationRecord, selfCommitted: Boolean) {
        //TODO: Update only the single mutation we have received
        val mutations = ILibraryController.getMutationHistory()
        handler.post {
            val adapter = mutationList.adapter as MutationAdapter
            adapter.updateDataSource(mutations)
        }
    }

    override fun updatedTransaction(transaction: TransactionRecord)
    {
        //TODO: Update only the single mutation we have received
        val mutations = ILibraryController.getMutationHistory()
        handler.post {
            val adapter = mutationList.adapter as MutationAdapter
            adapter.updateDataSource(mutations)
        }
    }
}
