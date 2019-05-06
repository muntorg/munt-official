package com.gulden.unity_wallet.util

import com.gulden.unity_wallet.UnityCore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlin.coroutines.CoroutineContext

abstract class AppBaseFragment: androidx.fragment.app.Fragment(), CoroutineScope {

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
        launch(Dispatchers.Main) {
            UnityCore.instance.walletReady.invokeOnCompletion { handler ->
                if (handler == null)
                    onWalletReady()
            }
        }
    }
}
