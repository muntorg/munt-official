package com.gulden.unity_wallet

import com.squareup.moshi.Moshi
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import kotlin.random.Random
import com.itkacher.okhttpprofiler.OkHttpProfilerInterceptor
import com.jayway.jsonpath.JsonPath
import com.squareup.moshi.JsonAdapter
import kotlinx.coroutines.coroutineScope
import okhttp3.*
import se.ansman.kotshi.JsonSerializable
import se.ansman.kotshi.KotshiJsonAdapterFactory
import java.util.*

data class NocksQuoteResult(val amountNLG: Double, val amountEUR: Double)
data class NocksOrderResult(val depositAddress: String, val depositAmountNLG: Double, val amountEUR: Double)

// TODO: Use host sandbox.nocks.com for testnet build
private const val FAKE_NOCKS_SERVICE = false

@JsonSerializable
data class ErrorValue(
        var code: Int = 0,
        var message: String = ""
)

@JsonSerializable
data class NocksError(
        var status: Int? = null,
        var error: ErrorValue? = null
)

@JsonSerializable
data class NocksSimpleError(
        var error: String
)

@JsonSerializable
data class NocksListError(
        var error: List<String>?
)

@JsonSerializable
data class NocksAmount(
        val amount: String,
        val currency: String
)

@JsonSerializable
data class NocksQuoteParams(
        val source_currency: String,
        val target_currency: String,
        val amount: NocksAmount,
        val payment_method: PaymentMethod = PaymentMethod("gulden")
) {
    data class PaymentMethod(val method: String)
}

@JsonSerializable
data class NocksQuoteResponse(
        val data: Data,
        val status: Int
) {
    data class Data(
            val source_amount: NocksAmount,
            val target_amount: NocksAmount,
            val fee_amount: NocksAmount,
            val rate: String,
            val rate_reversed: String
    )
}

@JsonSerializable
data class NocksTransactionParams(
        val source_currency: String,
        val target_currency: String,
        val target_address: String,
        val name: String,
        val description: String = "",
        val amount: NocksAmount,
        val payment_method: PaymentMethod = PaymentMethod("gulden")
) {
    data class PaymentMethod(val method: String)
}

@KotshiJsonAdapterFactory
abstract class ApplicationJsonAdapterFactory : JsonAdapter.Factory {
    companion object {
        val INSTANCE: ApplicationJsonAdapterFactory = KotshiApplicationJsonAdapterFactory
    }
}

class NocksException(val errorText: String) : RuntimeException(errorText)

class NocksService {

    enum class Symbol(val symbol: String) {
        NLG("NLG"),
        EUR("EUR")
    }

    private var client: OkHttpClient
    private var moshi: Moshi

    @Suppress("ConstantConditionIf")
    private val apiHost:String
        get() = if (BuildConfig.TESTNET) "sandbox.nocks.com" else "api.nocks.com"

    // Only create one client per session so that we avoid unnecessary extra SSL handshakes and can take advantage of connection pooling etc.
    init {

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
        if (BuildConfig.DEBUG && (System.getProperty("java.runtime.name")?.contains("android", true) == true)) {
            builder.addInterceptor(OkHttpProfilerInterceptor())
        }
        client = builder.connectionSpecs(Collections.singletonList(spec)).build()
        moshi = Moshi.Builder().add(ApplicationJsonAdapterFactory.INSTANCE).build()
    }

    private fun extractNocksError(input: String): String {
        try {
            val adapterError = moshi?.adapter(NocksError::class.java)
            val error1 = adapterError?.fromJson(input)
            return error1?.error?.message!!
        } catch (e: Throwable) {
            try {
                val adapterError = moshi?.adapter(NocksSimpleError::class.java)
                val error2 = adapterError?.fromJson(input)
                return error2?.error!!
            } catch (e: Throwable) {
                try {
                    val adapterError = moshi?.adapter(NocksListError::class.java)
                    val error3 = adapterError?.fromJson(input)
                    return error3?.error!![0]
                } catch (e: Throwable) {
                    return "Nocks request error"
                }
            }
        }
    }

