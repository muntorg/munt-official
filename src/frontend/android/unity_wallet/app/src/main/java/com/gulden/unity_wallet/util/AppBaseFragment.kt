package com.gulden.unity_wallet.util

import android.content.DialogInterface
import com.gulden.unity_wallet.UnityCore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import android.os.Handler
import android.os.Looper
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlin.coroutines.CoroutineContext

abstract class AppBaseFragment : androidx.fragment.app.Fragment(), CoroutineScope {

    private val handlerUI = Handler(Looper.getMainLooper())

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
                handlerUI.post { onWalletReady() }
            }
        }
    }

    fun errorMessage(msg: String) {
        context?.run {
            MaterialAlertDialogBuilder(context!!)
                    .setMessage(msg)
                    .setPositiveButton(android.R.string.ok) { dialogInterface: DialogInterface, i: Int -> }
                    .show()
        }
    }
}
