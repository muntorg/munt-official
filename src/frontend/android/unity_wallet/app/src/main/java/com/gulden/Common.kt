package com.gulden.unity_wallet

import android.net.Uri
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.UriRecipient
import com.gulden.jniunifiedbackend.UriRecord
import java.util.HashMap

fun Uri.getParameters(): HashMap<String, String> {
    val items : HashMap<String, String> = HashMap<String, String>()
    if (isOpaque)
        return items

    val query = encodedQuery ?: return items

    var start = 0
    do {
        val nextAmpersand = query.indexOf('&', start)
        val end = if (nextAmpersand != -1) nextAmpersand else query.length

        var separator = query.indexOf('=', start)
        if (separator > end || separator == -1) {
            separator = end
        }

        if (separator == end) {
            items[Uri.decode(query.substring(start, separator))] = ""
        } else {
            items[Uri.decode(query.substring(start, separator))] = Uri.decode(query.substring(separator + 1, end))
        }

        // Move start to end of name.
        if (nextAmpersand != -1) {
            start = nextAmpersand + 1
        } else {
            break
        }
    } while (true)
    return items
}

fun uriRecicpient(text: String): UriRecipient {
    var parsedQRCodeURI = Uri.parse(text)
    var address: String = ""

    // Handle all possible scheme variations (foo: foo:// etc.)
    if (parsedQRCodeURI?.authority == null && parsedQRCodeURI?.path == null) {
        parsedQRCodeURI = Uri.parse(text.replaceFirst(":", "://"))
    }
    if (parsedQRCodeURI?.authority != null) address += parsedQRCodeURI.authority
    if (parsedQRCodeURI?.path != null) address += parsedQRCodeURI.path

    val parsedQRCodeURIRecord = UriRecord(parsedQRCodeURI.scheme, address, parsedQRCodeURI.getParameters())
    return GuldenUnifiedBackend.IsValidRecipient(parsedQRCodeURIRecord)
}
