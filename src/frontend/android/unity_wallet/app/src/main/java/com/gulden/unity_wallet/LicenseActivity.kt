// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import kotlinx.android.synthetic.main.activity_license.*
import org.apache.commons.io.IOUtils


class LicenseActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_license)
        val inStream = resources.openRawResource(R.raw.license)
        licenseTextView.text = IOUtils.toString(inStream, "utf-8")
        inStream.close()
    }

    fun onBackButtonPushed(view: View) {
        finish()
    }

}
