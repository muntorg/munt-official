// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_NOCKSREQUEST_H
#define GULDEN_NOCKSREQUEST_H

#include <string>
#include <map>

#include <QObject>
#include <QSslError>
#include <QList>


class OptionsModel;
class QNetworkAccessManager;
class QNetworkReply;
class CurrencyTicker;
class SendCoinsRecipient;


class NocksRequest : public QObject
{
    Q_OBJECT

public:
    
    enum RequestType
    {
        Quotation,
        Order
    };
    
    // parent should be QApplication object
    NocksRequest(QObject* parent, SendCoinsRecipient* recipient, RequestType type, QString from="", QString to="", QString amount="");
    ~NocksRequest();

    // OptionsModel is used for getting proxy settings and display unit
    void setOptionsModel(OptionsModel *optionsModel);
    
    QString nativeAmount;

Q_SIGNALS:
    void requestProcessed();

public Q_SLOTS:

private Q_SLOTS:
    void netRequestFinished(QNetworkReply*);
    void reportSslErrors(QNetworkReply*, const QList<QSslError> &);

protected:
private:
    QNetworkAccessManager* netManager;
    OptionsModel* optionsModel;
    SendCoinsRecipient* m_recipient;
    std::string originalAddress;
    RequestType requestType;
};

#endif
