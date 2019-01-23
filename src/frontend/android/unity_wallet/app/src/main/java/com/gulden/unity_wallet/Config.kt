package com.gulden.unity_wallet

import android.net.Uri

class Config {
    companion object {
        val COIN = 100000000
        val PRECISION_SHORT = 2
        val PRECISION_FULL = 8
        val DEFAULT_CURRENCY_CODE get() = getDefaultCurrencyCode()
        val USER_AGENT = "/${BuildConfig.APPLICATION_ID}:${BuildConfig.VERSION_NAME}/"
        val BLOCK_EXPLORER = Uri.parse("https://blockchain.gulden.com")

        fun getDefaultCurrencyCode(): String {
            return AppContext.instance.getString(R.string.default_currency)
        }
    }
}
