// Copyright (c) 2015 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_TRANSLATION_H
#define GULDEN_TRANSLATION_H

#include <QTranslator>

class GuldenTranslator : public QTranslator
{
    Q_OBJECT
public:
    GuldenTranslator(QObject * parent = 0);
    GuldenTranslator(bool isFallback);
    ~GuldenTranslator();
#if QT_VERSION >= 0x050000
    virtual QString translate(const char * context, const char * sourceText, const char * disambiguation = 0, int n=-1) const;
#else
    virtual QString translate(const char * context, const char * sourceText, const char * disambiguation = 0) const;
#endif
    bool isEmpty() const { return false; }
private:
    bool isFallback;
};

#endif

