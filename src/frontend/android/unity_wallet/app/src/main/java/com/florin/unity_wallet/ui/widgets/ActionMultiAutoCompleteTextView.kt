// Copyright (c) 2018 The Florin developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the Florin software license, see the accompanying
// file COPYING

package com.florin.unity_wallet.ui.widgets

import android.content.Context
import androidx.appcompat.widget.AppCompatMultiAutoCompleteTextView
import android.util.AttributeSet
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection

class ActionMultiAutoCompleteTextView : AppCompatMultiAutoCompleteTextView
{
    constructor(context: Context) : super(context)

    constructor(context: Context, attrs: AttributeSet) : super(context, attrs)

    constructor(context: Context, attrs: AttributeSet, defStyle: Int) : super(context, attrs, defStyle)

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection
    {
        val conn = super.onCreateInputConnection(outAttrs)
        outAttrs.imeOptions = outAttrs.imeOptions and EditorInfo.IME_FLAG_NO_ENTER_ACTION.inv()
        return conn
    }
}
