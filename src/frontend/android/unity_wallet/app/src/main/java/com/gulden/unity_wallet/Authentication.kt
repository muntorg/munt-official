// Copyright (c) 2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Context
import android.text.Editable
import android.text.TextWatcher
import android.text.format.DateUtils
import android.util.Log
import android.view.LayoutInflater
import android.view.animation.AnimationUtils
import android.view.inputmethod.InputMethodManager
import androidx.preference.PreferenceManager
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.unity_wallet.Constants.ACCESS_CODE_ATTEMPTS_ALLOWED
import com.gulden.unity_wallet.Constants.ACCESS_CODE_LENGTH
import kotlinx.android.synthetic.main.access_code_entry.view.*
import kotlinx.android.synthetic.main.access_code_recovery.view.*
import org.jetbrains.anko.alert
import org.jetbrains.anko.appcompat.v7.Appcompat


private const val TAG = "authentication"

private const val BLOCK_DURATION = 241 * DateUtils.MINUTE_IN_MILLIS
private const val BLOCKED_UNTIL_KEY = "authentication-blocked-until"
private const val NUM_FAILED_ATTEMPTS_KEY = "authentication-num-failed-attempts"

class Authentication {
    interface LockingObserver {
        fun onUnlock()
        fun onLock()
    }

    fun addObserver(lockingObserver: LockingObserver) {
        lockingObservers.add(lockingObserver)
    }

    fun removeObserver(lockingObserver: LockingObserver) {
        lockingObservers.remove(lockingObserver)
    }

    fun isLocked(): Boolean {
        return locked
    }

    fun unlock(context: Context, title: String?, msg: String?) {
        unlock(context, title, msg) { }
    }

    /**
     * Unlock after successful authentication. All lockingObservers are
     * notified and then action is executed.
     *
     */
    //TODO: lock()/unlock() should rather be driven by signals coming from the unity core.
    private fun unlock(context: Context, title: String?, msg: String?, action: () -> Unit) {
        if (isLocked()) {
            authenticate(context, title, msg) {
                locked = false
                lockingObservers.forEach {
                    it.onUnlock()
                }
                action()
            }
        } else // not locked just execute action
            action()
    }

    //TODO: lock()/unlock() should rather be driven by signals coming from the unity core.
    fun lock() {
        if (!isLocked()) {
            locked = true
            lockingObservers.forEach { it.onLock() }
        }
    }

    private fun isBlocked(context: Context): Boolean {
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        val blockedUntil = preferences.getLong(BLOCKED_UNTIL_KEY, 0)
        val now = System.currentTimeMillis()
        return now < blockedUntil
    }

    fun block(context: Context) {
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        val editor = preferences.edit()
        val blockUntil = System.currentTimeMillis() + BLOCK_DURATION
        editor.putLong(BLOCKED_UNTIL_KEY, blockUntil)
        editor.apply()

        resetFailedAttempts(context)
    }

    private fun unblock(context: Context) {
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        val editor = preferences.edit()
        editor.remove(BLOCKED_UNTIL_KEY)
        editor.apply()
    }

    fun showBlocking(context: Context) {
        context.alert(Appcompat) {
            this.title = context.getString(R.string.authentication_blocked_title)
            val preferences = PreferenceManager.getDefaultSharedPreferences(context)
            val blockedUntil = preferences.getLong(BLOCKED_UNTIL_KEY, 0)
            positiveButton(context.getString(R.string.authentication_blocked_later_btn)) {}
            // TODO: recovery using the mnemonic is not possible because the (core) wallet needs to be unlocked to verify the mnemonic
            // so this is a chicken egg problem. it could be fixed for example by storing a hashed version of the mnemonic for this purpose
            if (false /*GuldenUnifiedBackend.IsMnemonicWallet()*/) {
                this.message = context.getString(R.string.authentication_blocked_msg_recovery).format(
                        DateUtils.getRelativeTimeSpanString(blockedUntil, System.currentTimeMillis(), 0, 0).toString().toLowerCase())
                neutralPressed(context.getString(R.string.authentication_blocked_choose_new_btn)) {
                    chooseNewWithRecovery(context)
                }
            }
            else {
                this.message = context.getString(R.string.authentication_blocked_msg).format(
                        DateUtils.getRelativeTimeSpanString(blockedUntil, System.currentTimeMillis(), 0, 0).toString().toLowerCase())
            }
        }.build().show()
    }

    private fun chooseNewWithRecovery(context: Context) {
        val contentView = LayoutInflater.from(context).inflate(R.layout.access_code_recovery, null)
        context.alert(Appcompat) {
            customView = contentView
            this.title = context.getString(R.string.authentication_blocked_title)
            negativeButton(android.R.string.cancel) { }
            positiveButton(android.R.string.ok) {
                if (GuldenUnifiedBackend.IsMnemonicCorrect(contentView.recoveryPhrase.text.toString())) {
                    chooseAccessCode(context, null) {
                        unblock(context)
                    }
                } else {
                    context.alert(Appcompat, context.getString(R.string.access_code_recovery_incorrect),
                            context.getString(R.string.authentication_blocked_title)) {
                        positiveButton(android.R.string.ok) {}
                    }.build().show()
                }
            }
        }.build().show()
    }

