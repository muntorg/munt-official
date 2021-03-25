package com.novo.unity_wallet

class Config {
    companion object {
        const val COIN = 100000000
        const val PRECISION_SHORT = 2
        const val PRECISION_FULL = 8
        val DEFAULT_CURRENCY_CODE get() = getDefaultCurrencyCode()
        const val USER_AGENT = "/Novo android:${BuildConfig.VERSION_NAME}/"
        const val BLOCK_EXPLORER_TX_TEMPLATE = "https://www.dactual.com/transaction.php?tx=%s"
        const val BLOCK_EXPLORER_BLOCK_TEMPLATE = "https://blockchain.novocurrency.com/block/%s"
        const val AUDIBLE_NOTIFICATIONS_INTERVAL = 30 * 1000
        const val USE_RATE_PRECISION = true
        const val RATE_FETCH_INTERVAL = 60 * 1000 // minimum interval for fetching new exchange rates

        private fun getDefaultCurrencyCode(): String {
            return AppContext.instance.getString(R.string.default_currency_code)
        }
    }
}
