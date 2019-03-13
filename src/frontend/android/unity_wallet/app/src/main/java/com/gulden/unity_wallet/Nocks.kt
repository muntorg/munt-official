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
import com.itkacher.okhttpprofiler.OkHttpProfilerInterceptor

data class NocksQuoteResult(val amountNLG: Double)

data class NocksOrderResult(val depositAddress: String, val depositAmountNLG: Double)

private const val NOCKS_HOST = "www.nocks.com"

// TODO: Use host sandbox.nocks.com for testnet build
private const val FAKE_NOCKS_SERVICE = false

@JsonClass(generateAdapter = true)
class NocksQuoteApiResult
{
    class SuccessValue {
        var amount: Double = -1.0
    }
    var error: String? = null
    var success: SuccessValue? = null
}

@JsonClass(generateAdapter = true)
class NocksOrderApiResult
{
    class SuccessValue {
        var depositAmount: Double = -1.0
        var deposit: String? = null
        var expirationTimestamp: String? = null
        var withdrawalAmount: String? = null
        var withdrawalOriginal: String? = null
    }
    var error: String? = null
    var success: SuccessValue? = null
}

private suspend inline fun <reified ResultType> nocksRequest(endpoint: String, jsonParams: String): ResultType?
{
    val request = Request.Builder()
            .url("https://$NOCKS_HOST/api/$endpoint")
            .header("User-Agent", Config.USER_AGENT)
            .post(RequestBody.create(MediaType.get("application/json; charset=utf-8"), jsonParams))
            .build()

    val builder = OkHttpClient.Builder()
    if (BuildConfig.DEBUG) {
        builder.addInterceptor(OkHttpProfilerInterceptor() )
    }
    val client = builder.build()

    // execute on IO thread pool
    val result = withContext(IO) {
        val response = client.newCall(request).execute()
        val body = response.body()
        if (body != null)
            body.string()
        else
            throw RuntimeException("Null body in Nocks response.")
    }

    val adapter = Moshi.Builder().build().adapter<ResultType>(ResultType::class.java)
    return adapter.fromJson(result)
}

suspend fun nocksQuote(amountEuro: Double): NocksQuoteResult
{
    return if (FAKE_NOCKS_SERVICE) {
        delay(500)
        val amount = Random.nextDouble(300.0, 400.0)
        NocksQuoteResult(amountNLG = amount)
    }
    else {
        val result = nocksRequest<NocksQuoteApiResult>("price", "{\"pair\": \"NLG_EUR\", \"amount\": \"$amountEuro\", \"fee\": \"yes\", \"amountType\": \"withdrawal\"}")
        val amountNLG = result?.success?.amount!!
        NocksQuoteResult(amountNLG)
    }
}

suspend fun nocksOrder(amountEuro: Double, destinationIBAN:String): NocksOrderResult
{
    if (FAKE_NOCKS_SERVICE) {
        delay(500)
        val amount = Random.nextDouble(300.0, 400.0)
        return NocksOrderResult(
                depositAddress = "GeDH37Y17DaLZb5x1XsZsFGq7Ked17uC8c",
                depositAmountNLG = amount)
    }
    else {
        val result = nocksRequest<NocksOrderApiResult>(
                "transaction",
                "{\"pair\": \"NLG_EUR\", \"amount\": \"$amountEuro\", \"withdrawal\": \"$destinationIBAN\"}")

        if (result?.success?.withdrawalOriginal != destinationIBAN)
            throw RuntimeException("Withdrawal address modified, please contact a developer for assistance.")

        return NocksOrderResult(
                depositAddress = result.success?.deposit!!,
                depositAmountNLG = result.success?.depositAmount!!)
    }
}
