/*
 * Copyright (C) The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This file contains modifications by The Gulden developers
 * All modifications copyright (C) The Gulden developers
 */
package com.gulden.barcodereader

import android.content.Context
import androidx.annotation.UiThread

import com.google.android.gms.vision.Tracker
import com.google.android.gms.vision.barcode.Barcode


// Monitor for new barcode detections and alert via BarcodeUpdateListener interface.
class BarcodeTracker internal constructor(context: Context) : Tracker<Barcode>()
{

    private var mBarcodeUpdateListener: BarcodeUpdateListener? = null

    interface BarcodeUpdateListener
    {
        @UiThread
        fun onBarcodeDetected(barcode: Barcode?)
    }

    init
    {
        if (context is BarcodeUpdateListener)
        {
            this.mBarcodeUpdateListener = context
        }
        else
        {
            throw RuntimeException("Hosting activity must implement BarcodeUpdateListener")
        }
    }

    override fun onNewItem(id: Int, item: Barcode?)
    {
        mBarcodeUpdateListener?.onBarcodeDetected(item)
    }
}