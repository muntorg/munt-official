// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.MainActivityFragments

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.support.v4.app.Fragment

import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import android.graphics.Bitmap
import android.support.v4.view.MenuItemCompat
import android.support.v7.app.AppCompatActivity
import android.support.v7.view.ActionMode
import android.support.v7.widget.ShareActionProvider
import android.text.Spannable
import android.text.SpannableString
import android.view.*
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.fragment_receive.*
import java.nio.ByteBuffer


// TODO: Rename parameter arguments, choose names that match
// the fragment initialization parameters, e.g. ARG_ITEM_NUMBER
private const val ARG_PARAM1 = "param1"
private const val ARG_PARAM2 = "param2"

/* Handle display of current address; as well as copying/sharing of address */
class ReceiveFragment : Fragment()
{
    // TODO: Rename and change types of parameters
    private var param1: String? = null
    private var param2: String? = null
    private var listener: OnFragmentInteractionListener? = null

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
        arguments?.let {
            param1 = it.getString(ARG_PARAM1)
            param2 = it.getString(ARG_PARAM2)
        }
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        var inflated = inflater.inflate(R.layout.fragment_receive, container, false)

        return inflated;
    }

    override fun onActivityCreated(savedInstanceState: Bundle?)
    {
        super.onActivityCreated(savedInstanceState)

        updateAddress();
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
        listener = null
    }

    interface OnFragmentInteractionListener
    {
        // TODO: Update argument type and name
        fun onFragmentInteraction(uri: Uri)
    }

    companion object
    {
        /**
         * Use this factory method to create a new instance of
         * this fragment using the provided parameters.
         *
         * @param param1 Parameter 1.
         * @param param2 Parameter 2.
         * @return A new instance of fragment ReceiveFragment.
         */
        // TODO: Rename and change types and number of parameters
        @JvmStatic fun newInstance(param1: String, param2: String) =
                ReceiveFragment().apply {
                    arguments = Bundle().apply {
                        putString(ARG_PARAM1, param1)
                        putString(ARG_PARAM2, param2)
                    }
                }
    }

    public fun updateAddress()
    {
        if (currentAddressQrView != null)
        {
            var address = GuldenUnifiedBackend.GetReceiveAddress();
            var imageData = GuldenUnifiedBackend.QRImageFromString("gulden://"+address, 600)
            val bitmap = Bitmap.createBitmap(imageData.width, imageData.width, Bitmap.Config.ALPHA_8);
            bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(imageData.pixelData))
            currentAddressQrView.setImageBitmap(bitmap)
            currentAddressLabel.setText(address)
            updateShareActionProvider(address);
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
                //BuyActivity.start(activity, currentAddressString)
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
            shareActionProvider = ShareActionProvider(activity)
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

    public fun setFocusOnAddress()
    {
        if (actionBarCallback == null)
        {
            actionBarCallback = ActionBarCallBack()
            storedMode = (activity as AppCompatActivity).startSupportActionMode(ActionBarCallBack())
        }
        else
        {
            dismissActionBar()
        }
    }

    private fun dismissActionBar()
    {
        if (storedMode != null) storedMode!!.finish()
        shareActionProvider = null
        actionBarCallback = null
    }
}
