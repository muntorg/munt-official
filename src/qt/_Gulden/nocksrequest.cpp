// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "nocksrequest.h"
#include "utilmoneystr.h"
#include "arith_uint256.h"
#include "units.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QVariant>
#include <QModelIndex>
#include <QDialog>

#include "walletmodel.h"
#include "gui.h"



#include <ostream>
#include <iomanip>
#include <sstream>

#include <openssl/x509_vfy.h>

static QString nocksHost()
{
    if (IsArgSet("-testnet"))
        return QString("sandbox.nocks.com");
    else
        return QString("api.nocks.com");
}

NocksRequest::NocksRequest( QObject* parent)
: optionsModel( nullptr )
, m_recipient( nullptr )
, networkReply( nullptr )
{
    netManager = new QNetworkAccessManager( this );
    netManager->setObjectName("nocks_request_manager");

    connect( netManager, SIGNAL( finished( QNetworkReply* ) ), this, SLOT( netRequestFinished( QNetworkReply* ) ) );
    connect( netManager, SIGNAL( sslErrors( QNetworkReply*, const QList<QSslError>& ) ), this, SLOT( reportSslErrors( QNetworkReply*, const QList<QSslError>& ) ) );
}

NocksRequest::~NocksRequest()
{
    cancel();
    delete netManager;
}

void NocksRequest::startRequest(SendCoinsRecipient* recipient, RequestType type, QString from, QString to, QString amount, QString description)
{
    assert(networkReply == nullptr);

    m_recipient = recipient;
    requestType = type;

    QNetworkRequest netRequest;
    QString httpPostParamaters;

    if (requestType == RequestType::Quotation)
    {
        httpPostParamaters = QString("{\"source_currency\": \"%1\", \"target_currency\": \"%2\", \"amount\": {\"amount\": \"%3\", \"currency\": \"%2\"}, \"payment_method\": {\"method\":\"gulden\"}}").arg(from, to, amount);
        netRequest.setUrl( QString("https://%1/api/v2/transaction/quote").arg(nocksHost()));
    }
    else
    {
        if (m_recipient)
        {
            originalAddress = m_recipient->address.toStdString();
        }

        QString forexCurrencyType;
        if (recipient->paymentType == SendCoinsRecipient::PaymentType::BitcoinPayment)
            forexCurrencyType = "BTC";
        else if (recipient->paymentType == SendCoinsRecipient::PaymentType::IBANPayment)
            forexCurrencyType = "EUR";

        //Stop infinite recursion.
        if (m_recipient)
        {
            m_recipient->forexPaymentType = m_recipient->paymentType;
            m_recipient->forexAmount = m_recipient->amount;
            m_recipient->paymentType = SendCoinsRecipient::PaymentType::NormalPayment;
            description = m_recipient->forexDescription;
        }
               
        //fixme: (Post-2.1) (SEPA)
        QString httpExtraParams = "";
        /*
        if (forexExtraName != null && !forexExtraName.isEmpty())
            httpExtraParams = httpExtraParams + String.format(", \"name\": \"%s\"", forexExtraName);*/
        if (!description.isEmpty())
            httpExtraParams = httpExtraParams + ", \"name\": \"" + description + "\"";
        /*if (forexExtraRemmitance2 != null && !forexExtraRemmitance2.isEmpty())
            httpExtraParams = httpExtraParams + String.format(", \"reference\": \"%s\"", forexExtraRemmitance2);*/

        QString forexAmount = GuldenUnits::format(GuldenUnits::NLG, recipient->amount, false, GuldenUnits::separatorNever);

        //fixme: Pass a refund_address here
        httpPostParamaters = QString("{\"source_currency\": \"%1\", \"target_currency\": \"%2\", \"target_address\": \"%3\", \"amount\": {\"amount\": \"%4\", \"currency\": \"%2\"}, \"payment_method\": {\"method\":\"gulden\"}%5}").arg(from, to, recipient->address, forexAmount, httpExtraParams);
        netRequest.setUrl( QString("https://%1/api/v2/transaction").arg(nocksHost()));
    }

    netRequest.setRawHeader( "User-Agent", QByteArray(UserAgent().c_str()));
    netRequest.setRawHeader( "Accept", "application/json" );
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QSslConfiguration config( QSslConfiguration::defaultConfiguration() );
    netRequest.setSslConfiguration( config );

    QByteArray data = httpPostParamaters.toStdString().c_str();

    networkReply = netManager->post( netRequest, data );
}

void NocksRequest::cancel()
{
    netManager->disconnect(this);

    if (networkReply)
    {
        networkReply->abort();
        networkReply->deleteLater();
        networkReply = nullptr;
    }
}

