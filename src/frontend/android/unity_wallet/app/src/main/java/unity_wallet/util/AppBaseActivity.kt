// Copyright (c) 2018-2022 The Centure developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

package unity_wallet.util

import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.FragmentManager
import unity_wallet.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlin.coroutines.CoroutineContext

inline fun FragmentManager.inTransaction(func: androidx.fragment.app.FragmentTransaction.() -> androidx.fragment.app.FragmentTransaction) {
    beginTransaction().func().commit()
}

abstract class AppBaseActivity : AppCompatActivity(), CoroutineScope {

    /**
     * Called when walletReady is completed, but always after onResume
     * This can be used instead of using the walletReady deferred to simplify code.
     * Do NOT call the super method.
     */
    open fun onWalletReady() {}

    /**
     * Called when a wallet needs to be created. This is linked to the core notifyInitWithoutExistingWallet event.
     * This is always called after onResume.
     * The default behaviour will either continue to the welcome or upgrade activity,
     * when overriding this you should normally NOT CALL the super method.
     */
    open fun onWalletCreate() {
        welcomeOrUpgrade()
    }

    override val coroutineContext: CoroutineContext = Dispatchers.Main + SupervisorJob()

    override fun onResume() {
        super.onResume()

        // schedule onWalletReady
        UnityCore.instance.walletReady.invokeOnCompletion { handler ->
            if (handler == null) {
                runOnUiThread { onWalletReady() }
            }
        }

        // schedule onWalletCreate
        UnityCore.instance.walletCreate.invokeOnCompletion { handler ->
            if (handler == null) {
                runOnUiThread { onWalletCreate() }
            }
        }
    }

    private fun welcomeOrUpgrade() {
        // upgrade old wallet when a protobuf wallet file is present and is not marked as upgraded
        val upgradedMarkerFile = getFileStreamPath(Constants.OLD_WALLET_PROTOBUF_FILENAME+".upgraded")
        if (!upgradedMarkerFile.exists() && getFileStreamPath(Constants.OLD_WALLET_PROTOBUF_FILENAME).exists())
            gotoActivity(UpgradeActivity::class.java)
        else
            gotoActivity(WelcomeActivity::class.java)
    }

    private fun gotoActivity(cls: Class<*> )
    {
        val intent = Intent(this, cls)
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        startActivity(intent)

        finish()
    }

    fun gotoFragment(fragment: androidx.fragment.app.Fragment, frameId: Int) {
        supportFragmentManager.popBackStackImmediate()
        supportFragmentManager.inTransaction { replace(frameId, fragment) }
    }

    fun pushFragment(fragment: androidx.fragment.app.Fragment, frameId: Int) {
        supportFragmentManager.inTransaction { replace(R.id.mainLayout, fragment).addToBackStack(null) }
    }

    fun addFragment(fragment: androidx.fragment.app.Fragment, frameId: Int) {
        supportFragmentManager.inTransaction { add(frameId, fragment) }
    }
}
