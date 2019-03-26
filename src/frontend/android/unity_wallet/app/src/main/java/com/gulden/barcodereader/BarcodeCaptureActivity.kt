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
import android.annotation.SuppressLint
import android.app.AlertDialog
import android.content.DialogInterface
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.graphics.Rect
import android.hardware.Camera
import android.hardware.Camera.Parameters.FLASH_MODE_OFF
import android.hardware.Camera.Parameters.FLASH_MODE_TORCH
import android.os.Bundle
import com.google.android.material.snackbar.Snackbar
import androidx.core.app.ActivityCompat
import androidx.appcompat.app.AppCompatActivity
import android.util.Log
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.View
import android.widget.ImageView
import android.widget.Toast

import com.google.android.gms.common.ConnectionResult
import com.google.android.gms.common.GoogleApiAvailability
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.vision.Detector
import com.google.android.gms.vision.FocusingProcessor
import com.google.android.gms.vision.Tracker
import com.google.android.gms.vision.barcode.Barcode
import com.google.android.gms.vision.barcode.BarcodeDetector
import com.gulden.unity_wallet.R
import kotlinx.android.synthetic.main.barcode_capture.*
import org.jetbrains.anko.contentView

import java.io.IOException


// Only detect barcodes that fall within our target area
class TargetBarcodeFocusingProcessor(preview : CameraSourcePreview, detectionBox: Rect, detector: Detector<Barcode>, tracker: Tracker<Barcode>) : FocusingProcessor<Barcode>(detector, tracker)
{
    private var detectionBoundingBox = detectionBox
    private var mPreview = preview

    private var widthScaleFactor = 1.0f
    private var heightScaleFactor = 1.0f

    private fun scaleX(horizontal: Int): Int
    {
        return (horizontal * widthScaleFactor).toInt()
    }


    private fun scaleY(vertical: Int): Int
    {
        return (vertical * heightScaleFactor).toInt()
    }

    private fun translateX(x: Int): Int
    {
        return scaleX(x)
    }

    private fun translateY(y: Int): Int
    {
        return scaleY(y)
    }

    override fun selectFocus(detections: Detector.Detections<Barcode>?): Int
    {
        val barcodes = detections?.detectedItems
        heightScaleFactor = mPreview.previewHeight / (detections?.frameMetadata?.height)?.toFloat()!!
        widthScaleFactor = mPreview.previewWidth / (detections.frameMetadata?.width)?.toFloat()!!

        for (i in 0 .. barcodes!!.size())
        {
            val barcodeID = barcodes.keyAt(i)
            val barcode = barcodes.get(barcodeID)
            val barcodeBoundingBox = barcode.boundingBox
            barcodeBoundingBox.top = translateY(barcodeBoundingBox.top)
            barcodeBoundingBox.bottom = translateY(barcodeBoundingBox.bottom)
            barcodeBoundingBox.left = translateX(barcodeBoundingBox.left)
            barcodeBoundingBox.right = translateX(barcodeBoundingBox.right)
            if (detectionBoundingBox.intersect(barcodeBoundingBox))
            {
                return barcodeID
            }
        }
        return -1
    }
}

/**
 * Activity for barcode scanning.This app detects barcodes with the
 * rear facing camera.
 */
class BarcodeCaptureActivity : AppCompatActivity(), BarcodeTracker.BarcodeUpdateListener
{
    private var mCameraSource: CameraSource? = null
    private var mPreview: CameraSourcePreview? = null
    private var mTargetOverlay: ImageView? = null
    private var mProcessor: TargetBarcodeFocusingProcessor? = null
    private var mUseFlash : Boolean = false
    private var mAutoFocus : Boolean = false

    // helper objects for detecting taps and pinches.
    private var scaleGestureDetector: ScaleGestureDetector? = null

