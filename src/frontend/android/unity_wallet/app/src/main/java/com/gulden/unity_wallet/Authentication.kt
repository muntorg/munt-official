// Copyright (c) 2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Context
import android.text.Editable
import android.text.TextWatcher
import android.util.Log
import android.view.LayoutInflater
import android.view.animation.AnimationUtils
import android.view.inputmethod.InputMethodManager
import com.gulden.unity_wallet.Constants.ACCESS_CODE_ATTEMPTS_ALLOWED
import com.gulden.unity_wallet.Constants.ACCESS_CODE_LENGTH
import com.gulden.unity_wallet.Constants.FAKE_ACCESS_CODE
import kotlinx.android.synthetic.main.access_code_entry.view.*
import org.jetbrains.anko.alert
import org.jetbrains.anko.appcompat.v7.Appcompat

private const val TAG = "authentication"

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
    fun unlock(context: Context, title: String?, msg: String?, action: () -> Unit) {
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

    fun lock() {
        if (!isLocked()) {
            locked = true
            lockingObservers.forEach { it.onLock() }
        }
    }

    /**
     * Present user with authentication method.
     * On successful authentication action is executed.
     */
    fun authenticate(context: Context, title: String?, msg: String?, action: () -> Unit) {
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
                        // TODO block locking/authentication for time period after too many attempts
                        var numAttemptsRemaining = ACCESS_CODE_ATTEMPTS_ALLOWED
                        override fun afterTextChanged(s: Editable?) {
                            val code = s.toString()
                            if (code.length == ACCESS_CODE_LENGTH) {
                                // TODO actual access code checking
                                if (code == FAKE_ACCESS_CODE) {
                                    Log.i(TAG, "successful authentication USING FAKE ACCESS CHECKING")
                                    it.dismiss()
                                    action()
                                } else {
                                    numAttemptsRemaining -= 1
                                    if (numAttemptsRemaining > 0) {
                                        s?.clear()
                                        contentView.accessCode.startAnimation(AnimationUtils.loadAnimation(context, R.anim.shake))
                                        contentView.attemptsRemaining.text = context.getString(R.string.access_code_entry_remaining).format(numAttemptsRemaining)
                                    } else {
                                        Log.i(TAG, "failed authentication")
                                        it.dismiss()
                                    }
                                }
                            }
                        }

                        override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                        override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
                    })

            contentView.accessCode.requestFocus()
            val imm = context.applicationContext.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            imm.showSoftInput(contentView.accessCode, InputMethodManager.SHOW_IMPLICIT)
        }
        dialog.show()
    }

    fun chooseAccessCode(context: Context, action: () -> Unit) {
        val contentView = LayoutInflater.from(context).inflate(R.layout.access_code_entry, null)

        val builder = context.alert(Appcompat) {
            this.title = context.getString(R.string.access_code_choose_title)
            customView = contentView
            negativeButton("Cancel") {
            }
        }

        val dialog = builder.build()
        dialog.setOnShowListener {
            contentView.accessCode.addTextChangedListener(
                    object : TextWatcher {
                        var verifying = false
                        lateinit var choosenCode: String
                        override fun afterTextChanged(s: Editable?) {
                            val code = s.toString()
                            if (code.length == ACCESS_CODE_LENGTH) {
                                if (verifying) {
                                    if (code == choosenCode) {
                                        Log.i(TAG, "access code successfully chosen TODO SECURELY STORE")
                                        it.dismiss()
                                        action()
                                    }
                                    else {
                                        verifying = false
                                        s?.clear()
                                        contentView.accessCode.startAnimation(AnimationUtils.loadAnimation(context, R.anim.shake))
                                        contentView.message.text = ""
                                    }
                                }
                                else {
                                    choosenCode = code
                                    verifying = true
                                    s?.clear()
                                    contentView.message.text = context.getString(R.string.access_code_choose_verify)
                                }
                            }
                        }

                        override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                        override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
                    })

            contentView.accessCode.requestFocus()
            val imm = context.applicationContext.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            imm.showSoftInput(contentView.accessCode, InputMethodManager.SHOW_IMPLICIT)
        }
        dialog.show()
    }

    private var lockingObservers: MutableSet<LockingObserver> = mutableSetOf()
    private var locked = true

    companion object {
        val instance: Authentication = Authentication()
    }
}
