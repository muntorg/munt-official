package com.gulden.unity_wallet

import kotlinx.coroutines.delay
import kotlin.random.Random

data class NocksQuoteResult(val amountNLG: String)

data class NocksOrderResult(val depositAddress: String, val depositAmountNLG: String)

private const val FAKE_NOCKS_SERVICE = true

suspend fun nocksQuote(amountEuro: String): NocksQuoteResult
{
    if (FAKE_NOCKS_SERVICE) {
        delay(500)
        val amount = Random.nextDouble(300.0, 400.0)
        return NocksQuoteResult(amountNLG = String.format("%.${Config.PRECISION_FULL}f", amount))
    }
    else {
        //TODO: implement Nocks quote json post request
        return NocksQuoteResult("")
    }
}

suspend fun nocksOrder(amountEuro: String, iban:String): NocksOrderResult
{
    if (FAKE_NOCKS_SERVICE) {
        delay(500)
        val amount = Random.nextDouble(300.0, 400.0)
        return NocksOrderResult(
                depositAddress = "GeDH37Y17DaLZb5x1XsZsFGq7Ked17uC8c",
                depositAmountNLG = String.format("%.${Config.PRECISION_FULL}f", amount))
    }
    else {
        //TODO: implement Nocks order json post request
        return NocksOrderResult("", "")
    }
}