    /**
     * Initializes the UI and creates the detector pipeline.
     */
    public override fun onCreate(icicle: Bundle?)
    {
        super.onCreate(icicle)
        setContentView(R.layout.barcode_capture)

        mPreview = findViewById<View>(R.id.preview) as CameraSourcePreview
        mTargetOverlay = findViewById<View>(R.id.scanTargetOverlayImage) as ImageView

        // read parameters from the intent used to launch the activity.
        mAutoFocus = intent.getBooleanExtra(AutoFocus, false)
        mUseFlash = intent.getBooleanExtra(UseFlash, false)

        scanCancelButton.setOnClickListener { finish() }
        scanToggleFlashButton.setOnClickListener { mUseFlash = !mUseFlash; mCameraSource?.setFlashMode(if (mUseFlash) {FLASH_MODE_TORCH} else { FLASH_MODE_OFF}) }


        // Below relies on sizing of view items so can only run after the view is visible
        contentView?.post {
            runOnUiThread {
                // Check for the camera permission before accessing the camera.  If the  permission is not granted yet, request permission.
                val rc = ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
                if (rc == PackageManager.PERMISSION_GRANTED)
                {
                    createCameraSource(mAutoFocus, mUseFlash)
                    startCameraSource()
                }
                else
                {
                    requestCameraPermission()
                }
            }
        }

        scaleGestureDetector = ScaleGestureDetector(this, ScaleListener())

        Snackbar.make(mPreview!!, "Pinch/Stretch to zoom", Snackbar.LENGTH_LONG).show()
    }

    /**
     * Handles the requesting of the camera permission.  This includes
     * showing a "Snackbar" message of why the permission is needed then
     * sending the request.
     */
    private fun requestCameraPermission()
    {
        Log.w(TAG, "Camera permission is not granted. Requesting permission")

        val permissions = arrayOf(Manifest.permission.CAMERA)

        if (!ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.CAMERA))
        {
            ActivityCompat.requestPermissions(this, permissions, RC_HANDLE_CAMERA_PERM)
            return
        }

        val thisActivity = this

        val listener = View.OnClickListener {
            ActivityCompat.requestPermissions(thisActivity, permissions, RC_HANDLE_CAMERA_PERM)
        }

