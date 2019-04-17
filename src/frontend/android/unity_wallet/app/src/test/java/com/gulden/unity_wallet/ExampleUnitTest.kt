// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import com.jayway.jsonpath.JsonPath
import kotlinx.coroutines.runBlocking
import org.junit.Test

import org.junit.Assert.*
import java.io.File
import java.util.logging.Level
import java.util.logging.Logger

/**
 * Example local unit test, which will execute on the development machine (host).
 *
 * See [testing documentation](http://d.android.com/tools/testing).
 */
class ExampleUnitTest
{
    @Test fun addition_isCorrect()
    {
        assertEquals(4, 2 + 2)
    }

    @Test
    fun liveNocksQuote() {
        val nocks = NocksService()

        lateinit var result1: NocksQuoteResult
        lateinit var result2: NocksQuoteResult

        try {
            runBlocking {
                val amountEUR = 5.0
                result1 = nocks.nocksQuote(amount = amountEUR, amtCurrency = NocksService.Symbol.EUR)
                Logger.getAnonymousLogger().log(Level.INFO, "quote for EUR %f is %f NLG %f EUR".format(amountEUR, result1.amountNLG, result1.amountEUR))

                val amountNLG = 1000.0
                result2 = nocks.nocksQuote(amount = amountNLG, amtCurrency = NocksService.Symbol.NLG)
                Logger.getAnonymousLogger().log(Level.INFO, "quote for NLG %f is %f NLG %f EUR".format(amountNLG, result2.amountNLG, result2.amountEUR))
            }
        } catch (e: NocksException) {
            Logger.getAnonymousLogger().log(Level.INFO, "Nocks error: ${e.errorText}")
        }
        assertTrue(result1.amountEUR > 0.0)
        assertTrue(result1.amountNLG > 0.0)
        assertTrue(result2.amountEUR > 0.0)
        assertTrue(result2.amountNLG > 0.0)
    }

    @Test fun parseNocksTx()
    {
        val json = File("src/test/resources/nocks-tx-sample.json").readText()
        val context = JsonPath.parse(json)

        val amountNLG = context.read<String>("\$.data.source_amount.amount").toDouble()
        val sourceCurrency = context.read<String>("\$.data.source_amount.currency")

        val amountEUR = context.read<String>("\$.data.target_amount.amount").toDouble()
        val targetCurrency = context.read<String>("\$.data.target_amount.currency")

        val guldenAddress = context.read<List<String>>("\$.data.payments.data[*].metadata.address").first()

        println("transact $amountNLG $sourceCurrency into $amountEUR $targetCurrency address $guldenAddress")

        assertEquals("NLG", sourceCurrency)
        assertEquals(312.1128, amountNLG, 0.0)
        assertEquals("EUR", targetCurrency)
        assertEquals(5.0, amountEUR, 0.0)
        assertEquals("GaSq3VWKoRpoCCekaQBkLMr5i3MHwwxgEW", guldenAddress)
    }

    @Test fun liveNocksOrder()
    {
        val nocks = NocksService()

        lateinit var order: NocksOrderResult
        try {
            runBlocking {
                // REMARK: this is a generated random IBAN which is not verified with Nocks
                // there is no realtime verification of the name and IBAN at Nocks
                // the order succeeds regardless what name is used
                order = nocks.nocksOrder(5.0, NocksService.Symbol.EUR,"NL69ABNA5560006823", "xyz gulden unit test")
                order
            }
            System.out.println("Nocks order: ${order.depositAmountNLG} NLG to ${order.depositAddress} for ${order.amountEUR} EUR")
            assertTrue(order.depositAmountNLG > 0.0 && order.depositAddress.isNotEmpty())
        }
        catch (e: NocksException) {
            System.out.println(e.errorText)
        }


        lateinit var order2: NocksOrderResult
        try {
            runBlocking {
                // REMARK: this is a generated random IBAN which is not verified with Nocks
                // there is no realtime verification of the name and IBAN at Nocks
                // the order succeeds regardless what name is used
                order2 = nocks.nocksOrder(5.0, NocksService.Symbol.EUR, "NL69ABNA5560006823")
                order2
            }
            System.out.println("Nocks order: ${order2.depositAmountNLG} NLG to ${order2.depositAddress} for ${order2.amountEUR} EUR")
        }
        catch (e: NocksException) {
            System.out.println(e.errorText)
            assertTrue(e.errorText.contains("Your IBAN isn't verified", true))
        }

        lateinit var order3: NocksOrderResult
        try {
            runBlocking {
                // REMARK: this is a generated random IBAN which is not verified with Nocks
                // there is no realtime verification of the name and IBAN at Nocks
                // the order succeeds regardless what name is used
                order3 = nocks.nocksOrder(1000.0, NocksService.Symbol.NLG, "NL69ABNA5560006823", "xyz gulden unit test")
                order3
            }
            System.out.println("Nocks order: ${order3.depositAmountNLG} NLG to ${order3.depositAddress} for ${order3.amountEUR} EUR")
        }
        catch (e: NocksException) {
            System.out.println(e.errorText)
            assertTrue(e.errorText.contains("Your IBAN isn't verified", true))
        }
    }
}
