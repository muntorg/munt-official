// Copyright (c) 2018-2019 The Florin developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the Florin software license, see the accompanying
// file COPYING

package com.florin.unity_wallet.main_activity_fragments

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Intent
import android.graphics.Bitmap
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.core.content.ContextCompat.getSystemService
import com.florin.jniunifiedbackend.ILibraryController
import com.florin.unity_wallet.R
import com.florin.unity_wallet.WalletActivity
import com.florin.unity_wallet.util.AppBaseFragment
import kotlinx.android.synthetic.main.fragment_receive.*
import org.jetbrains.anko.dimen
import org.jetbrains.anko.sdk27.coroutines.onLongClick
import java.nio.ByteBuffer


/* Handle display of current address; as well as copying/sharing of address */
class ReceiveFragment : AppBaseFragment() {

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        return inflater.inflate(R.layout.fragment_receive, container, false)
    }

    override fun onWalletReady() {
        updateAddress()
        buyButton.setOnClickListener { (activity as WalletActivity).gotoBuyActivity() }
        shareButton.setOnClickListener {
            val share = Intent(Intent.ACTION_SEND)
            share.type = "text/plain"
            share.putExtra(Intent.EXTRA_TEXT, currentAddressLabel.text)
            startActivity(Intent.createChooser(share, getString(R.string.receive_fragment_share_title)))
        }

        currentAddressQrView.onLongClick {
            copyToClipboard()
        }

        currentAddressLabel.onLongClick {
            copyToClipboard()
        }
    }

    private fun copyToClipboard() {
        val clip: ClipData = ClipData.newPlainText(getString(R.string.coin_address_clipboard_label), currentAddressLabel.text)
        val clipboard = getSystemService<ClipboardManager>(activity!!, ClipboardManager::class.java) as ClipboardManager
        clipboard.setPrimaryClip(clip)

        val toast = Toast.makeText(context, getString(R.string.copied_to_clipboard_toast), Toast.LENGTH_SHORT)
        toast.setGravity(Gravity.TOP, 0, context!!.dimen(R.dimen.top_toast_offset) )
        toast.show()
    }

    private fun updateAddress()
    {
        if (currentAddressQrView != null)
        {
            val address = ILibraryController.GetReceiveAddress()
            val imageData = ILibraryController.QRImageFromString("florin://$address", 600)
            val bitmap = Bitmap.createBitmap(imageData.width, imageData.width, Bitmap.Config.ALPHA_8)
            bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(imageData.pixelData))
            currentAddressQrView.setImageBitmap(bitmap)
            currentAddressLabel.text = address
        }
    }
}
