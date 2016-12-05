// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "bitcoinaddressvalidator.h"

#include "base58.h"
#include <QRegularExpression>

/* Base58 characters are:
     "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"

  This is:
  - All numbers except for '0'
  - All upper-case letters except for 'I' and 'O'
  - All lower-case letters except for 'l'
*/

BitcoinAddressEntryValidator::BitcoinAddressEntryValidator(QObject* parent)
    : QValidator(parent)
{
}

QValidator::State BitcoinAddressEntryValidator::validate(QString& input, int& pos) const
{
    Q_UNUSED(pos);

    if (input.isEmpty())
        return QValidator::Intermediate;

    for (int idx = 0; idx < input.size();) {
        bool removeChar = false;
        QChar ch = input.at(idx);

        switch (ch.unicode()) {

        case 0x200B: // ZERO WIDTH SPACE
        case 0xFEFF: // ZERO WIDTH NO-BREAK SPACE
            removeChar = true;
            break;
        default:
            break;
        }

        if (ch.isSpace())
            removeChar = true;

        if (removeChar)
            input.remove(idx, 1);
        else
            ++idx;
    }

    /*QValidator::State state = QValidator::Acceptable;
    for (int idx = 0; idx < input.size(); ++idx)
    {
        int ch = input.at(idx).unicode();

        if (((ch >= '0' && ch<='9') ||
            (ch >= 'a' && ch<='z') ||
            (ch >= 'A' && ch<='Z')) &&
            ch != 'l' && ch != 'I' && ch != '0' && ch != 'O')
        {

        }
        else
        {
            state = QValidator::Invalid;
        }
    }*/

    return QValidator::Acceptable;
}

BitcoinAddressCheckValidator::BitcoinAddressCheckValidator(QObject* parent)
    : QValidator(parent)
{
}

QValidator::State BitcoinAddressCheckValidator::validate(QString& input, int& pos) const
{
    Q_UNUSED(pos);

    CBitcoinAddress addr(input.toStdString());

    QRegularExpression patternMatcherIBAN("^[a-zA-Z]{2,2}[0-9]{2,2}(?:[a-zA-Z0-9]{1,30})$");
    if (addr.IsValid() || addr.IsValidBCOIN() || patternMatcherIBAN.match(input).hasMatch())
        return QValidator::Acceptable;

    return QValidator::Invalid;
}
