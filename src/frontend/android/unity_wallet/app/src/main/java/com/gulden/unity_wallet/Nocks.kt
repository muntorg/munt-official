package com.gulden.unity_wallet

import com.squareup.moshi.Moshi
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import kotlin.random.Random
import com.itkacher.okhttpprofiler.OkHttpProfilerInterceptor
import com.squareup.moshi.JsonAdapter
import okhttp3.*
import se.ansman.kotshi.JsonSerializable
import se.ansman.kotshi.KotshiJsonAdapterFactory
import java.util.*

data class NocksQuoteResult(val amountNLG: Double)

data class NocksOrderResult(val depositAddress: String, val depositAmountNLG: Double)

private const val NOCKS_HOST = "www.nocks.com"

// TODO: Use host sandbox.nocks.com for testnet build
private const val FAKE_NOCKS_SERVICE = false

@JsonSerializable
data class SuccessValueNocksQuote (
    var amount: Double = -1.0
)

@JsonSerializable
data class NocksQuoteApiResult (
    var success: SuccessValueNocksQuote? = null,
    var error: List<String>? = null,
    var errorMessage: String? = null
)

@JsonSerializable
data class SuccessValueNocksOrder (
    var depositAmount: Double = -1.0,
    var deposit: String? = null,
    var expirationTimestamp: String? = null,
    var withdrawalAmount: String? = null,
    var withdrawalOriginal: String? = null
)

@JsonSerializable
data class NocksOrderApiResult (
    var error: String? = null,
    var success: SuccessValueNocksOrder? = null
)

@KotshiJsonAdapterFactory
abstract class ApplicationJsonAdapterFactory : JsonAdapter.Factory {
    companion object {
        val INSTANCE: ApplicationJsonAdapterFactory = KotshiApplicationJsonAdapterFactory()
    }
}

var client : OkHttpClient? = null
var moshi : Moshi? = null

// Only create one client per session so that we avoid unnecessary extra SSL handshakes and can take advantage of connection pooling etc.
fun initNocks()
{
    // Clean up any previously failed nocks sessions
    terminateNocks()

    // Force okhttp to include specific SSL certificates that are required, otherwise these calls simply fail unexpectedly on various devices.
    // See: https://github.com/square/okhttp/issues/3894
    val spec = ConnectionSpec.Builder(ConnectionSpec.COMPATIBLE_TLS)
            .tlsVersions(TlsVersion.TLS_1_2, TlsVersion.TLS_1_1, TlsVersion.TLS_1_0)
            .cipherSuites(
                    CipherSuite.TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
                    CipherSuite.TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
                    CipherSuite.TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
                    CipherSuite.TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA)
            .build()

    val builder = OkHttpClient.Builder()
    if (BuildConfig.DEBUG) {
        builder.addInterceptor(OkHttpProfilerInterceptor() )
    }
    client = builder.connectionSpecs(Collections.singletonList(spec)).build()
    moshi = Moshi.Builder().add(ApplicationJsonAdapterFactory.INSTANCE).build()
}

// Release client when we are done with current session
fun terminateNocks()
{
    //TODO: Cancel all unpaid payment requests here.
    client = null
    moshi = null
}

private suspend inline fun <reified ResultType> nocksRequest(endpoint: String, jsonParams: String): ResultType?
{
    val request = Request.Builder()
            .url("https://$NOCKS_HOST/api/$endpoint")
            .header("User-Agent", Config.USER_AGENT)
            .header("Content-Type", "application/json")
            .header("Accept", "application/json")
            .post(RequestBody.create(MediaType.get("application/json; charset=utf-8"), jsonParams))
            .build()

    // execute on IO thread pool
    val result = withContext(IO) {
        val response = client?.newCall(request)?.execute()
        val body = response?.body()
        if (body != null)
            body.string()
        else
            throw RuntimeException("Null body in Nocks response.")
    }

    val adapter = moshi?.adapter(ResultType::class.java)
    return adapter?.fromJson(result)
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
