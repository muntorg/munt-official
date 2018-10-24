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

import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF

import com.google.android.gms.vision.barcode.Barcode

/**
 * Graphic instance for rendering barcode position, size, and ID within an associated graphic
 * overlay view.
 */
class BarcodeGraphic internal constructor(overlay: GraphicOverlay<*>) : GraphicOverlay.Graphic(overlay)
{

    var id: Int = 0

    private val mRectPaint: Paint
    private val mTextPaint: Paint
    @Volatile
    var barcode: Barcode? = null
        private set

    init
    {

        mCurrentColorIndex = (mCurrentColorIndex + 1) % COLOR_CHOICES.size
        val selectedColor = COLOR_CHOICES[mCurrentColorIndex]

        mRectPaint = Paint()
        mRectPaint.color = selectedColor
        mRectPaint.style = Paint.Style.STROKE
        mRectPaint.strokeWidth = 4.0f

        mTextPaint = Paint()
        mTextPaint.color = selectedColor
        mTextPaint.textSize = 36.0f
    }

    /**
     * Updates the barcode instance from the detection of the most recent frame.  Invalidates the
     * relevant portions of the overlay to trigger a redraw.
     */
    internal fun updateItem(barcode: Barcode)
    {
        this.barcode = barcode
        postInvalidate()
    }

    /**
     * Draws the barcode annotations for position, size, and raw value on the supplied canvas.
     */
    override fun draw(canvas: Canvas)
    {
        val barcode = this.barcode ?: return

        // Draws the bounding box around the barcode.
        val rect = RectF(barcode.boundingBox)
        rect.left = translateX(rect.left)
        rect.top = translateY(rect.top)
        rect.right = translateX(rect.right)
        rect.bottom = translateY(rect.bottom)
        canvas.drawRect(rect, mRectPaint)

        // Draws a label at the bottom of the barcode indicate the barcode value that was detected.
        canvas.drawText(barcode.rawValue, rect.left, rect.bottom, mTextPaint)
    }

    companion object
    {

        private val COLOR_CHOICES = intArrayOf(Color.BLUE, Color.CYAN, Color.GREEN)

        private var mCurrentColorIndex = 0
    }
}