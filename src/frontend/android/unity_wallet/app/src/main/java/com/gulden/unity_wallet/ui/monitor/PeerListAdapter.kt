// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.ui.monitor

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.gulden.jniunifiedbackend.PeerRecord
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.peer_list_row.view.*

class PeerListAdapter : ListAdapter<PeerRecord, PeerListAdapter.ItemViewHolder>(DiffCallback())  {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ItemViewHolder {
        return ItemViewHolder(
                LayoutInflater.from(parent.context)
                        .inflate(R.layout.peer_list_row, parent, false)
        )
    }

    override fun onBindViewHolder(holder: PeerListAdapter.ItemViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    class ItemViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        fun bind(item: PeerRecord) = with(itemView) {
            /* Concept of download peer not implemented currently
            val style = if (item.download) Typeface.DEFAULT_BOLD else Typeface.DEFAULT
            itemView.peer_list_row_height.typeface = style
            itemView.peer_list_row_version.typeface = style
            itemView.peer_list_row_protocol.typeface = style
            itemView.peer_list_row_ping.typeface = style */
            itemView.peer_list_row_ip.text = if (item.hostname.isEmpty()) item.ip else item.hostname
            itemView.peer_list_row_height.text = if (item.height > 0) item.height.toString() + " blocks" else null
            itemView.peer_list_row_user_agent.text = item.userAgent
            itemView.peer_list_row_protocol.text = item.protocol.toString()
            itemView.peer_list_row_ping.text = item.latency.toString()+"ms"
        }
    }
}

class DiffCallback : DiffUtil.ItemCallback<PeerRecord>() {
    override fun areItemsTheSame(oldItem: PeerRecord, newItem: PeerRecord): Boolean {
        return oldItem.ip == newItem.ip
    }

    override fun areContentsTheSame(oldItem: PeerRecord, newItem: PeerRecord): Boolean {
        return oldItem == newItem
    }
}
