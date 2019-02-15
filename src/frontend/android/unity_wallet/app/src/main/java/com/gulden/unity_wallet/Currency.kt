package com.gulden.unity_wallet

import android.preference.PreferenceManager
import android.util.Log
import kotlinx.coroutines.*
import org.json.JSONObject
import java.net.URL
import java.text.DecimalFormat
import java.util.*

private const val TAG = "currency"

private const val GULDEN_MARKET_URL = "https://api.gulden.com/api/v1/ticker"

/**
 * Fetch currency conversion rate from server (suspended, use from co-routine)
 *
 * @param code currency code
 * @throws Throwable on any error
 * @return conversion rate to convert from gulden to specified currency (ie. value = gulden * rate)
 */
suspend fun fetchCurrencyRate(code: String): Double
{
    // 1st iteration
    // 1. fetch rates from server
    // 2. return rate queried for
    // 3. or throw some exception

    // fetch rate data from server
    lateinit var data: String
    try {
        data  = withContext(Dispatchers.IO) {
            URL(GULDEN_MARKET_URL).readText()
        }
    }
    catch (e: Throwable) {
        Log.i(TAG, "Failed to fetch currency rates from $GULDEN_MARKET_URL")
        throw e
    }

    // parse json rate data from server
    val rates = TreeMap<String, Double>()
    try {
        val json = JSONObject(data).getJSONArray("data")
        for (i in 0 until json.length()) {
            val cCode: String = json.getJSONObject(i).getString("code")
            val cRate: Double = json.getJSONObject(i).getString("rate").toDouble()
            if (cRate > 0.0)
                rates[cCode] = cRate
        }
    }
    catch (e: Throwable) {
        Log.i(TAG, "Failed to decode currency rates")
        throw e
    }

    return rates.getValue(code)
}

data class Currency (val code: String, val name: String, val short: String, val precision: Int )

class Currencies {
    companion object {
        var knownCurrencies = TreeMap<String, Currency>()

        init {
            val codes = AppContext.instance.resources.getStringArray(R.array.currency_codes)
            val names = AppContext.instance.resources.getStringArray(R.array.currency_names)
            val shorts = AppContext.instance.resources.getStringArray(R.array.currency_shorts)
            val precisions = AppContext.instance.resources.getIntArray(R.array.currency_precisions)

            for (i in 0 until codes.size) {
                val c = Currency(code = codes[i], name = names[i], short = shorts[i], precision = precisions[i])
                knownCurrencies[codes[i]] = c
            }
        }
    }
}

val localCurrency: Currency
    get() {
        val sharedPreferences = PreferenceManager.getDefaultSharedPreferences(AppContext.instance)
        val code = sharedPreferences.getString("preference_local_currency", Config.DEFAULT_CURRENCY_CODE)!!
        return Currencies.knownCurrencies[code]!!
    }

fun formatNative(nativeAmount: Long, useNativePrefix: Boolean = true): String
{
    val native = "%s %s".format(if (useNativePrefix) "G" else "",
            (DecimalFormat("+#,##0.00;-#").format(nativeAmount.toDouble() / 100000000)))
    return native
}

fun formatNativeAndLocal(nativeAmount: Long, conversionRate: Double, useNativePrefix: Boolean = true): String
{
    val native = formatNative(nativeAmount, useNativePrefix)

    return if (conversionRate > 0.0) {
        val pattern = "+#,##0.%s;-#".format("0".repeat(localCurrency.precision))
        "%s (%s %s)".format(native, localCurrency.short,
                DecimalFormat(pattern).format(conversionRate * nativeAmount.toDouble() / 100000000) )
    }
    else
        native
}
