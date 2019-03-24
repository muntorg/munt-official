// Copyright (c) 2018-2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet.main_activity_fragments

import android.content.Intent
import android.graphics.Bitmap
import android.os.Bundle
import android.view.*
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.WalletActivity
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
        buyButton.setOnClickListener { (activity as WalletActivity).gotoBuyActivity() }
        shareButton.setOnClickListener {
            val share = Intent(Intent.ACTION_SEND)
            share.type = "text/plain"
            share.putExtra(Intent.EXTRA_TEXT, currentAddressLabel.text)
            startActivity(Intent.createChooser(share, getString(R.string.receive_fragment_share_title)))
        }
    }

    fun updateAddress()
    {
        if (currentAddressQrView != null)
        {
            val address = GuldenUnifiedBackend.GetReceiveAddress()
            val imageData = GuldenUnifiedBackend.QRImageFromString("gulden://$address", 600)
            val bitmap = Bitmap.createBitmap(imageData.width, imageData.width, Bitmap.Config.ALPHA_8)
            bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(imageData.pixelData))
            currentAddressQrView.setImageBitmap(bitmap)
            currentAddressLabel.text = address
        }
    }
}
