package unity_wallet

class Config {
    companion object {
        const val COIN = 100000000
        const val PRECISION_SHORT = 2
        const val PRECISION_FULL = 8
        val DEFAULT_CURRENCY_CODE get() = getDefaultCurrencyCode()
        const val USER_AGENT = "/Munt android:${BuildConfig.VERSION_NAME}/"
        const val BLOCK_EXPLORER_TX_TEMPLATE = "https://munt.chainviewer.org/tx/%s"
        const val BLOCK_EXPLORER_BLOCK_TEMPLATE = "https://explorer.munt.org/block/%s"
        const val AUDIBLE_NOTIFICATIONS_INTERVAL = 30 * 1000
        const val USE_RATE_PRECISION = true
        const val RATE_FETCH_INTERVAL = 60 * 1000 // minimum interval for fetching new exchange rates

        private fun getDefaultCurrencyCode(): String {
            return AppContext.instance.getString(R.string.default_currency_code)
        }
    }
}
