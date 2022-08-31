// Copyright (c) 2018-2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package unity_wallet

import android.preference.PreferenceManager
import android.util.Log
import unity_wallet.Config.Companion.RATE_FETCH_INTERVAL
import kotlinx.coroutines.*
import org.json.JSONObject
import java.net.URL
import java.text.DecimalFormat
import java.util.*
import kotlin.math.floor
import kotlin.math.roundToLong

private const val TAG = "currency"

private const val APP_MARKET_URL = "https://api.gulden.com/api/v1/ticker"

/**
 * Fetch currency conversion rate from server (suspended, use from co-routine)
 *
 * @param code currency code
 * @throws Throwable on any error
 * @return conversion rate to convert from gulden to specified currency (ie. value = gulden * rate)
 */
suspend fun fetchCurrencyRate(code: String): Double
{
    return fetchAllCurrencyRates().getValue(code)
}

suspend fun fetchAllCurrencyRates(): Map<String, Double> {
    // use cached currency rates if last fetch within RATE_FETCH_INTERVAL
    val now = System.currentTimeMillis()
    if (now - currencyRatesLastFetched > RATE_FETCH_INTERVAL) {
        currencyRates = reallyfetchAllCurrencyRates()
        currencyRatesLastFetched = now
    }
    return currencyRates
}

// caching of currency rates
private var currencyRatesLastFetched = 0L
private var currencyRates = mapOf<String, Double>()

private suspend fun reallyfetchAllCurrencyRates(): Map<String, Double> {
    Log.i(TAG, "reallyfetchAllCurrencyRates")

    // fetch rate data from server
    lateinit var data: String
    try {
        data  = withContext(Dispatchers.IO) {
            URL(APP_MARKET_URL).readText()
        }
    }
    catch (e: Throwable) {
        Log.i(TAG, "Failed to fetch currency rates from $APP_MARKET_URL")
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

    return rates
}

data class Currency (val code: String, val name: String, val short: String, val precision: Int, val ratePrecision: Int) {
    fun formatRate(rate: Double, usePrefix: Boolean = true): String
    {
        val appliedPrecision = if (Config.USE_RATE_PRECISION) ratePrecision else precision
        val rateStr = if (appliedPrecision > 0) {
            val pattern = "#,##0.%s;-#".format("0".repeat(appliedPrecision))
            DecimalFormat(pattern).format(rate)
        }
        else {
            rate.roundToLong().toString()
        }
        return "%s %s".format(short, rateStr)
    }
}

class Currencies {
    companion object {
        var knownCurrencies = TreeMap<String, Currency>()

        init {
            val codes = AppContext.instance.resources.getStringArray(R.array.currency_codes)
            val names = AppContext.instance.resources.getStringArray(R.array.currency_names)
            val shorts = AppContext.instance.resources.getStringArray(R.array.currency_shorts)
            val precisions = AppContext.instance.resources.getIntArray(R.array.currency_precisions)
            val ratePrecisions = AppContext.instance.resources.getIntArray(R.array.currency_rate_precisions)

            for (i in codes.indices) {
                val c = Currency(code = codes[i], name = names[i], short = shorts[i], precision = precisions[i], ratePrecision = ratePrecisions[i])
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

/**
 * Format native amount using nu thousands separator, no locale (ie. always use "." (dot) for decimal separator) and PRECISION_SHORT decimals
 */
fun formatNativeSimple(nativeAmount: Long): String
{
    return String.format(Locale.ROOT,"%.${Config.PRECISION_SHORT}f", nativeAmount.toDouble() / 100000000)
}

fun formatNative(nativeAmount: Long, useNativePrefix: Boolean = true): String
{
    return "%s %s".format(if (useNativePrefix) "G" else "",
            (DecimalFormat("+#,##0.00;-#").format(nativeAmount.toDouble() / 100000000)))
}

fun formatNativeAndLocal(nativeAmount: Long, conversionRate: Double, useNativePrefix: Boolean = true, nativeFirst: Boolean = true): String
{
    val native = formatNative(nativeAmount, useNativePrefix)

    return if (conversionRate > 0.0) {
        val pattern = "+#,##0.%s;-#".format("0".repeat(localCurrency.precision))
        val local = DecimalFormat(pattern).format(conversionRate * nativeAmount.toDouble() / 100000000)
        if (nativeFirst)
            "%s (%s %s)".format(native, localCurrency.short, local)
        else
            "(%s %s) %s".format(localCurrency.short, local, native)
    }
    else
        native
}

/**
 * Convert native amount to locale agnostic, ie. api/json compatible string
 */
fun wireFormatNative(nativeAmount: Double): String {
    return String.format(Locale.ROOT,"%.${Config.PRECISION_FULL}f", nativeAmount)
}

fun Double.toNative(): Long {
    return floor(this * 100000000.0).toLong()
}
