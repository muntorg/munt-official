package unity_wallet.ui

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
import androidx.appcompat.view.ActionMode
import androidx.appcompat.widget.ShareActionProvider
import androidx.core.view.MenuItemCompat
import unity_wallet.jniunifiedbackend.ILibraryController
import unity_wallet.*
import unity_wallet.util.AppBaseActivity
import unity_wallet.util.gotoWalletActivity
import unity_wallet.util.setFauxButtonEnabledState
import kotlinx.android.synthetic.main.activity_show_recovery_phrase.*

private const val TAG = "show-recovery-activity"

class ShowRecoveryPhraseActivity : AppBaseActivity(), UnityCore.Observer
{
    private val erasedWallet = UnityCore.instance.isCoreReady()

    //fixme: (GULDEN) Change to char[] to we can securely wipe.
    private var recoveryPhrase: String? = null
    internal var recoveryPhraseTrimmed: String? = null

    private var shareActionProvider: ShareActionProvider? = null

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_show_recovery_phrase)

        recoveryPhrase = intent.getStringExtra(this.packageName + "recovery_phrase_with_birth")
        recoveryPhraseTrimmed = intent.getStringExtra(this.packageName + "recovery_phrase")
        recovery_phrase_text_view.run {
            //TODO: Reintroduce showing birth time here if/when we decide we want it in future
            text = recoveryPhraseTrimmed
            setOnClickListener { setFocusOnRecoveryPhrase() }
        }

        supportActionBar?.hide()

        updateView()

        acknowledge_recovery_phrase.setOnClickListener{  onAcknowledgeRecoveryPhrase(it) }
        button_accept_recovery_phrase.setOnClickListener{  onAcceptRecoveryPhrase(it) }

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
        recoveryPhraseTrimmed = ""
        super.onDestroy()
    }

    override fun onWalletReady() {
        if (!erasedWallet)
            gotoWalletActivity(this)
    }

    override fun onWalletCreate() {
        // do nothing, we are supposed to sit here until the wallet was created
    }

    @Suppress("UNUSED_PARAMETER")
    fun onAcceptRecoveryPhrase(view: View)
    {
        // Only allow user to move on once they have acknowledged writing the recovery phrase down.
        if (!acknowledge_recovery_phrase.isChecked)
        {
            Toast.makeText(applicationContext, "Write down your recovery phrase", Toast.LENGTH_LONG).show()
            return
        }

        button_accept_recovery_phrase.visibility = View.INVISIBLE
        // TODO must have core started and createWallet signal

        Authentication.instance.chooseAccessCode(
                this,
                null,
                action = fun(password: CharArray) {
                    if (UnityCore.instance.isCoreReady()) {
                        if (ILibraryController.ContinueWalletFromRecoveryPhrase(recoveryPhrase, password.joinToString(""))) {
                            gotoWalletActivity(this)
                        } else {
                            internalErrorAlert(this, "$TAG continuation failed")
                        }
                    } else {
                        // Create the new wallet, a coreReady event will follow which will proceed to the main activity
                        if (!ILibraryController.InitWalletFromRecoveryPhrase(recoveryPhrase, password.joinToString("")))
                            internalErrorAlert(this, "$TAG init failed")
                    }
                },
                cancelled = fun() {button_accept_recovery_phrase.visibility = View.VISIBLE}
        )
    }

    @Suppress("UNUSED_PARAMETER")
    fun onAcknowledgeRecoveryPhrase(view: View)
    {
        updateView()
    }

    private fun updateView()
    {
        setFauxButtonEnabledState(button_accept_recovery_phrase, acknowledge_recovery_phrase.isChecked)
    }

    internal inner class ActionBarCallBack : ActionMode.Callback
    {
        override fun onActionItemClicked(mode: ActionMode, item: MenuItem): Boolean
        {
            return if (item.itemId == R.id.item_copy_to_clipboard)
            {
                val clipboard = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                val clip = ClipData.newPlainText("backup", recoveryPhraseTrimmed)
                clipboard.setPrimaryClip(clip)
                mode.finish()
                Toast.makeText(applicationContext, R.string.recovery_phrase_copy, Toast.LENGTH_LONG).show()
                true
            }
            else false

        }


        override fun onCreateActionMode(mode: ActionMode, menu: Menu): Boolean
        {
            mode.menuInflater.inflate(R.menu.share_menu, menu)

            // Handle buy button
            val itemBuy = menu.findItem(R.id.item_buy_coins)
            itemBuy.isVisible = false

            // Handle copy button
            //MenuItem itemCopy = menu.findItem(R.id.item_copy_to_clipboard);

            // Handle share button
            val itemShare = menu.findItem(R.id.action_share)
            shareActionProvider = ShareActionProvider(this@ShowRecoveryPhraseActivity)
            MenuItemCompat.setActionProvider(itemShare, shareActionProvider)

            val intent = Intent(Intent.ACTION_SEND)
            intent.type = "text/plain"
            intent.putExtra(Intent.EXTRA_TEXT, recoveryPhraseTrimmed)
            shareActionProvider!!.setShareIntent(intent)

            @Suppress("DEPRECATION")
            val color = resources.getColor(R.color.colorPrimary)
            val spannableString = SpannableString(recovery_phrase_text_view.text)
            spannableString.setSpan(BackgroundColorSpan(color), 0, recovery_phrase_text_view.text.length, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
            recovery_phrase_text_view.text = spannableString

            return true
        }

        override fun onDestroyActionMode(mode: ActionMode)
        {
            shareActionProvider = null

            recovery_phrase_text_view.text = recoveryPhraseTrimmed ?: ""
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
