package com.gulden.unity_wallet.ui

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.text.Spannable
import android.text.SpannableString
import android.text.style.BackgroundColorSpan
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.view.ActionMode
import androidx.appcompat.widget.ShareActionProvider
import androidx.core.view.MenuItemCompat
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.*
import com.gulden.unity_wallet.util.gotoWalletActivity
import kotlinx.android.synthetic.main.activity_show_recovery_phrase.*
import org.jetbrains.anko.sdk27.coroutines.onClick

private const val TAG = "show-recovery-activity"

class ShowRecoveryPhraseActivity : AppCompatActivity(), UnityCore.Observer
{
    //fixme: (GULDEN) Change to char[] to we can securely wipe.
    internal var recoveryPhrase: String? = null

    private var shareActionProvider: ShareActionProvider? = null

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_show_recovery_phrase)

        recoveryPhrase = intent.getStringExtra(this.packageName + "recovery_phrase")
        recovery_phrase_text_view.run {
            text = recoveryPhrase
            onClick { setFocusOnRecoveryPhrase() }
        }

        supportActionBar?.hide()

        updateView()

        UnityCore.instance.addObserver(this, fun (callback:() -> Unit) { runOnUiThread { callback() }})
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean
    {
        when (item.itemId)
        {
            android.R.id.home ->
            {
                finish()
                return true
            }
        }

        return super.onOptionsItemSelected(item)
    }

    override fun onDestroy()
    {
        UnityCore.instance.removeObserver(this)
        //fixme: (GULDEN) Securely wipe.
        recoveryPhrase = ""
        super.onDestroy()
    }

    override fun onCoreReady(): Boolean {
        gotoWalletActivity(this)
        return true
    }

    @Suppress("UNUSED_PARAMETER")
    fun onAcceptRecoveryPhrase(view: View)
    {
        Authentication.instance.chooseAccessCode(this) {
            if (UnityCore.instance.isCoreReady()) {
                if (GuldenUnifiedBackend.ContineWalletFromRecoveryPhrase(recoveryPhrase)) {
                    gotoWalletActivity(this)
                } else {
                    internalErrorAlert(this, "$TAG continuation failed")
                }
            } else {
                // Create the new wallet, a coreReady event will follow which will proceed to the main activity
                if (!GuldenUnifiedBackend.InitWalletFromRecoveryPhrase(recoveryPhrase))
                    internalErrorAlert(this, "$TAG init failed")
            }
        }
    }

    @Suppress("UNUSED_PARAMETER")
    fun onAcknowledgeRecoveryPhrase(view: View)
    {
        updateView()
    }

    private fun updateView()
    {
        // Only allow user to move on once they have acknowledged writing the recovery phrase down.
        button_accept_recovery_phrase.isEnabled = acknowledge_recovery_phrase.isChecked
    }

    internal inner class ActionBarCallBack : ActionMode.Callback
    {
        override fun onActionItemClicked(mode: ActionMode, item: MenuItem): Boolean
        {
            return if (item.itemId == R.id.item_copy_to_clipboard)
            {
                val clipboard = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                val clip = ClipData.newPlainText("backup", recoveryPhrase)
                clipboard.primaryClip = clip
                mode.finish()
                Toast.makeText(applicationContext, R.string.recovery_phrase_copy, Toast.LENGTH_LONG).show()
                true
            }
            else false

        }


        override fun onCreateActionMode(mode: androidx.appcompat.view.ActionMode, menu: Menu): Boolean
        {
            mode.menuInflater.inflate(R.menu.share_menu, menu)

            // Handle buy button
            val itemBuy = menu.findItem(R.id.item_buy_gulden)
            itemBuy.isVisible = false

            // Handle copy button
            //MenuItem itemCopy = menu.findItem(R.id.item_copy_to_clipboard);

            // Handle share button
            val itemShare = menu.findItem(R.id.action_share)
            shareActionProvider = ShareActionProvider(this@ShowRecoveryPhraseActivity)
            MenuItemCompat.setActionProvider(itemShare, shareActionProvider)

            val intent = Intent(Intent.ACTION_SEND)
            intent.type = "text/plain"
            intent.putExtra(Intent.EXTRA_TEXT, recoveryPhrase)
            shareActionProvider!!.setShareIntent(intent)

            @Suppress("DEPRECATION")
            val color = resources.getColor(R.color.colorPrimary)
            val spannableString = SpannableString(recovery_phrase_text_view.text)
            spannableString.setSpan(BackgroundColorSpan(color), 0, recovery_phrase_text_view.text.length, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
            recovery_phrase_text_view.setText(spannableString)

            return true
        }

        override fun onDestroyActionMode(mode: ActionMode)
        {
            shareActionProvider = null

            recovery_phrase_text_view.setText(recoveryPhrase ?: "")
        }

        override fun onPrepareActionMode(mode: ActionMode, menu: Menu): Boolean
        {
            return false
        }
    }

    private fun setFocusOnRecoveryPhrase()
    {
        startSupportActionMode(ActionBarCallBack())
    }

}
