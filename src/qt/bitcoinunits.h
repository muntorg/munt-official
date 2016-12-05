// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef BITCOIN_QT_BITCOINUNITS_H
#define BITCOIN_QT_BITCOINUNITS_H

#include "amount.h"

#include <QAbstractListModel>
#include <QString>

// U+2009 THIN SPACE = UTF-8 E2 80 89
#define REAL_THIN_SP_CP 0x2009
#define REAL_THIN_SP_UTF8 "\xE2\x80\x89"
#define REAL_THIN_SP_HTML "&thinsp;"

#define HAIR_SP_CP 0x200A
#define HAIR_SP_UTF8 "\xE2\x80\x8A"
#define HAIR_SP_HTML "&#8202;"

#define SIXPEREM_SP_CP 0x2006
#define SIXPEREM_SP_UTF8 "\xE2\x80\x86"
#define SIXPEREM_SP_HTML "&#8198;"

#define FIGURE_SP_CP 0x2007
#define FIGURE_SP_UTF8 "\xE2\x80\x87"
#define FIGURE_SP_HTML "&#8199;"

#define HTML_HACK_SP "<span style='white-space: nowrap; font-size: 6pt'> </span>"

#define THIN_SP_CP REAL_THIN_SP_CP
#define THIN_SP_UTF8 REAL_THIN_SP_UTF8
#define THIN_SP_HTML HTML_HACK_SP

/** Bitcoin unit definitions. Encapsulates parsing and formatting
   and serves as list model for drop-down selection boxes.
*/
class BitcoinUnits : public QAbstractListModel {
    Q_OBJECT

public:
    explicit BitcoinUnits(QObject* parent);

    /** Bitcoin units.
      @note Source: https://en.bitcoin.it/wiki/Units . Please add only sensible ones
     */
    enum Unit {
        BTC,
        mBTC,
        uBTC
    };

    enum SeparatorStyle {
        separatorNever,
        separatorStandard,
        separatorAlways
    };

    static QList<Unit> availableUnits();

    static bool valid(int unit);

    static QString name(int unit);

    static QString description(int unit);

    static qint64 factor(int unit);

    static int decimals(int unit);

    static QString format(int unit, const CAmount& amount, bool plussign = false, SeparatorStyle separators = separatorStandard, int numDecimals = -1);

    static QString formatWithUnit(int unit, const CAmount& amount, bool plussign = false, SeparatorStyle separators = separatorStandard, int numDecimals = -1);

    static QString formatHtmlWithUnit(int unit, const CAmount& amount, bool plussign = false, SeparatorStyle separators = separatorStandard);

    static bool parse(int unit, const QString& value, CAmount* val_out);

    static QString getAmountColumnTitle(int unit);

    enum RoleIndex {
        /** Unit identifier */
        UnitRole = Qt::UserRole
    };
    int rowCount(const QModelIndex& parent) const;
    QVariant data(const QModelIndex& index, int role) const;

    static QString removeSpaces(QString text)
    {
        text.remove(' ');
        text.remove(QChar(THIN_SP_CP));
#if (THIN_SP_CP != REAL_THIN_SP_CP)
        text.remove(QChar(REAL_THIN_SP_CP));
#endif
        return text;
    }

    static CAmount maxMoney();

private:
    QList<BitcoinUnits::Unit> unitlist;
};
typedef BitcoinUnits::Unit BitcoinUnit;

#endif // BITCOIN_QT_BITCOINUNITS_H
