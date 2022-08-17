package unity_wallet

import android.content.Context
import android.content.DialogInterface
import android.net.Uri
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import unity_wallet.jniunifiedbackend.ILibraryController
import unity_wallet.jniunifiedbackend.UriRecipient
import unity_wallet.jniunifiedbackend.UriRecord
import org.apache.commons.validator.routines.IBANValidator
import java.util.HashMap

fun Uri.getParameters(): HashMap<String, String> {
    val items : HashMap<String, String> = HashMap()
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

class InvalidRecipientException(message: String): RuntimeException(message)

fun createRecipient(text: String): UriRecipient {
    var uri = Uri.parse(text)

    // If Uri has been parsed as non-hierarchical force it to reparse as hierarchical so that we can access any query portions correctly.
    if (!uri.isHierarchical)
        uri = Uri.parse(text.replaceFirst(":", "://"))

    if (uri.scheme == null)
        uri = Uri.parse("gulden://$text")

    val address = uri.host ?: throw InvalidRecipientException("No recipient address")
    val amount = uri.getQueryParameter("amount")?.toDoubleOrZero()?.toNative() ?: 0
    val label = uri.getQueryParameter("label") ?: ""
    val description = uri.getQueryParameter("description") ?: ""

    if (IBANValidator.getInstance().isValid(address))
        return UriRecipient(false, address, label, description, amount)

    // if there is a scheme it should equal gulden or guldencoin, but that will be checked inside C++, so just pass it through
    val coreRecipient = ILibraryController.IsValidRecipient(UriRecord(uri.scheme, address, HashMap<String, String>()))
    if (!coreRecipient.valid)
        throw InvalidRecipientException("Core deemed recipient invalid")

    return UriRecipient(true, coreRecipient.address, if (coreRecipient.label != "") coreRecipient.label else label, if (coreRecipient.desc != "") coreRecipient.desc else description, amount)
}

fun ellipsizeString(sourceString: String, maxLength: Int): String
{
    //Remove 3 chars for "ellipses" length and then split into two equal halves
    val halfStringLength = (maxLength - 3) / 2
    return when
    {
        sourceString.length > maxLength -> sourceString.replaceFirst(Regex("(.{$halfStringLength}).+(.{$halfStringLength})"), "$1…$2")
        else -> sourceString
    }
}

fun internalErrorAlert(context: Context, msg: String) {
    MaterialAlertDialogBuilder(context)
            .setTitle("Internal error!")
            .setMessage("An unexpected error occurred, this is likely a bug. Please contact the developers.\n\n$msg")
            .setPositiveButton(android.R.string.ok) { dialogInterface: DialogInterface, i: Int -> }
            .show()
}

fun String.toDoubleOrZero(): Double {
    return toDoubleOrNull() ?: 0.0
}
