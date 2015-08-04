// Copyright (c) 2015 The Guldencoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDENCOIN_TRANSLATION_H
#define GULDENCOIN_TRANSLATION_H

#include <QTranslator>

class QGuldencoinTranslator : public QTranslator
{
    Q_OBJECT
public:
    QGuldencoinTranslator(QObject * parent = 0);
    QGuldencoinTranslator(bool isFallback);
    ~QGuldencoinTranslator();
    virtual QString translate(const char * context, const char * sourceText, const char * disambiguation = 0, int n=-1) const;
    bool isEmpty() const { return false; }
private:
    bool isFallback;
};

#endif

