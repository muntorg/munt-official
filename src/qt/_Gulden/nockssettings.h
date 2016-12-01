// Copyright (c) 2016 The Gulden developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_NOCKSSETTINGS_H
#define GULDEN_NOCKSSETTINGS_H

#include <string>
#include <map>

#include <QObject>
#include <QSslError>
#include <QList>
#include "amount.h"

class OptionsModel;
class QNetworkAccessManager;
class QNetworkReply;
class CurrencyTicker;



class NocksSettings : public QObject
{
    Q_OBJECT

public:
    // parent should be QApplication object
    NocksSettings(QObject* parent);
    ~NocksSettings();

    // OptionsModel is used for getting proxy settings and display unit
    void setOptionsModel(OptionsModel *optionsModel);
    
    CAmount getMinimumForCurrency(std::string symbol);
    CAmount getMaximumForCurrency(std::string symbol);
    std::string getMinimumForCurrencyAsString(std::string symbol);
    std::string getMaximumForCurrencyAsString(std::string symbol);

Q_SIGNALS:
    void nocksSettingsUpdated();

public Q_SLOTS:
    void pollSettings();

private Q_SLOTS:
    void netRequestFinished(QNetworkReply*);
    void reportSslErrors(QNetworkReply*, const QList<QSslError> &);

protected:
private:
    QNetworkAccessManager* netManager;
    OptionsModel* optionsModel;
    std::map<std::string, std::pair<std::string, std::string> > exchangeLimits;
};

#endif
