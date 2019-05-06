package com.gulden.unity_wallet.util

import android.content.Intent
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.gulden.unity_wallet.*
import kotlinx.coroutines.*
import kotlin.coroutines.CoroutineContext

abstract class AppBaseActivity : AppCompatActivity(), CoroutineScope {

    /**
     * Called when walletReady is completed, but always after onResume
     * This can be used instead of using the walletReady deferred to simplify code.
     * Do NOT call the super method.
     */
    open fun onWalletReady() {}

    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        UnityCore.instance.addObserver(coreObserver, fun (callback:() -> Unit) { runOnUiThread { callback() }})
    }

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

    private val coreObserver = object: UnityCore.Observer {
        override fun createNewWallet() {
            // upgrade old wallet when a protobuf wallet file is present and is not marked as upgraded
            val upgradedMarkerFile = getFileStreamPath(Constants.OLD_WALLET_PROTOBUF_FILENAME+".upgraded")
            if (!upgradedMarkerFile.exists() && getFileStreamPath(Constants.OLD_WALLET_PROTOBUF_FILENAME).exists())
                gotoActivity(UpgradeActivity::class.java)
            else
                gotoActivity(WelcomeActivity::class.java)
        }
    }

    private fun gotoActivity(cls: Class<*> )
    {
        val intent = Intent(this, cls)
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        startActivity(intent)

        finish()
    }
}
