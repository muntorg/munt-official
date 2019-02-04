// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.monitor

import android.content.Intent
import android.net.Uri
import android.text.format.DateUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.PopupMenu
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.gulden.jniunifiedbackend.BlockInfoRecord
import com.gulden.unity_wallet.Config
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.block_row.view.*
import org.jetbrains.anko.sdk27.coroutines.onClick

class BlockListAdapter : ListAdapter<BlockInfoRecord, BlockListAdapter.ItemViewHolder>(BlockDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ItemViewHolder {
        return ItemViewHolder(
                LayoutInflater.from(parent.context)
                        .inflate(R.layout.block_row, parent, false)
        )
    }

    override fun onBindViewHolder(holder: BlockListAdapter.ItemViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    class ItemViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        fun bind(item: BlockInfoRecord) = with(itemView) {
            itemView.block_list_row_height.text = item.height.toString()
            val ms = item.timeStamp * DateUtils.SECOND_IN_MILLIS
            itemView.block_list_row_time.text =
                    if (ms < System.currentTimeMillis() - DateUtils.MINUTE_IN_MILLIS) {
                        DateUtils.getRelativeDateTimeString(context, ms, DateUtils.MINUTE_IN_MILLIS, DateUtils.WEEK_IN_MILLIS, 0).toString()
                    } else
                        context.getString(R.string.block_row_now)
            itemView.block_list_row_hash.text = item.blockHash
            itemView.block_list_row_menu.onClick {
                val popupMenu = PopupMenu(context, itemView.block_list_row_menu)
                popupMenu.inflate(R.menu.blocks_context)
                popupMenu.setOnMenuItemClickListener { menuItem ->
                    when (menuItem.itemId) {
                        R.id.blocks_context_browse -> {
                            val intent = Intent(Intent.ACTION_VIEW, Uri.withAppendedPath(Config.BLOCK_EXPLORER, "/block/" + item.blockHash))
                            context.startActivity(intent)
                        }
                    }
                    return@setOnMenuItemClickListener true
                }
                popupMenu.show()
            }
        }
    }
}

private class BlockDiffCallback : DiffUtil.ItemCallback<BlockInfoRecord>() {
    override fun areItemsTheSame(oldItem: BlockInfoRecord, newItem: BlockInfoRecord): Boolean {
        return oldItem.blockHash == newItem.blockHash
    }

    override fun areContentsTheSame(oldItem: BlockInfoRecord, newItem: BlockInfoRecord): Boolean {
        return oldItem == newItem
    }
}
