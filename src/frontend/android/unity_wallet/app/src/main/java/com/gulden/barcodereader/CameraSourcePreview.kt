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

import android.Manifest
import android.content.Context
import android.content.res.Configuration
import androidx.annotation.RequiresPermission
import android.util.AttributeSet
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.ViewGroup

import java.io.IOException

class CameraSourcePreview(private val mContext: Context, attrs: AttributeSet) : ViewGroup(mContext, attrs)
{
    private val mSurfaceView: SurfaceView
    private var mStartRequested: Boolean = false
    private var mSurfaceAvailable: Boolean = false
    private var mCameraSource: CameraSource? = null

    private val isPortraitMode: Boolean
        get()
        {
            val orientation = mContext.resources.configuration.orientation
            if (orientation == Configuration.ORIENTATION_LANDSCAPE)
            {
                return false
            }
            if (orientation == Configuration.ORIENTATION_PORTRAIT)
            {
                return true
            }

            Log.d(TAG, "isPortraitMode returning false by default")
            return false
        }

    init
    {
        mStartRequested = false
        mSurfaceAvailable = false

        mSurfaceView = SurfaceView(mContext)
        mSurfaceView.holder.addCallback(SurfaceCallback())
        addView(mSurfaceView)
    }

    @RequiresPermission(Manifest.permission.CAMERA)
    @Throws(IOException::class, SecurityException::class)
    fun start(cameraSource: CameraSource?)
    {
        if (cameraSource == null)
        {
            stop()
        }

        mCameraSource = cameraSource

        if (mCameraSource != null)
        {
            mStartRequested = true
            startIfReady()
        }
    }

    fun stop()
    {
        if (mCameraSource != null)
        {
            mCameraSource?.stop()
        }
    }

    fun release()
    {
        if (mCameraSource != null)
        {
            mCameraSource?.release()
            mCameraSource = null
        }
    }

    @RequiresPermission(Manifest.permission.CAMERA)
    @Throws(IOException::class, SecurityException::class)
    private fun startIfReady()
    {
        if (mStartRequested && mSurfaceAvailable)
        {
            mCameraSource?.start(mSurfaceView.holder)
            mStartRequested = false
        }
    }

    private inner class SurfaceCallback : SurfaceHolder.Callback
    {
        override fun surfaceCreated(surface: SurfaceHolder)
        {
            mSurfaceAvailable = true
            try
            {
                startIfReady()
            }
            catch (se: SecurityException)
            {
                Log.e(TAG, "Do not have permission to start the camera", se)
            }
            catch (e: IOException)
            {
                Log.e(TAG, "Could not start camera source.", e)
            }

        }

        override fun surfaceDestroyed(surface: SurfaceHolder)
        {
            mSurfaceAvailable = false
        }

        override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int)
        {
        }
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int)
    {
        var width = 320
        var height = 240
        if (mCameraSource != null)
        {
            val size = mCameraSource!!.previewSize
            if (size != null)
            {
                width = size.width
                height = size.height
            }
        }

        // Swap width and height sizes when in portrait, since it will be rotated 90 degrees
        if (isPortraitMode)
        {
            val tmp = width

            width = height
            height = tmp
        }

        val layoutWidth = right - left
        val layoutHeight = bottom - top

        // Computes height and width for potentially doing fit width.
        var childWidth = layoutWidth
        var childHeight = (layoutWidth.toFloat() / width.toFloat() * height).toInt()

        // If height is too tall using fit width, does fit height instead.
        if (childHeight > layoutHeight)
        {
            childHeight = layoutHeight
            childWidth = (layoutHeight.toFloat() / height.toFloat() * width).toInt()
        }

        for (i in 0 until childCount)
        {
            getChildAt(i).layout(0, 0, childWidth, childHeight)
        }

        try
        {
            startIfReady()
        }
        catch (se: SecurityException)
        {
            Log.e(TAG, "Do not have permission to start the camera", se)
        }
        catch (e: IOException)
        {
            Log.e(TAG, "Could not start camera source.", e)
        }

    }

    companion object
    {
        private const val TAG = "CameraSourcePreview"
    }
}