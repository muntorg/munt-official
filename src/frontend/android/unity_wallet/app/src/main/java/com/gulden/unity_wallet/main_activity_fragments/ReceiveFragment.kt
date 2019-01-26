// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.main_activity_fragments

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle

import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import android.graphics.Bitmap
import androidx.core.view.MenuItemCompat
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.view.ActionMode
import androidx.appcompat.widget.ShareActionProvider
import android.view.*
import com.gulden.unity_wallet.WalletActivity
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.fragment_receive.*
import java.nio.ByteBuffer


/* Handle display of current address; as well as copying/sharing of address */
class ReceiveFragment : androidx.fragment.app.Fragment()
{

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        return inflater.inflate(R.layout.fragment_receive, container, false)
    }

    override fun onActivityCreated(savedInstanceState: Bundle?)
    {
        super.onActivityCreated(savedInstanceState)

        updateAddress()
        currentAddressQrView!!.setOnClickListener { setFocusOnAddress() }
        currentAddressLabel!!.setOnClickListener { setFocusOnAddress() }
    }

    override fun onAttach(context: Context)
    {
        super.onAttach(context)
        if (context is OnFragmentInteractionListener)
        {
            listener = context
        }
        else
        {
            throw RuntimeException(context.toString() + " must implement OnFragmentInteractionListener")
        }
    }

    override fun onDetach()
    {
        super.onDetach()
        dismissActionBar()
        listener = null
    }

    private var listener: OnFragmentInteractionListener? = null
    interface OnFragmentInteractionListener
    {
        fun onFragmentInteraction(uri: Uri)
    }

    fun updateAddress()
    {
        if (currentAddressQrView != null)
        {
            var address = GuldenUnifiedBackend.GetReceiveAddress()
            var imageData = GuldenUnifiedBackend.QRImageFromString("gulden://"+address, 600)
            val bitmap = Bitmap.createBitmap(imageData.width, imageData.width, Bitmap.Config.ALPHA_8)
            bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(imageData.pixelData))
            currentAddressQrView.setImageBitmap(bitmap)
            currentAddressLabel.text = address
            updateShareActionProvider(address)
        }
    }

    private var shareActionProvider: ShareActionProvider? = null
    private var actionBarCallback: ActionBarCallBack? = null
    private var storedMode: ActionMode? = null

    private fun updateShareActionProvider(stringValue: String)
    {
        if (shareActionProvider != null)
        {
            val intent = Intent(Intent.ACTION_SEND)
            intent.type = "text/plain"
            intent.putExtra(Intent.EXTRA_TEXT, stringValue)
            shareActionProvider?.setShareIntent(intent)
        }
    }


    internal inner class ActionBarCallBack : ActionMode.Callback
    {
        override fun onActionItemClicked(mode: ActionMode, item: MenuItem): Boolean
        {
            if (item.itemId == R.id.item_copy_to_clipboard)
            {
                val clipboard = activity?.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                val clip = ClipData.newPlainText("address", currentAddressLabel.text)
                clipboard.primaryClip = clip
                mode.finish()
                return true
            }
            else if (item.itemId == R.id.item_buy_gulden)
            {
                (activity as WalletActivity).gotoBuyActivity()
                return true
            }

            return false
        }


        override fun onCreateActionMode(mode: ActionMode, menu: Menu): Boolean
        {
            mode.menuInflater.inflate(R.menu.share_menu, menu)

            // Handle buy button
            val itemBuy = menu.findItem(R.id.item_buy_gulden)
            itemBuy.isVisible = true

            // Handle copy button
            //MenuItem itemCopy = menu.findItem(R.id.item_copy_to_clipboard);

            // Handle share button
            val itemShare = menu.findItem(R.id.action_share)
            shareActionProvider = MenuItemCompat.getActionProvider(itemShare) as ShareActionProvider
            MenuItemCompat.setActionProvider(itemShare, shareActionProvider)

            updateAddress()

            return true
        }

        override fun onDestroyActionMode(mode: ActionMode)
        {
            shareActionProvider = null
            updateAddress()
        }

        override fun onPrepareActionMode(mode: ActionMode, menu: Menu): Boolean
        {
            return false
        }
    }

    fun setFocusOnAddress()
    {
        if (actionBarCallback == null)
        {
            actionBarCallback = ActionBarCallBack()
            storedMode = (activity as AppCompatActivity).startSupportActionMode(actionBarCallback!!)
        }
        else
        {
            dismissActionBar()
        }
    }

    private fun dismissActionBar()
    {
        if (storedMode != null)
        {
            storedMode!!.finish()
        }
        shareActionProvider = null
        actionBarCallback = null
    }
}
