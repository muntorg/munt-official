package com.gulden.unity_wallet

class Config {
    companion object {
        val COIN = 100000000
        val DEFAULT_CURRENCY_CODE get() = getDefaultCurrencyCode()

        fun getDefaultCurrencyCode(): String {
            return AppContext.instance.getString(R.string.default_currency)
        }
    }
}
