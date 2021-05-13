package com.florin.unity_wallet.util

import com.florin.unity_wallet.UnityCore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import org.jetbrains.anko.alert
import org.jetbrains.anko.appcompat.v7.Appcompat
import org.jetbrains.anko.support.v4.runOnUiThread
import kotlin.coroutines.CoroutineContext

abstract class AppBaseFragment : androidx.fragment.app.Fragment(), CoroutineScope {

    /**
     * Called when walletReady is completed, but always after onResume
     * This can be used instead of using the walletReady deferred to simplify code.
     * Do NOT call the super method.
     */
    open fun onWalletReady() {}

    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    override fun onResume() {
        super.onResume()

        // schedule onWalletReady
        UnityCore.instance.walletReady.invokeOnCompletion { handler ->
            if (handler == null) {
                runOnUiThread { onWalletReady() }
            }
        }
    }

    fun errorMessage(msg: String) {
        context?.run {
            alert(Appcompat, msg) {
                positiveButton(getString(android.R.string.ok)) {}
            }.show()
        }
    }

}
