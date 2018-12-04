package com.gulden.unity_wallet

class Config {
    companion object {
        val COIN = 100000000
        val PRECISION_SHORT = 2
        val PRECISION_FULL = 8
        val DEFAULT_CURRENCY_CODE get() = getDefaultCurrencyCode()

        fun getDefaultCurrencyCode(): String {
            return AppContext.instance.getString(R.string.default_currency)
        }
    }
}
