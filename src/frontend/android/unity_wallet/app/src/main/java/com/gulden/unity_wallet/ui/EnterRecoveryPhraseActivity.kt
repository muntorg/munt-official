package com.gulden.unity_wallet.ui

import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.MenuItem
import android.view.View
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.MultiAutoCompleteTextView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.*
import com.gulden.unity_wallet.util.gotoWalletActivity
import kotlinx.android.synthetic.main.activity_enter_recovery_phrase.*
import org.jetbrains.anko.backgroundDrawable
import org.jetbrains.anko.sdk27.coroutines.textChangedListener

private const val TAG = "enter-recovery-activity"

class EnterRecoveryPhraseActivity : AppCompatActivity(), UnityCore.Observer
{
    private val recoveryPhrase: String
        get() = recover_from_phrase_text_view.text.toString().trim { it <= ' ' }

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

        recover_from_phrase_text_view.run {
            addTextChangedListener(object : TextWatcher {
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
            imeOptions = EditorInfo.IME_ACTION_DONE
            setOnEditorActionListener(TextView.OnEditorActionListener { _, actionId, _ ->
                if (actionId == EditorInfo.IME_ACTION_DONE)
                {
                    val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                    imm.hideSoftInputFromWindow(windowToken, 0)
                    return@OnEditorActionListener true
                }
                false
            })
            setTokenizer(SpaceTokenizer())
        }

        supportActionBar?.hide()

        updateView()

        UnityCore.instance.addObserver(this, fun (callback:() -> Unit) { runOnUiThread { callback() }})
    }

    override fun onDestroy() {
        super.onDestroy()

        UnityCore.instance.removeObserver(this)
    }

    override fun onCoreReady(): Boolean {
        gotoWalletActivity(this)
        return true
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
        if (!GuldenUnifiedBackend.IsValidRecoveryPhrase(recoveryPhrase))
        {
            Toast.makeText(applicationContext, "Invalid recovery phrase", Toast.LENGTH_LONG).show()
            return;
        }

        Authentication.instance.chooseAccessCode(this) {
            password->
            if (UnityCore.instance.isCoreReady()) {
                if (GuldenUnifiedBackend.ContinueWalletFromRecoveryPhrase(recoveryPhrase, password.joinToString(""))) {
                    gotoWalletActivity(this)
                } else {
                    internalErrorAlert(this, "$TAG continuation failed")
                }
            } else {
                // Create the new wallet, a coreReady event will follow which will proceed to the main activity
                if (!GuldenUnifiedBackend.InitWalletFromRecoveryPhrase(recoveryPhrase, password.joinToString("")))
                    internalErrorAlert(this, "$TAG init failed")
            }
        }
    }

    fun updateView()
    {
        // Toggle button visual disabled/enabled indicator while still keeping it clickable
        var buttonBackground = R.drawable.shape_rounded_button_disabled
        if (GuldenUnifiedBackend.IsValidRecoveryPhrase(recoveryPhrase))
            buttonBackground = R.drawable.shape_rounded_button_enabled
        recover_from_phrase_proceed_button.setBackgroundResource(buttonBackground);
    }

}