    /**
     * Present user with authentication method.
     * On successful authentication action is executed.
     */
    fun authenticate(context: Context, title: String?, msg: String?, action: (CharArray) -> Unit) {


        if (isBlocked(context)) {
            showBlocking(context)
            return
        }

        val contentView = LayoutInflater.from(context).inflate(R.layout.access_code_entry, null)
        msg?.let { contentView.message.text = msg }

        val builder = context.alert(Appcompat) {
            this.title = title ?: context.getString(R.string.access_code_entry_title)
            customView = contentView
            negativeButton("Cancel") {
            }
        }

        val dialog = builder.build()
        dialog.setOnShowListener {
            contentView.accessCode.addTextChangedListener(
                    object : TextWatcher {

                        override fun afterTextChanged(s: Editable?) {
                            if (s?.length == ACCESS_CODE_LENGTH)
                            {
                                var chosenCode = CharArray(ACCESS_CODE_LENGTH)
                                s.getChars(0, s.length, chosenCode, 0)

                                if (GuldenUnifiedBackend.UnlockWallet(chosenCode.joinToString(""))) {
                                    Log.i(TAG, "successful authentication")
                                    it.dismiss()
                                    action(chosenCode)
                                } else {
                                    var numAttemptsRemaining = ACCESS_CODE_ATTEMPTS_ALLOWED - incFailedAttempts(context)
                                    if (numAttemptsRemaining > 0) {
                                        s.clear()
                                        contentView.accessCode.startAnimation(AnimationUtils.loadAnimation(context, R.anim.shake))
                                        contentView.attemptsRemaining.text = context.resources.getQuantityString(R.plurals.access_code_entry_remaining, numAttemptsRemaining, numAttemptsRemaining)
                                    } else {
                                        Log.i(TAG, "failed authentication")
                                        block(context)
                                        showBlocking(context)
                                        it.dismiss()
                                    }
                                }
                                s.clear()
                                @Suppress("UNUSED_VALUE")
                                chosenCode = CharArray(ACCESS_CODE_LENGTH)
                            }
                        }

                        override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                        override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
                    })

            contentView.accessCode.requestFocus()
            contentView.accessCode.post {
                val imm = context.applicationContext.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                imm.showSoftInput(contentView.accessCode, InputMethodManager.SHOW_IMPLICIT)
            }
        }
        dialog.show()
    }

    fun chooseAccessCode(context: Context, title: String?, action: (CharArray) -> Unit) {
        val contentView = LayoutInflater.from(context).inflate(R.layout.access_code_entry, null)

        val builder = context.alert(Appcompat) {
            this.title = title ?: context.getString(R.string.access_code_choose_title)
            customView = contentView
            negativeButton("Cancel") {
            }
        }

        val dialog = builder.build()
        dialog.setOnShowListener {
            contentView.accessCode.addTextChangedListener(
                    object : TextWatcher {
                        var verifying = false
                        var chosenCode: CharArray = CharArray(ACCESS_CODE_LENGTH)
                        override fun afterTextChanged(s: Editable?) {
                            if (s?.length == ACCESS_CODE_LENGTH)
                            {
                                var code = CharArray(ACCESS_CODE_LENGTH)
                                s.getChars(0, s.length, code, 0)
                                if (verifying) {
                                    if (code.contentEquals(chosenCode)) {
                                        Log.i(TAG, "access code successfully chosen")
                                        it.dismiss()
                                        action(chosenCode)
                                        s.clear()
                                        chosenCode = CharArray(ACCESS_CODE_LENGTH)
                                        @Suppress("UNUSED_VALUE")
                                        code = CharArray(ACCESS_CODE_LENGTH)
                                    }
                                    else {
                                        verifying = false
                                        s.clear()
                                        chosenCode = CharArray(ACCESS_CODE_LENGTH)
                                        @Suppress("UNUSED_VALUE")
                                        code = CharArray(ACCESS_CODE_LENGTH)
                                        contentView.accessCode.startAnimation(AnimationUtils.loadAnimation(context, R.anim.shake))
                                        contentView.message.text = ""
                                    }
                                }
                                else {
                                    chosenCode = code
                                    verifying = true
                                    s.clear()
                                    @Suppress("UNUSED_VALUE")
                                    code = CharArray(ACCESS_CODE_LENGTH)
                                    contentView.message.text = context.getString(R.string.access_code_choose_verify)
                                }
                            }
                        }

                        override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                        override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
                    })

            contentView.accessCode.requestFocus()
            contentView.accessCode.post {
                val imm = context.applicationContext.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                imm.showSoftInput(contentView.accessCode, InputMethodManager.SHOW_IMPLICIT)
            }
        }
        dialog.show()
    }

    private fun incFailedAttempts(context: Context): Int {
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        val numFailedAttempts = 1 + preferences.getInt(NUM_FAILED_ATTEMPTS_KEY, 0)
        val editor = preferences.edit()
        editor.putInt(NUM_FAILED_ATTEMPTS_KEY, numFailedAttempts)
        editor.apply()
        return numFailedAttempts
    }

    private fun resetFailedAttempts(context: Context) {
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        val editor = preferences.edit()
        editor.remove(NUM_FAILED_ATTEMPTS_KEY)
        editor.apply()
    }

    private var lockingObservers: MutableSet<LockingObserver> = mutableSetOf()
    private var locked = true

    companion object {
        val instance: Authentication = Authentication()
    }
}
