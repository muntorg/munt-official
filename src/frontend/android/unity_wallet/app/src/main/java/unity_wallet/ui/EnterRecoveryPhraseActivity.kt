package unity_wallet.ui

import android.content.Context
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
import androidx.appcompat.app.AlertDialog
import unity_wallet.jniunifiedbackend.ILibraryController
import unity_wallet.Authentication
import unity_wallet.R
import unity_wallet.UnityCore
import unity_wallet.internalErrorAlert
import unity_wallet.util.AppBaseActivity
import unity_wallet.util.gotoWalletActivity
import unity_wallet.util.setFauxButtonEnabledState
import kotlinx.android.synthetic.main.activity_enter_recovery_phrase.*

private const val TAG = "enter-recovery-activity"

class EnterRecoveryPhraseActivity : AppBaseActivity(), UnityCore.Observer
{
    private val erasedWallet = UnityCore.instance.isCoreReady()

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

        recover_from_phrase_proceed_button.setOnClickListener{  onAcceptRecoverFromPhrase(it) }

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

    override fun onWalletReady() {
        if (!erasedWallet)
            gotoWalletActivity(this)
    }

    override fun onWalletCreate() {
        // do nothing, we are supposed to sit here until the wallet was created
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

    private fun chooseAccessCodeAndProceed(mnemonicPhrase : String)
    {
        // TODO must have core started and createWallet signal

        val originaButtonState = recover_from_phrase_proceed_button.visibility
        recover_from_phrase_proceed_button.visibility = View.INVISIBLE
        Authentication.instance.chooseAccessCode(
                this,
                null,
                action = { password->
                    if (UnityCore.instance.isCoreReady()) {
                        if (ILibraryController.ContinueWalletFromRecoveryPhrase(mnemonicPhrase, password.joinToString(""))) {
                            gotoWalletActivity(this)
                        } else {
                            internalErrorAlert(this, "$TAG continuation failed")
                        }
                    } else {
                        // Create the new wallet, a coreReady event will follow which will proceed to the main activity
                        if (!ILibraryController.InitWalletFromRecoveryPhrase(mnemonicPhrase, password.joinToString("")))
                            internalErrorAlert(this, "$TAG init failed")
                    }
                },
                cancelled = fun() {recover_from_phrase_proceed_button.visibility = originaButtonState}
        )
    }

    private fun promptUserForBirthDate()
    {
        val builder = AlertDialog.Builder(this)

        //TODO: Display info message 'R.string.wallet_birth_info' here (requires a custom dialog as AlertDialog can't handle both)
        //TODO: Create a nicer looking dialog here (look at using a bottom drawer for instance)
        builder.setTitle(getString(R.string.wallet_birth_heading))
                .setItems(R.array.wallet_birth_date) { dialog, selection ->
                    val unixTime = System.currentTimeMillis() / 1000L
                    when (selection)
                    {
                        0 -> chooseAccessCodeAndProceed(ILibraryController.ComposeRecoveryPhrase(recoveryPhrase, unixTime-(86400*31)).phraseWithBirthNumber)
                        1 -> chooseAccessCodeAndProceed(ILibraryController.ComposeRecoveryPhrase(recoveryPhrase, unixTime-(86400*365)).phraseWithBirthNumber)
                        2 -> chooseAccessCodeAndProceed(recoveryPhrase)
                    }
                }
        builder.create().show()
    }

    private fun onAcceptRecoverFromPhrase(view: View)
    {
        if (!ILibraryController.IsValidRecoveryPhrase(recoveryPhrase))
        {
            Toast.makeText(applicationContext, "Invalid recovery phrase", Toast.LENGTH_LONG).show()
            return
        }

        if (recoveryPhrase == recoveryPhrase.trimEnd('0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ' '))
        {
            promptUserForBirthDate()
        }
        else
        {
            chooseAccessCodeAndProceed(recoveryPhrase)
        }
    }

    fun updateView()
    {
        // Toggle button visual disabled/enabled indicator while still keeping it clickable
        setFauxButtonEnabledState(recover_from_phrase_proceed_button, ILibraryController.IsValidRecoveryPhrase(recoveryPhrase))
    }

}
