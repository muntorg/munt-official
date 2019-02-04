package com.gulden.unity_wallet.ui

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.core.view.MenuItemCompat
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.view.ActionMode
import androidx.appcompat.widget.ShareActionProvider
import android.text.Spannable
import android.text.SpannableString
import android.text.style.BackgroundColorSpan
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.TextView
import android.widget.Toast
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.CoreObserverProxy

import com.gulden.unity_wallet.WalletActivity
import com.gulden.unity_wallet.R
import com.gulden.unity_wallet.UnityCore

class ShowRecoveryPhraseActivity : AppCompatActivity(), UnityCore.Observer
{
    private val coreObserverProxy = CoreObserverProxy(this, this)

    internal var recoveryPhraseView: TextView? = null
    internal var recoveryPhraseAcknowledgeCheckBox: CheckBox? = null
    internal var recoveryPhraseAcceptButton: Button? = null
    //fixme: (GULDEN) Change to char[] to we can securely wipe.
    internal var recoveryPhrase: String? = null

    val isNewWallet: Boolean?
        get() = !intent.hasExtra(this.packageName + "do_not_start_wallet_activity_on_close")

    private var shareActionProvider: ShareActionProvider? = null

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_show_recovery_phrase)

        recoveryPhraseAcknowledgeCheckBox = findViewById(R.id.acknowledge_recovery_phrase)

        recoveryPhraseAcceptButton = findViewById(R.id.button_accept_recovery_phrase)

        recoveryPhraseView = findViewById(R.id.recovery_phrase_text_view)

        recoveryPhrase = intent.getStringExtra(this.packageName + "recovery_phrase")
        recoveryPhraseView!!.text = recoveryPhrase
        recoveryPhraseView!!.setOnClickListener { setFocusOnRecoveryPhrase() }


        // If we are not coming here from the 'welcome page' then we want a back button in title bar.
        if (!(isNewWallet!!))
        {
            //Sort action bar out
            run {
                supportActionBar?.setDisplayShowTitleEnabled(false)
                supportActionBar?.setDisplayShowCustomEnabled(true)
                supportActionBar?.setDisplayShowHomeEnabled(true)
                supportActionBar?.setDisplayHomeAsUpEnabled(true)
            }

            recoveryPhraseAcceptButton?.visibility = View.GONE
        }
        else
        {
            supportActionBar?.hide()
        }

        updateView()

        UnityCore.instance.addObserver(coreObserverProxy)
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
        UnityCore.instance.removeObserver(coreObserverProxy)
        //fixme: (GULDEN) Securely wipe.
        recoveryPhrase = ""
        super.onDestroy()
    }

    override fun onCoreReady(): Boolean {
        // Proceed to main activity
        val intent = Intent(this, WalletActivity::class.java)
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        startActivity(intent)
        finish()
        return true
    }

    @Suppress("UNUSED_PARAMETER")
    fun onAcceptRecoveryPhrase(view: View)
    {
        if (isNewWallet!!)
        {
            // Create the new wallet
            GuldenUnifiedBackend.InitWalletFromRecoveryPhrase(recoveryPhrase)

            // a coreReady event will follow which will proceed to the main activity
        }
        else
            finish()
    }

    @Suppress("UNUSED_PARAMETER")
    fun onAcknowledgeRecoveryPhrase(view: View)
    {
        updateView()
    }

    fun updateView()
    {
        if (isNewWallet!!)
        {
            // Only allow user to move on once they have acknowledged writing the recovery phrase down.
            recoveryPhraseAcceptButton!!.isEnabled = recoveryPhraseAcknowledgeCheckBox!!.isChecked
        }
        else
        {
            recoveryPhraseAcknowledgeCheckBox?.visibility = View.INVISIBLE
        }
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
            val spannableString = SpannableString(recoveryPhraseView!!.text)
            spannableString.setSpan(BackgroundColorSpan(color), 0, recoveryPhraseView!!.text.length, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
            recoveryPhraseView!!.text = spannableString

            return true
        }

        override fun onDestroyActionMode(mode: ActionMode)
        {
            shareActionProvider = null

            recoveryPhraseView!!.text = recoveryPhrase
        }

        override fun onPrepareActionMode(mode: ActionMode, menu: Menu): Boolean
        {
            return false
        }
    }

    fun setFocusOnRecoveryPhrase()
    {
        startSupportActionMode(ActionBarCallBack())
    }

}
