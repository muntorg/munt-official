// Copyright (c) 2015 The Guldencoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "GuldencoinTranslator.h"
#include <QMessageBox>

QGuldencoinTranslator::QGuldencoinTranslator(QObject * parent )
: QTranslator(parent)
, isFallback(false)
{
}

QGuldencoinTranslator::QGuldencoinTranslator(bool isFallback_)
: QTranslator()
, isFallback(isFallback_)
{
}

QGuldencoinTranslator::~QGuldencoinTranslator()
{
}

#if QT_VERSION >= 0x050000
QString QGuldencoinTranslator::translate(const char * context, const char * sourceText, const char * disambiguation, int n ) const
#else
QString QGuldencoinTranslator::translate(const char * context, const char * sourceText, const char * disambiguation) const
#endif
{
    // Load original translated string.
    #if QT_VERSION >= 0x050000
    QString translatedString = QTranslator::translate(context, sourceText, disambiguation);
    #else
    QString translatedString = QTranslator::translate(context, sourceText, disambiguation, n);
    #endif

    if (isFallback)
        translatedString = sourceText;

    // Make some application specific replacements.
    translatedString.replace("bitcoin.com","guldencoin.com");
    translatedString.replace("Bitcoin","Gulden");
    translatedString.replace("bitcoin:","guldencoin:");
    translatedString.replace("bitcoin","gulden");
    translatedString.replace("BITCOIN","GULDEN");
    translatedString.replace("BTC","NLG");
    translatedString.replace("bitcoin:","guldencoin:");
    translatedString.replace("bitcoin.conf","guldencoin.conf");
    translatedString.replace("bitcoin-cli","guldencoin-cli");
    translatedString.replace("bitcoin-tx","guldencoin-tx");
    translatedString.replace("bitcoin-qt","guldencoin-qt");
    translatedString.replace("bitcoind","guldencoind");
    translatedString.replace("bitcoin","gulden");
    translatedString.replace("Bitcoin","Gulden");
    translatedString.replace("BITCOIN","GULDEN");
    translatedString.replace("BTC","NLG");
    translatedString.replace("btc","nlg");


    // Use result instead of original - this allows us to easily track upstream translations without having to constantly merge translation files.
    return translatedString;
}
