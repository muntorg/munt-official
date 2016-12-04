// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "nocksrequest.h"
#include "utilmoneystr.h"
#include "arith_uint256.h"
#include "bitcoinunits.h"

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

#include "walletmodel.h"


#include <ostream>
#include <iomanip>
#include <sstream>

#include <openssl/x509_vfy.h>

NocksRequest::NocksRequest( QObject* parent, SendCoinsRecipient* recipient, RequestType type, QString from, QString to, QString amount)
: optionsModel( NULL )
, m_recipient( recipient )
, requestType(type)
{
    netManager = new QNetworkAccessManager( this );

    connect( netManager, SIGNAL( finished( QNetworkReply* ) ), this, SLOT( netRequestFinished( QNetworkReply* ) ) );
    connect( netManager, SIGNAL( sslErrors( QNetworkReply*, const QList<QSslError>& ) ), this, SLOT( reportSslErrors( QNetworkReply*, const QList<QSslError>& ) ) );
    
        
    QNetworkRequest netRequest;
    QString httpPostParamaters;
    
    if (requestType == RequestType::Quotation)
    {
        httpPostParamaters = QString("{\"pair\": \"%1_%2\", \"amount\": \"%3\", \"fee\": \"yes\"}").arg(from, to, amount);
        netRequest.setUrl( QString::fromStdString( "https://nocks.co/api/price" ) );
    }
    else
    {
        if (m_recipient)
        {
            originalAddress = m_recipient->address.toStdString();
        }
        
        //fixme: (SEPA)
        QString httpExtraParams = "";
        /*
        if (forexExtraName != null && !forexExtraName.isEmpty())
            httpExtraParams = httpExtraParams + String.format(", \"name\": \"%s\"", forexExtraName);
        if (forexExtraRemmitance1 != null && !forexExtraRemmitance1.isEmpty())
            httpExtraParams = httpExtraParams + String.format(", \"text\": \"%s\"", forexExtraRemmitance1);
        if (forexExtraRemmitance2 != null && !forexExtraRemmitance2.isEmpty())
            httpExtraParams = httpExtraParams + String.format(", \"reference\": \"%s\"", forexExtraRemmitance2);*/
        
        QString forexCurrencyType;
        if (recipient->paymentType == SendCoinsRecipient::PaymentType::BCOINPayment)
            forexCurrencyType = "BTC";
        else if (recipient->paymentType == SendCoinsRecipient::PaymentType::IBANPayment)
            forexCurrencyType = "EUR";
            
        //Stop infinite recursion.
        if (m_recipient)
        {
            m_recipient->forexPaymentType = m_recipient->paymentType;
            m_recipient->forexAmount = m_recipient->amount;
            m_recipient->paymentType = SendCoinsRecipient::PaymentType::NormalPayment;
        }
            
        QString forexAmount = BitcoinUnits::format(BitcoinUnits::BTC, recipient->amount, false, BitcoinUnits::separatorNever);
    
        httpPostParamaters = QString("{\"pair\": \"NLG_%1\", \"amount\": \"%2\", \"withdrawal\": \"%3\"%4}").arg(forexCurrencyType, forexAmount, recipient->address, httpExtraParams);
        netRequest.setUrl( QString::fromStdString( "https://nocks.co/api/transaction" ) );
    }
    
    netRequest.setRawHeader( "User-Agent", "Gulden-qt" );
    netRequest.setRawHeader( "Accept", "application/json" );
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QSslConfiguration config( QSslConfiguration::defaultConfiguration() );
    netRequest.setSslConfiguration( config );
    
    QByteArray data = httpPostParamaters.toStdString().c_str();

    netManager->post( netRequest, data );
}

NocksRequest::~NocksRequest()
{

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
    if ( reply->error() != QNetworkReply::NetworkError::NoError )
    {
        //fixme: Better error code
        m_recipient->forexFailCode = "Nocks is temporarily unreachable, please try again later.";
        Q_EMIT requestProcessed();
        return;
    }
    else
    {
        int statusCode = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();

        if ( statusCode < 200 && statusCode > 202 )
        {
            //fixme: Better error code
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
            QString temp = QString::fromStdString( jsonReply.data() );
            QJsonDocument jsonDoc = QJsonDocument::fromJson( jsonReply );
            QJsonValue successValue = jsonDoc.object().value( "success" );
            QJsonValue errorValue = jsonDoc.object().value( "error" );
            
            if (errorValue != QJsonValue::Undefined)
            {
                if (m_recipient)
                {
                    m_recipient->forexFailCode = errorValue.toArray().at(0).toString().toStdString();
                    if (m_recipient->forexFailCode.empty())
                        m_recipient->forexFailCode = "Nocks is temporarily unreachable, please try again later.";
                    Q_EMIT requestProcessed();
                }
                return;
            }
            else if (successValue == QJsonValue::Undefined)
            {
                //fixme: Better error code
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
                    nativeAmount = successValue.toObject().value("amount").toString();
                }
                else
                {
                    QString depositAmount = successValue.toObject().value("depositAmount").toString();
                    QString depositAddress = successValue.toObject().value("deposit").toString();
                    QString expirationTime = successValue.toObject().value("expirationTimestamp").toString();
                    QString withdrawalAmount = successValue.toObject().value("withdrawalAmount").toString();
                    QString withdrawalAddress = successValue.toObject().value("withdrawalOriginal").toString();
                    
                    //fixme: Should check amount adds up as well, but can't because of fee... - Manually subtract fee and verify it all adds up?
                    if (withdrawalAddress.toStdString() != originalAddress )
                    {
                        m_recipient->forexFailCode = "Withdrawal address modified, please contact a developer for assistance.";
                        Q_EMIT requestProcessed();
                        return;
                    }
                    m_recipient->paymentType = SendCoinsRecipient::PaymentType::NormalPayment;
                    m_recipient->forexAddress = QString::fromStdString(originalAddress);
                    m_recipient->address = depositAddress;
                    BitcoinUnits::parse(BitcoinUnits::BTC, depositAmount, &m_recipient->amount);
                    m_recipient->expiry = expirationTime.toLong() - 120;
                    m_recipient->forexFailCode = "";
                }
                
                Q_EMIT requestProcessed();
                return;
            }
        }
    }
    //this->deleteLater();
}

void NocksRequest::reportSslErrors( QNetworkReply* reply, const QList<QSslError>& errorList )
{
    m_recipient->forexFailCode = "Nocks is temporarily unreachable, please try again later.";
    Q_EMIT requestProcessed();
    
    //Delete ourselves.
    this->deleteLater();
}