    private suspend inline fun <reified RequestType : Any> nocksRequestBody(endpoint: String, params: RequestType): String = coroutineScope {
        // transform params to json
        val paramAdapter = moshi.adapter(params.javaClass)
        val jsonParams = paramAdapter!!.toJson(params)

        // build request
        val request = Request.Builder()
                .url("https://${apiHost}/api/v2/$endpoint")
                .header("User-Agent", Config.USER_AGENT)
                .header("Content-Type", "application/json")
                .header("Accept", "application/json")
                .post(RequestBody.create(MediaType.get("application/json; charset=utf-8"), jsonParams))
                .build()

        // execute request suspended on IO thread pool and return response body
        val responseBody = withContext(IO) {
            val response = client!!.newCall(request).execute()

            val bodyStr = response.body()?.string()
                    ?: throw NocksException("Null body in Nocks response")

            if (response.code() < 200 || response.code() >= 300)
                throw NocksException(extractNocksError(bodyStr))

            bodyStr
        }

        responseBody
    }

    private suspend inline fun <reified ResultType, reified RequestType : Any> nocksRequestParsed(endpoint: String, params: RequestType): ResultType {
        val responseBody = nocksRequestBody(endpoint, params)

        // try parsing response body as expected result type regardless of status or error, if there is an error the
        // adapter will throw, which is catched and then the error is extracted
        try {
            val resultAdapter = moshi.adapter(ResultType::class.java).failOnUnknown().nonNull()
            return resultAdapter.fromJson(responseBody)!!
        } catch (e: Throwable) {
            val message = extractNocksError(responseBody)
            throw NocksException(message)
        }
    }

    suspend fun nocksQuote(amount: Double, amtCurrency: Symbol): NocksQuoteResult {
        return if (FAKE_NOCKS_SERVICE) {
            delay(500)
            val randomAmount = Random.nextDouble(300.0, 400.0)
            NocksQuoteResult(amountNLG = randomAmount, amountEUR = 1.0 / randomAmount)
        } else {
            // note that Double.toString() always uses dot "." for decimal separator it is not localized
            // see https://docs.oracle.com/javase/8/docs/api/java/lang/Double.html#toString-double-
            val params = NocksQuoteParams("NLG", "EUR", NocksAmount(amount.toString(), amtCurrency.symbol))
            val result = nocksRequestParsed<NocksQuoteResponse, NocksQuoteParams>("transaction/quote", params)
            NocksQuoteResult(amountNLG = result.data.source_amount.amount.toDouble(), amountEUR = result.data.target_amount.amount.toDouble())
        }
    }

    suspend fun nocksOrder(amount: Double, amtCurrency: Symbol, destinationIBAN: String, name: String = "", description: String = ""): NocksOrderResult {
        // May only contain letters, numbers, dashes and spaces. Nothing else is allowed, so don't escape but just strip away others.
        val re = Regex("[^-A-Za-z0-9 ]")
        val nameStripped = re.replace(name, "")
        val descriptionStripped = re.replace(description, "")

        if (FAKE_NOCKS_SERVICE) {
            delay(500)
            val amount = Random.nextDouble(300.0, 400.0)
            return NocksOrderResult(depositAddress = "GeDH37Y17DaLZb5x1XsZsFGq7Ked17uC8c", depositAmountNLG = amount, amountEUR = 1.0 / amount)
        } else {
            val params = NocksTransactionParams(source_currency = "NLG", target_currency = "EUR", target_address = destinationIBAN,
                    name = nameStripped, description = descriptionStripped, amount = NocksAmount(amount.toString(), amtCurrency.symbol))

            try {
                // Nocks v2 API transaction has a quite a complex structure of which we need only a couple of fields that are deeply nested inside
                // See the nocks-tx-sample.json in the unit tests
                // So we don't have model classes to deserialize the json but JsonPath is used to easily get the few values needed
                val body = nocksRequestBody("transaction", params)
                val context = JsonPath.parse(body)

                val amountNLG = context.read<String>("\$.data.source_amount.amount").toDouble()
                val sourceCurrency = context.read<String>("\$.data.source_amount.currency")
                if (sourceCurrency != Symbol.NLG.symbol)
                    throw NocksException("Invalid source currency")

                val guldenAddress = context.read<List<String>>("\$.data.payments.data[*].metadata.address").first()

                val amountEUR = context.read<String>("\$.data.target_amount.amount").toDouble()
                val targetCurrency = context.read<String>("\$.data.target_amount.currency")
                if (targetCurrency != Symbol.EUR.symbol)
                    throw NocksException("Invalid target currency")

                return NocksOrderResult(depositAddress = guldenAddress, depositAmountNLG = amountNLG, amountEUR = amountEUR)
            } catch (e: NocksException) {
                throw e
            } catch (e: Throwable) {
                throw NocksException("Error decoding Nocks request")
            }
        }
    }

}