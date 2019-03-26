// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import kotlinx.coroutines.runBlocking
import org.junit.After
import org.junit.Test

import org.junit.Assert.*
import org.junit.Before
import java.util.logging.Level
import java.util.logging.Logger

/**
 * Example local unit test, which will execute on the development machine (host).
 *
 * See [testing documentation](http://d.android.com/tools/testing).
 */
class ExampleUnitTest
{
    @Before
    fun setUp() {
        initNocks()
    }

    @After
    fun tearDown() {
        terminateNocks()
    }

    @Test fun addition_isCorrect()
    {
        assertEquals(4, 2 + 2)
    }

    @Test fun liveNocksQuote()
    {
        val amountEUR = 5.0
        var amountNLG = -1.0
        runBlocking {
            amountNLG = nocksQuote(amountEUR).amountNLG
        }
        Logger.getAnonymousLogger().log(Level.INFO, "quote for EUR %f is %f NLG".format(amountEUR, amountNLG))
        assertTrue(amountNLG > 0.0)
    }

    @Test fun liveNocksOrder()
    {
        lateinit var order: NocksOrderResult
        runBlocking {
            // REMARK: this is a generated random IBAN which is not verified with Nocks
            // there is no realtime verification of the name and IBAN at Nocks
            // the order succeeds regardless what name is used
            order = nocksOrder(5.0, "NL69ABNA5560006823", "xyz gulden unit test")
            order
        }

        if (order.errorText != "")
            System.out.println(order.errorText)
        else
            System.out.println("Nocks order: ${order.depositAmountNLG} NLG to ${order.depositAddress}")

        assertTrue(order.depositAmountNLG > 0.0 &&
                order.depositAddress.isNotEmpty())

        lateinit var order2: NocksOrderResult
        runBlocking {
            // REMARK: this is a generated random IBAN which is not verified with Nocks
            // there is no realtime verification of the name and IBAN at Nocks
            // the order succeeds regardless what name is used
            order2 = nocksOrder(5.0, "NL69ABNA5560006823")
            order2
        }


        if (order2.errorText != "")
            System.out.println(order2.errorText)
        else
            System.out.println("Nocks order: ${order2.depositAmountNLG} NLG to ${order2.depositAddress}")

        assertTrue(order2.errorText.contains("Your IBAN isn't verified", true))
    }
}
