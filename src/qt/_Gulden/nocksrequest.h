// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_NOCKSREQUEST_H
#define GULDEN_NOCKSREQUEST_H

#include "amount.h"

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
    NocksRequest(QObject* parent);
    virtual ~NocksRequest();

    /** Start request.
        Connect to the requestProcessed() signal BEFORE starting the request. Though the request
        is asynchrounous it is possible that it finishes early and the signal would be missed.
    */
    void startRequest(SendCoinsRecipient* recipient, RequestType type, QString from="", QString to="", QString amount="");

    //! Cancel the request. The requestProcessed() signal will not be triggered.
    void cancel();

    // OptionsModel is used for getting proxy settings and display unit
    void setOptionsModel(OptionsModel *optionsModel);

    CAmount nativeAmount;

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
    QNetworkReply* networkReply;
};

#endif