        findViewById<View>(R.id.topLayout).setOnClickListener(listener)
        Snackbar.make(mPreview!!, "Camera permission required to scan QR code", Snackbar.LENGTH_INDEFINITE).setAction("ok", listener).show()
    }

    override fun onTouchEvent(e: MotionEvent): Boolean
    {
        if (scaleGestureDetector!!.onTouchEvent(e))
            return true

        return super.onTouchEvent(e)
    }

    /**
     * Creates and starts the camera.  Note that this uses a higher resolution in comparison
     * to other detection examples to enable the barcode detector to detect small barcodes
     * at long distances.
     *
     * Suppressing InlinedApi since there is a check that the minimum version is met before using
     * the constant.
     */
    @SuppressLint("InlinedApi")
    private fun createCameraSource(autoFocus: Boolean, useFlash: Boolean)
    {
        val context = applicationContext

        // A barcode detector is created to track barcodes.
        //  An associated focusing-processor instance  is set to receive the barcode detection results, track the barcodes, and maintain
        // graphics for each barcode on screen.  The factory is used by the multi-processor to
        // create a separate tracker instance for each barcode.
        val barcodeDetector = BarcodeDetector.Builder(context).build()
        val tracker = BarcodeTracker(this)

        val targetOverlayRect = Rect()
        mTargetOverlay!!.getGlobalVisibleRect(targetOverlayRect)

        mProcessor = TargetBarcodeFocusingProcessor(mPreview!!, targetOverlayRect, barcodeDetector, tracker)
        barcodeDetector.setProcessor(mProcessor)

        if (!barcodeDetector.isOperational)
        {
            // Note: The first time that an app using the barcode or face API is installed on a
            // device, GMS will download a native libraries to the device in order to do detection.
            // Usually this completes before the app is run for the first time.  But if that
            // download has not yet completed, then the above call will not detect any barcodes
            // and/or faces.
            //
            // isOperational() can be used to check if the required native libraries are currently
            // available.  The detectors will automatically become operational once the library
            // downloads complete on device.
            Log.w(TAG, "Detector dependencies are not yet available.")

            // Check for low storage.  If there is low storage, the native library will not be
            // downloaded, so detection will not become operational.
            val lowstorageFilter = IntentFilter(Intent.ACTION_DEVICE_STORAGE_LOW)
            val hasLowStorage = registerReceiver(null, lowstorageFilter) != null

            if (hasLowStorage)
            {
                Toast.makeText(this, "No space available to install qr code scanner", Toast.LENGTH_LONG).show()
                Log.w(TAG, "No space available to install qr code scanner")
            }
        }

        // Creates and starts the camera.  Note that this uses a higher resolution in comparison
        // to other detection examples to enable the barcode detector to detect small barcodes
        // at long distances.
        var builder = CameraSource.Builder(applicationContext, barcodeDetector).setFacing(CameraSource.CAMERA_FACING_BACK).setRequestedPreviewSize(1600, 1024).setRequestedFps(15.0f)

        if (autoFocus)
            builder = builder.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)

        if (useFlash)
            builder.setFlashMode(Camera.Parameters.FLASH_MODE_TORCH)

        mCameraSource = builder.build()
    }

    /**
     * Restarts the camera.
     */
    override fun onResume()
    {
        super.onResume()
        startCameraSource()
    }

    /**
     * Stops the camera.
     */
    override fun onPause()
    {
        super.onPause()
        if (mPreview != null)
        {
            mPreview?.stop()
        }
    }

    /**
     * Releases the resources associated with the camera source, the associated detectors, and the
     * rest of the processing pipeline.
     */
    override fun onDestroy()
    {
        super.onDestroy()
        if (mPreview != null)
        {
            mPreview?.release()
        }
    }

    // Handle camera permissions for scanning.
    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray)
    {
        if (requestCode != RC_HANDLE_CAMERA_PERM)
        {
            Log.d(TAG, "Got unexpected permission result: $requestCode")
            super.onRequestPermissionsResult(requestCode, permissions, grantResults)
            return
        }

        if (grantResults.size != 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED)
        {
            Log.d(TAG, "Camera permission granted - initialize the camera source")
            // we have permission, so create the camerasource
            createCameraSource(mAutoFocus, mUseFlash)
            startCameraSource()
            return
        }

        Log.e(TAG, "Permission not granted: results len = " + grantResults.size + " Result code = " + if (grantResults.size > 0) grantResults[0] else "(empty)")

        val listener = DialogInterface.OnClickListener { _, id -> finish() }

        val builder = AlertDialog.Builder(this)
        builder.setTitle("Gulden").setMessage("Unable to get camera permission").setPositiveButton("ok", listener).show()
    }

    /**
     * Starts or restarts the camera source, if it exists.  If the camera source doesn't exist yet
     * (e.g., because onResume was called before the camera source was created), this will be called
     * again when the camera source is created.
     */
    @Throws(SecurityException::class)
    private fun startCameraSource()
    {
        // check that the device has play services available.
        val code = GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(applicationContext)
        if (code != ConnectionResult.SUCCESS)
        {
            val dlg = GoogleApiAvailability.getInstance().getErrorDialog(this, code, RC_HANDLE_GMS)
            dlg.show()
        }

        if (mCameraSource != null)
        {
            try
            {
                mPreview?.start(mCameraSource as CameraSource)
            }
            catch (e: IOException)
            {
                Log.e(TAG, "Unable to start camera source.", e)
                mCameraSource?.release()
                mCameraSource = null
            }

        }
    }

    private inner class ScaleListener : ScaleGestureDetector.OnScaleGestureListener
    {
        override fun onScale(detector: ScaleGestureDetector): Boolean
        {
            return false
        }

        override fun onScaleBegin(detector: ScaleGestureDetector): Boolean
        {
            return true
        }

        override fun onScaleEnd(detector: ScaleGestureDetector)
        {
            mCameraSource?.doZoom(detector.scaleFactor)
        }
    }

    override fun onBarcodeDetected(barcode: Barcode?)
    {
        val data = Intent()
        data.putExtra(BarcodeObject, barcode)
        setResult(CommonStatusCodes.SUCCESS, data)
        finish()
    }

    companion object
    {
        private const val TAG = "Barcode-reader"

        // intent request code to handle updating play services if needed.
        private const val RC_HANDLE_GMS = 9001

        // permission request codes need to be < 256
        private  const val RC_HANDLE_CAMERA_PERM = 2

        // constants used to pass extra data in the intent
        const val AutoFocus = "AutoFocus"
        const val UseFlash = "UseFlash"
        const val BarcodeObject = "Barcode"
    }
}