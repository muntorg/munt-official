package com.gulden.unity_wallet

import android.net.Uri

class Config {
    companion object {
        const val COIN = 100000000
        const val PRECISION_SHORT = 2
        const val PRECISION_FULL = 8
        val DEFAULT_CURRENCY_CODE get() = getDefaultCurrencyCode()
        const val USER_AGENT = "/${BuildConfig.APPLICATION_ID}:${BuildConfig.VERSION_NAME}/"
        val BLOCK_EXPLORER = Uri.parse("https://blockchain.gulden.com")!!
        const val AUDIBLE_NOTIFICATIONS_INTERVAL = 30 * 1000
        const val USE_RATE_PRECISION = true

        private fun getDefaultCurrencyCode(): String {
            return AppContext.instance.getString(R.string.default_currency_code)
        }
    }
}
