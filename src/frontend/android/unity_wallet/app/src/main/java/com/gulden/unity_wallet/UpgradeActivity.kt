// Copyright (c) 2018 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl), Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.View
import android.view.inputmethod.InputMethodManager
import androidx.appcompat.app.AppCompatActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.LegacyWalletResult
import com.gulden.unity_wallet.Constants.OLD_WALLET_PROTOBUF_FILENAME
import kotlinx.android.synthetic.main.upgrade_password.view.*
import org.jetbrains.anko.alert
import org.jetbrains.anko.appcompat.v7.Appcompat
import org.jetbrains.anko.design.longSnackbar
import java.io.File
import kotlin.concurrent.thread

private const val TAG = "upgrade-activity"

class UpgradeActivity : AppCompatActivity(), UnityCore.Observer
{
    private var haveExistingWalletFile = false;

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_upgrade)

        UnityCore.instance.addObserver(this, fun (callback:() -> Unit) { runOnUiThread { callback() }})
    }

    override fun onDestroy() {
        super.onDestroy()

        UnityCore.instance.removeObserver(this)
    }

    override fun onCoreReady(): Boolean {
        // create marker file to indicate upgrade
        // this prevents prompting for an upgrade again later should the user remove his wallet
        // still the data of the old wallet is retained so if (god forbid) should something go wrong with upgrades
        // in the field a fix can be published which could ignore the upgrade marker
        val upgradedMarkerFile = getFileStreamPath(Constants.OLD_WALLET_PROTOBUF_FILENAME+".upgraded")
        if (!upgradedMarkerFile.exists()) {
            val versionCode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                packageManager.getPackageInfo(packageName, 0).longVersionCode
            } else {
                @Suppress("DEPRECATION")
                packageManager.getPackageInfo(packageName, 0).versionCode.toLong()
            }
            upgradedMarkerFile.writeText("%d\n".format(versionCode))
        }

        gotoActivity(WalletActivity::class.java)
        return true
    }

    fun onUpgradeWithPassword(view : View, oldPassword : String)
    {
        thread(true)
        {
            val result = GuldenUnifiedBackend.isValidAndroidLegacyProtoWallet(filesDir.toString() + File.separator + OLD_WALLET_PROTOBUF_FILENAME, oldPassword)
            if (result == LegacyWalletResult.PASSWORD_INVALID)
            {
                this.runOnUiThread { view.longSnackbar(getString(R.string.upgrade_wrong_password)) }
            }
            else if (result == LegacyWalletResult.INVALID_OR_CORRUPT)
            {
                this.runOnUiThread { view.longSnackbar("Unable to upgrade old wallet, contact support for assistance.") }
            }
            else
            {
                chooseNewAccessCodeAndUpgrade(oldPassword, view)
            }
        }
    }

    private var processingUpgrade = false;
    fun onUpgrade(view: View)
    {
        if (processingUpgrade)
            return
        processingUpgrade = true;

        thread(true)
        {
            when (GuldenUnifiedBackend.isValidAndroidLegacyProtoWallet(filesDir.toString() + File.separator + OLD_WALLET_PROTOBUF_FILENAME, ""))
            {
                LegacyWalletResult.ENCRYPTED_PASSWORD_REQUIRED ->
                {
                    this.runOnUiThread {
                        val upgradeDialogView = layoutInflater.inflate(R.layout.upgrade_password, null)
                        val builder = alert(Appcompat) {
                            title = getString(R.string.upgrade_enter_password)
                            customView = upgradeDialogView
                            positiveButton("OK") {
                                onUpgradeWithPassword(view, upgradeDialogView.password.text.toString())
                            }
                        }
                        val dialog = builder.build()
                        dialog.setOnShowListener {
                            upgradeDialogView.password.requestFocus()
                            upgradeDialogView.password.post {
                                val imm = application.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                                imm.showSoftInput(upgradeDialogView.password, InputMethodManager.SHOW_IMPLICIT)
                            }
                        }
                        dialog.show()
                    }
                }
                LegacyWalletResult.VALID ->
                {
                    chooseNewAccessCodeAndUpgrade("", view)
                }
                else ->
                {
                    this.runOnUiThread { view.longSnackbar("Unable to upgrade old wallet, contact support for assistance.") }
                }
            }
        }

        processingUpgrade = false
    }

    private fun chooseNewAccessCodeAndUpgrade(oldPassword: String, view: View) {
        this.runOnUiThread {
            Authentication.instance.chooseAccessCode(this, getString(R.string.access_code_choose_upgrade_title)) { accessCode ->
                thread(true) {
                    val newPassword = accessCode.joinToString("")
                    GuldenUnifiedBackend.InitWalletFromAndroidLegacyProtoWallet(filesDir.toString() + File.separator + OLD_WALLET_PROTOBUF_FILENAME, oldPassword, newPassword)
                }
                this.runOnUiThread { view.longSnackbar("Wallet upgrade in progress") }
            }
        }
    }

    @Suppress("UNUSED_PARAMETER")
    fun onStartFresh(view: View)
    {
        alert(Appcompat, getString(R.string.upgrade_start_fresh_desription), getString(R.string.upgrade_start_fresh_title))
        {
            positiveButton("Start fresh")
            {
                gotoActivity(WelcomeActivity::class.java)
            }
            negativeButton("Cancel")
            {
            }
        }.show()
    }

    private fun gotoActivity(cls: Class<*> )
    {
        val intent = Intent(this, cls)
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        startActivity(intent)
    }
}
