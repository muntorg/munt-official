package com.gulden.unity_wallet.currency

import android.util.Log
import kotlinx.coroutines.*
import org.json.JSONObject
import java.net.URL
import java.util.*

private val TAG = "currency"

private val GULDEN_MARKET_URL = "https://api.gulden.com/api/v1/ticker"

/**
 * Fetch currency conversion rate from server (suspended, use from coroutine)
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
        Log.i(TAG, "Failed to fetch currency rates from ${GULDEN_MARKET_URL}")
        throw e
    }

    // parse json rate data from server
    val rates = TreeMap<String, Double>()
    try {
        val json = JSONObject(data).getJSONArray("data")
        for (i in 0 until json.length()) {
            val cCode: String = json.getJSONObject(i).getString("code")
            val cRate: Double = json.getJSONObject(i).getString("rate").toDouble()
            rates[cCode] = cRate
        }
    }
    catch (e: Throwable) {
        Log.i(TAG, "Failed to decode currency rates")
        throw e
    }

    return rates.getValue(code)
}
