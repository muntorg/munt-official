package com.gulden.unity_wallet

import com.squareup.moshi.JsonClass
import com.squareup.moshi.Moshi
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import okhttp3.MediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody
import kotlin.random.Random

data class NocksQuoteResult(val amountNLG: String)

data class NocksOrderResult(val depositAddress: String, val depositAmountNLG: String)

private const val NOCKS_HOST = "www.nocks.com"

// TODO: Use host sandbox.nocks.com for testnet build
private const val FAKE_NOCKS_SERVICE = false

@JsonClass(generateAdapter = true)
class NocksQuoteApiResult
{
    class SuccesValue {
        var amount: Double = -1.0
    }
    var error: String? = null
    var success: SuccesValue? = null
}

suspend fun nocksQuote(amountEuro: String): NocksQuoteResult
{
    if (FAKE_NOCKS_SERVICE) {
        delay(500)
        val amount = Random.nextDouble(300.0, 400.0)
        return NocksQuoteResult(amountNLG = String.format("%.${Config.PRECISION_FULL}f", amount))
    }
    else {
        val request = Request.Builder()
                .url("https://$NOCKS_HOST/api/price")
                .post(RequestBody.create(
                        MediaType.get("application/json; charset=utf-8"),
                        "{\"pair\": \"NLG_EUR\", \"amount\": \"$amountEuro\", \"fee\": \"yes\", \"amountType\": \"withdrawal\"}"))
                .build()
        val client = OkHttpClient()

        // execute on IO thread pool
        val result = withContext(IO) {
            val response = client.newCall(request).execute()
            response.body()?.string()
        }

        val adapter = Moshi.Builder().build().adapter<NocksQuoteApiResult>(NocksQuoteApiResult::class.java)
        val amountNLG = adapter.fromJson(result)?.success?.amount
        return NocksQuoteResult(String.format("%.${Config.PRECISION_FULL}f", amountNLG))
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
