// Copyright (c) 2015 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "GuldenTranslator.h"
#include <Gulden/translate.h>
#include <QMessageBox>

GuldenTranslator::GuldenTranslator(QObject * parent )
: QTranslator(parent)
, isFallback(false)
{
}

GuldenTranslator::GuldenTranslator(bool isFallback_)
: QTranslator()
, isFallback(isFallback_)
{
}

GuldenTranslator::~GuldenTranslator()
{
}

#if QT_VERSION >= 0x050000
QString GuldenTranslator::translate(const char * context, const char * sourceText, const char * disambiguation, int n ) const
#else
QString GuldenTranslator::translate(const char * context, const char * sourceText, const char * disambiguation) const
#endif
{
    // Load original translated string.
    #if QT_VERSION >= 0x050000
    QString translatedString = QTranslator::translate(context, sourceText, disambiguation, n);
    #else
    QString translatedString = QTranslator::translate(context, sourceText, disambiguation);
    #endif

    if (isFallback)
        translatedString = sourceText;

    // Make some application specific replacements.
    #define STR_TRANS(S,F,R) S.replace(F,R);
    GULDEN_TRANSLATIONS_REPLACE(STR_TRANS, translatedString);

    // Use result instead of original - this allows us to easily track upstream translations without having to constantly merge translation files.
    return translatedString;
}
