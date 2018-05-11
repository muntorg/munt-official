// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_QT_GULDENADDRESSVALIDATOR_H
#define GULDEN_QT_GULDENADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class GuldenAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit GuldenAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

/** Gulden address widget validator, checks for a valid Gulden address.
 */
class GuldenAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit GuldenAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

#endif // GULDEN_QT_GULDENADDRESSVALIDATOR_H
