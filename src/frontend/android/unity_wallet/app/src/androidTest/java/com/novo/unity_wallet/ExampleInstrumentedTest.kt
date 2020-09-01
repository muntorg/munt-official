// Copyright (c) 2018 The Novo developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the Novo software license, see the accompanying
// file COPYING

package com.novo.unity_wallet

import androidx.test.InstrumentationRegistry
import androidx.test.runner.AndroidJUnit4

import org.junit.Test
import org.junit.runner.RunWith

import org.junit.Assert.*

/**
 * Instrumented test, which will execute on an Android device.
 *
 * See [testing documentation](http://d.android.com/tools/testing).
 */
@RunWith(AndroidJUnit4::class)
class ExampleInstrumentedTest
{
    @Test fun useAppContext()
    {
        // Context of the app under test.
        val appContext = InstrumentationRegistry.getTargetContext()
        assertEquals("com.novo.unity_wallet", appContext.packageName)
    }
}
