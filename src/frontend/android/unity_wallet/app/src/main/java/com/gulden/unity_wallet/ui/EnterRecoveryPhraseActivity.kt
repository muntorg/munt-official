package com.gulden.unity_wallet.ui

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.text.Editable
import android.text.TextWatcher
import android.view.KeyEvent
import android.view.MenuItem
import android.view.View
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.CheckBox
import android.widget.MultiAutoCompleteTextView
import android.widget.TextView
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.MainActivity

import com.gulden.unity_wallet.R


class EnterRecoveryPhraseActivity : AppCompatActivity()
{

    private var proceedButton: Button? = null
    private var recoveryPhraseEditText: MultiAutoCompleteTextView? = null
    private var recoverFromPhraseWipeText: TextView? = null
    private var overwriteWalletCheckbox: CheckBox? = null

    val recoveryPhrase: String
        get() = recoveryPhraseEditText!!.text.toString().trim { it <= ' ' }

    val isNewWallet: Boolean?
        get() = !intent.hasExtra(this.packageName + "dont_start_wallet_activity_on_close")


    inner class SpaceTokenizer : MultiAutoCompleteTextView.Tokenizer
    {
        override fun findTokenStart(text: CharSequence, cursor: Int): Int
        {
            var i = cursor

            while (i > 0 && text[i - 1] != ' ')
            {
                i--
            }
            while (i < cursor && text[i] == ' ')
            {
                i++
            }

            return i
        }

        override fun findTokenEnd(text: CharSequence?, cursor: Int): Int
        {
            var len = 0
            if (text != null)
            {
                var i = cursor
                len = text.length

                while (i < len)
                {
                    if (text[i] == ' ')
                    {
                        return i
                    }
                    else
                    {
                        i++
                    }
                }
            }

            return len
        }

        override fun terminateToken(text: CharSequence): CharSequence
        {
            return text.toString().trim { it <= ' ' } + " "
        }
    }

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_enter_recovery_phrase)

        recoverFromPhraseWipeText = findViewById(R.id.recoverFromPhraseWipeText)

        overwriteWalletCheckbox = findViewById(R.id.recovery_phrase_acknowledge_wallet_overwrite)

        recoveryPhraseEditText = findViewById(R.id.recover_from_phrase_text_view)
        recoveryPhraseEditText!!.addTextChangedListener(object : TextWatcher
        {
            override fun afterTextChanged(s: Editable)
            {
                updateView()
            }

            override fun beforeTextChanged(s: CharSequence, start: Int, count: Int, after: Int)
            {

            }

            override fun onTextChanged(s: CharSequence, start: Int, before: Int, count: Int)
            {

            }

        })

        recoveryPhraseEditText!!.imeOptions = EditorInfo.IME_ACTION_DONE
        recoveryPhraseEditText!!.setOnEditorActionListener(TextView.OnEditorActionListener { v, actionId, event ->
            if (actionId == EditorInfo.IME_ACTION_DONE)
            {
                val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                imm.hideSoftInputFromWindow(recoveryPhraseEditText!!.windowToken, 0)
                return@OnEditorActionListener true
            }
            false
        })

        proceedButton = findViewById(R.id.recover_from_phrase_proceed_button)

        recoveryPhraseEditText?.setTokenizer(SpaceTokenizer())

        // If we are not coming here from the 'welcome page' then we want a back button in title bar.
        if ((isNewWallet!!))
        {
            supportActionBar?.hide()
        }
        else
        {
            //Sort action bar out
            run {
                supportActionBar?.setDisplayShowTitleEnabled(false)
                supportActionBar?.setDisplayShowCustomEnabled(true)
                supportActionBar?.setDisplayShowHomeEnabled(true)
                supportActionBar?.setDisplayHomeAsUpEnabled(true)
            }
        }

        updateView()
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

    fun onAcceptRecoverFromPhrase(view: View)
    {
        if (GuldenUnifiedBackend.InitWalletFromRecoveryPhrase(recoveryPhrase))
        {
            // Proceed to main activity
            val intent = Intent(this, MainActivity::class.java)
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            startActivity(intent)
            finish()
        }
        else
        {
            //TODO: Display error to user...
        }
    }

    fun onAcknowledgeOverwrite(view: View)
    {
        updateView()
    }

    fun updateView()
    {
        if (GuldenUnifiedBackend.IsValidRecoveryPhrase(recoveryPhrase))
        {
            val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            imm.hideSoftInputFromWindow(recoveryPhraseEditText!!.windowToken, 0)

            proceedButton?.isEnabled = true

            if (!overwriteWalletCheckbox!!.isChecked && (isNewWallet!!))
            {
                proceedButton?.isEnabled = false
            }
        }
        else
        {
            proceedButton!!.isEnabled = false
        }

        if (isNewWallet!!)
        {
            recoverFromPhraseWipeText?.visibility = View.GONE
            overwriteWalletCheckbox?.visibility = View.GONE
            findViewById<View>(R.id.recoverFromPhraseEULA).visibility = View.VISIBLE
        }
        else
        {
            recoverFromPhraseWipeText?.visibility = View.VISIBLE
            overwriteWalletCheckbox?.visibility = View.VISIBLE
            findViewById<View>(R.id.recoverFromPhraseEULA).visibility = View.GONE
        }
    }

}