void NocksRequest::setOptionsModel( OptionsModel* optionsModel_ )
{
    /*optionsModel = optionsModel;

    if ( optionsModel )
    {
        QNetworkProxy proxy;

        // Query active SOCKS5 proxy
        if ( optionsModel->getProxySettings( proxy ) )
        {
            netManager->setProxy( proxy );
            qDebug() << "PaymentServer::initNetManager: Using SOCKS5 proxy" << proxy.hostName() << ":" << proxy.port();
        }
        else
            qDebug() << "PaymentServer::initNetManager: No active proxy server found.";
    }*/
}

void NocksRequest::netRequestFinished( QNetworkReply* reply )
{
    assert(networkReply == reply);

    if ( reply->error() != QNetworkReply::NetworkError::NoError )
    {
        //fixme: (FUT) Better error code
        if (m_recipient)
            m_recipient->forexFailCode = "Nocks is temporarily unreachable, please try again later.";
        Q_EMIT requestProcessed();
        return;
    }
    else
    {
        int statusCode = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();

        if ( statusCode < 200 && statusCode > 202 )
        {
            //fixme: (FUT)  Better error code
            if (m_recipient)
            {
                m_recipient->forexFailCode = "Nocks is temporarily unreachable, please try again later.";
                Q_EMIT requestProcessed();
            }
            return;
        }
        else
        {
            QByteArray jsonReply = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson( jsonReply );
            QJsonValue errorValue = jsonDoc.object().value( "error" );
            if (errorValue != QJsonValue::Undefined)
            {
                if (m_recipient)
                {
                    m_recipient->forexFailCode = errorValue.isString() ?
                                errorValue.toString().toStdString()
                              : "Could not process your request, please try again later.";
                    Q_EMIT requestProcessed();
                }
                return;
            }
            
            QJsonValue dataObject = jsonDoc.object().value("data");            
            QJsonValue sourceAmount = dataObject.toObject().value("source_amount");
            QJsonValue targetAmount = dataObject.toObject().value("target_amount");
            

            if((sourceAmount == QJsonValue::Undefined) || (targetAmount == QJsonValue::Undefined))
            {
                //fixme: (FUT)  Better error code
                if (m_recipient)
                {
                    m_recipient->forexFailCode = "Nocks is temporarily unreachable, please try again later.";
                    Q_EMIT requestProcessed();
                }
                return;
            }
            else
            {
                if (requestType == RequestType::Quotation)
                {
                    GuldenUnits::parse(GuldenUnits::NLG, sourceAmount.toObject().value("amount").toString(), &nativeAmount);
                }
                else
                {
                    QString depositAmount = sourceAmount.toObject().value("amount").toString();
                    QString withdrawalAmount = targetAmount.toObject().value("amount").toString();
                    
                    // If on testnet then display the payment URL so user can verify it.
                    if (IsArgSet("-testnet"))
                    {
                        QString paymentURL = dataObject.toObject().value("payment_url").toString(); 
                        QString message = paymentURL;
                        QDialog* d = GUI::createDialog(nullptr, message, tr("Okay"), QString(""), 400, 180);
                        d->exec();
                    }
                    
                    //fixme: (FUT)  Should check amount adds up as well, but can't because of fee... - Manually subtract fee and verify it all adds up?
                    QString withdrawalAddress = dataObject.toObject().value("target_address").toString();               
                    if (withdrawalAddress.toStdString() != originalAddress )
                    {
                        m_recipient->forexFailCode = "Withdrawal address modified, please contact a developer for assistance.";
                        Q_EMIT requestProcessed();
                        return;
                    }
                    m_recipient->paymentType = SendCoinsRecipient::PaymentType::NormalPayment;
                    m_recipient->forexAddress = QString::fromStdString(originalAddress);
                    m_recipient->address = dataObject.toObject().find("payments")->toObject().find("data")->toArray()[0].toObject().find("metadata")->toObject().find("address")->toString();
                    GuldenUnits::parse(GuldenUnits::NLG, depositAmount, &m_recipient->amount);
                    m_recipient->expiry = dataObject.toObject().value("expire_at").toObject().value("timestamp").toVariant().toLongLong();
                    m_recipient->forexFailCode = "";
                }

                Q_EMIT requestProcessed();
                return;
            }
        }
    }
}

void NocksRequest::reportSslErrors( [[maybe_unused]] QNetworkReply* reply, [[maybe_unused]] const QList<QSslError>& errorList )
{
    if (m_recipient)
        m_recipient->forexFailCode = "Nocks is temporarily unreachable, please try again later.";
    Q_EMIT requestProcessed();
}





