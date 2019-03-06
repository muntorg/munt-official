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
import android.view.View.MeasureSpec
import android.view.TextureView
import android.graphics.SurfaceTexture




class CameraSourcePreview(private val mContext: Context, attrs: AttributeSet) : TextureView(mContext, attrs), TextureView.SurfaceTextureListener
{
    public var textureHeight : Int = 0
    public var textureWidth : Int = 0
    public var previewHeight : Int = 0
    public var previewWidth : Int = 0
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

        setSurfaceTextureListener(this);
    }

    /*Texture view listener overrides begin*/
    override fun onSurfaceTextureAvailable(texture: SurfaceTexture, width: Int, height: Int)
    {
        mSurfaceAvailable = true
        textureHeight = height
        textureWidth = width
        try
        {
            startIfReady()
        }
        catch (e: IOException)
        {
            Log.e(TAG, "Could not start camera source.", e)
        }

    }

    override fun onSurfaceTextureSizeChanged(texture: SurfaceTexture, width: Int, height: Int)
    {
        textureHeight = height
        textureWidth = width
    }

    override fun onSurfaceTextureDestroyed(texture: SurfaceTexture): Boolean
    {
        mSurfaceAvailable = false
        return true
    }

    override fun onSurfaceTextureUpdated(texture: SurfaceTexture)
    {
    }
    /*Texture view listener overrides end*/


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
            mCameraSource?.start(this)
            mStartRequested = false
        }
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int)
    {
        previewHeight = bottom - top
        previewWidth = right - left

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
