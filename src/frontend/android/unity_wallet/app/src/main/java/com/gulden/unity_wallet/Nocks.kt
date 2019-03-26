package com.gulden.unity_wallet

import com.squareup.moshi.Moshi
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import kotlin.random.Random
import com.itkacher.okhttpprofiler.OkHttpProfilerInterceptor
import com.squareup.moshi.JsonAdapter
import com.squareup.moshi.JsonWriter
import okhttp3.*
import okio.Buffer
import se.ansman.kotshi.JsonSerializable
import se.ansman.kotshi.KotshiJsonAdapterFactory
import java.util.*


data class NocksQuoteResult(val amountNLG: Double, val errorText: String)
data class NocksOrderResult(val depositAddress: String, val depositAmountNLG: Double, val errorText: String)

private const val NOCKS_HOST = "www.nocks.com"

// TODO: Use host sandbox.nocks.com for testnet build
private const val FAKE_NOCKS_SERVICE = false

@JsonSerializable
data class ErrorValue (
        var code: Int = 0,
        var message: String = ""
)

@JsonSerializable
data class NocksError (
    var status: Int? = null,
    var error: ErrorValue? = null
)

@JsonSerializable
data class NocksSimpleError (
     var error: String
)

@JsonSerializable
data class NocksListError (
    var error: List<String>?
)


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
    var error: List<String>? = null,
    var errorMessage: String? = null,
    var success: SuccessValueNocksOrder? = null
)

@KotshiJsonAdapterFactory
abstract class ApplicationJsonAdapterFactory : JsonAdapter.Factory {
    companion object {
        val INSTANCE: ApplicationJsonAdapterFactory? = KotshiApplicationJsonAdapterFactory
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
    if (BuildConfig.DEBUG && System.getProperty("java.runtime.name").contains("android", true)) {
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

fun escapeStringToJSON(inputString : String) : String
{
    if (inputString.isEmpty())
        return "\"\""

    val b = Buffer() // okio.Buffer
    val writer = JsonWriter.of(b)
    writer.value(inputString)
    writer.flush()
    return b.readUtf8()
}

private fun extractNocksError(input : String) : String
{
    try
    {
        val adapterError = moshi?.adapter(NocksError::class.java)
        val error1 = adapterError?.fromJson(input)
        return error1?.error?.message!!
    }
    catch (e: Throwable)
    {
        try
        {
            val adapterError = moshi?.adapter(NocksSimpleError::class.java)
            val error2 = adapterError?.fromJson(input)
            return error2?.error!!
        }
        catch (e: Throwable)
        {
            try
            {
                val adapterError = moshi?.adapter(NocksListError::class.java)
                val error3 = adapterError?.fromJson(input)
                return error3?.error!![0]
            }
            catch (e: Throwable)
            {
                return "Nocks request error"
            }
        }
    }
    return ""
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

    var error=false
    var code=0
    // execute on IO thread pool
    val result = withContext(IO) {
        val response = client?.newCall(request)?.execute()
        val body = response?.body()
        code = response?.code() ?: 0
        if (code >= 400)
            error = true

        if (body != null)
        {
            error = false
            body.string()
        }
        else
        {
            error = true
            "{\"error\": {\"Null body in Nocks response\"}}"
        }
    }

    // Error code should always be set if error, however in se it isn't we do a rudimentary start of string search for "error as well" (as we had at least one case of this in the past)
    if (error || result.startsWith("{\"error"))
    {
        // Nocks errors come in different forms so we can't handle it in the moshi class directly as it is too complex
        // Instead marshall the error via several intermediate types, tot ry and parse the error as best as possible - and then insert it into our eventual return type in a way it understands
        var message= extractNocksError(result);
        val adapter = moshi?.adapter(ResultType::class.java)
        return adapter?.fromJson("{\"errorMessage\": \"$message\"}")!!
    }
    else
    {
        val adapter = moshi?.adapter(ResultType::class.java)
        return adapter?.fromJson(result)
    }
}

suspend fun nocksQuote(amountEuro: Double): NocksQuoteResult
{
    return if (FAKE_NOCKS_SERVICE) {
        delay(500)
        val amount = Random.nextDouble(300.0, 400.0)
        NocksQuoteResult(amountNLG = amount, errorText =  "")
    }
    else {
        var strAmount = amountEuro.toString()
        strAmount = strAmount.replace(",",".")
        var strAmountLen = strAmount.length
        val result = nocksRequest<NocksQuoteApiResult>("price", "{\"pair\": \"NLG_EUR\", \"amount\": \"$strAmount\", \"fee\": \"yes\", \"amountType\": \"withdrawal\"}")
        val amount = if (result?.success != null) { result.success?.amount!! } else -1.0
        var errorMessage = ""
        if (result?.error != null)
        {
            errorMessage = result.error!![0]
        }
        else if (result?.errorMessage != null)
        {
            errorMessage = result.errorMessage!!
        }
        else if (amount < 0)
        {
            errorMessage = "Unknown error"
        }
        NocksQuoteResult(amountNLG = amount, errorText = errorMessage)
    }
}

suspend fun nocksOrder(amountEuro: Double, destinationIBAN:String, name:String = "", description: String = ""): NocksOrderResult
{
    var nameEscaped = escapeStringToJSON(name);
    var descriptionEscaped = escapeStringToJSON(description)

    if (FAKE_NOCKS_SERVICE) {
        delay(500)
        val amount = Random.nextDouble(300.0, 400.0)
        return NocksOrderResult(depositAddress = "GeDH37Y17DaLZb5x1XsZsFGq7Ked17uC8c", depositAmountNLG = amount, errorText = "")
    }
    else {
        val json =
                if (name.isEmpty())
                    "{\"pair\": \"NLG_EUR\", \"amount\": \"$amountEuro\", \"withdrawal\": \"$destinationIBAN\"}"
                else
                    "{\"pair\": \"NLG_EUR\", \"amount\": \"$amountEuro\", \"withdrawal\": \"$destinationIBAN\", \"name\": $nameEscaped, \"text\": $descriptionEscaped}"

        val result = nocksRequest<NocksOrderApiResult>(
                "transaction",
                json)

        var errorMessage = ""
        var depositAddress = ""
        var depositAmountNLG = -1.0
        if (result?.success != null)
        {
            if (result.success?.withdrawalOriginal != destinationIBAN) throw RuntimeException("Withdrawal address modified, please contact a developer for assistance.")
            depositAddress = result.success?.deposit!!
            depositAmountNLG = result.success?.depositAmount!!
        }
        else
        {
            if (result?.error != null)
            {
                errorMessage = result.error!![0]
            }
            else if (result?.errorMessage != null)
            {
                errorMessage = result.errorMessage!!
            }
            else
            {
                errorMessage = "Unknown error"
            }
        }
        return NocksOrderResult(depositAddress, depositAmountNLG, errorMessage)
    }
}
