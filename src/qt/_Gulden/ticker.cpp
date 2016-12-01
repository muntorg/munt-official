// Copyright (c) 2015 The Gulden developers.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ticker.h"
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


#include <ostream>
#include <iomanip>
#include <sstream>

#include <openssl/x509_vfy.h>

CurrencyTableModel::CurrencyTableModel(QObject *parent, CurrencyTicker* ticker)
: QAbstractTableModel( parent )
, m_ticker( ticker )
, m_currenciesMap( NULL )
{
}

int CurrencyTableModel::rowCount(const QModelIndex& parent) const
{
    if (m_currenciesMap)
        return m_currenciesMap->size();
    
    return 0;
}

int CurrencyTableModel::columnCount(const QModelIndex & parent) const
{
    return 3;
}

QVariant CurrencyTableModel::data(const QModelIndex& index, int role) const
{
    if (!m_currenciesMap)
        return QVariant();
    
    if (index.row() < 0 || index.row() >= (int)m_currenciesMap->size() || role != Qt::DisplayRole)
    {
        return QVariant();
    }
    
    auto iter = m_currenciesMap->begin();
    std::advance(iter, index.row());
    
    if (index.column() == 0)
    {
        return QString::fromStdString(iter->first);
    }
    if (index.column() == 1)
    {
        return QString("rate<br/>balance");
    }
    if (index.column() == 2)
    {
        CAmount temp;
        ParseMoney(iter->second,temp);
        //fixme: GULDEN Truncates - we should instead round here...
        QString rate = BitcoinUnits::format(BitcoinUnits::Unit::BTC, temp, false, BitcoinUnits::separatorAlways, 4);
        QString balance = BitcoinUnits::format(BitcoinUnits::Unit::BTC, m_ticker->convertGuldenToForex(m_balanceNLG, iter->first), false, BitcoinUnits::separatorAlways, 2);
        return rate + QString("<br/>") + balance;
    }
    
    return QVariant();
}

    
void CurrencyTableModel::setBalance(CAmount balanceNLG)
{
    if (balanceNLG != m_balanceNLG)
    {
        m_balanceNLG = balanceNLG;
    }
}

void CurrencyTableModel::balanceChanged(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    setBalance(balance + unconfirmedBalance + immatureBalance);
    Q_EMIT dataChanged(index(0, 0, QModelIndex()), index(rowCount()-1, columnCount()-1, QModelIndex()));
}

CurrencyTicker::CurrencyTicker( QObject* parent )
: optionsModel( NULL )
, count(0)
{
    netManager = new QNetworkAccessManager( this );

    connect( netManager, SIGNAL( finished( QNetworkReply* ) ), this, SLOT( netRequestFinished( QNetworkReply* ) ) );
    connect( netManager, SIGNAL( sslErrors( QNetworkReply*, const QList<QSslError>& ) ), this, SLOT( reportSslErrors( QNetworkReply*, const QList<QSslError>& ) ) );
}

CurrencyTicker::~CurrencyTicker()
{

}

void CurrencyTicker::setOptionsModel( OptionsModel* optionsModel_ )
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

CAmount CurrencyTicker::convertGuldenToForex(CAmount guldenAmount, std::string forexCurrencyCode)
{
    if (m_ExchangeRates.find(forexCurrencyCode) != m_ExchangeRates.end())
    {
        CAmount exchangeRate;
        if ( ParseMoney(m_ExchangeRates[forexCurrencyCode], exchangeRate) )
        {
            arith_uint256 forexAmountBN = guldenAmount;
            forexAmountBN *= exchangeRate;
            forexAmountBN /= COIN;
            return forexAmountBN.GetLow64();
        }
    }
    return CAmount(0);
}


CAmount CurrencyTicker::convertForexToGulden(CAmount forexAmount, std::string forexCurrencyCode)
{
    if (m_ExchangeRates.find(forexCurrencyCode) != m_ExchangeRates.end())
    {
        CAmount exchangeRate;
        if ( ParseMoney(m_ExchangeRates[forexCurrencyCode], exchangeRate) )
        {
            arith_uint256 forexAmountBN = forexAmount;
            forexAmountBN /= exchangeRate;
            forexAmountBN *= COIN;
            return forexAmountBN.GetLow64();
        }
    }
    return CAmount(0);
}

CurrencyTableModel* CurrencyTicker::GetCurrencyTableModel()
{
    CurrencyTableModel* model = new CurrencyTableModel(this, this);
    model->setMap(&m_ExchangeRates);
    model->setBalance(CAmount(0));
    return model;
}

void CurrencyTicker::pollTicker()
{
    QNetworkRequest netRequest;
    netRequest.setUrl( QString::fromStdString( "https://api.gulden.com/api/v1/ticker" ) );
    netRequest.setRawHeader( "User-Agent", "Gulden-qt" );
    netRequest.setRawHeader( "Accept", "application/json" );

    QSslConfiguration config( QSslConfiguration::defaultConfiguration() );
    netRequest.setSslConfiguration( config );

    netManager->get( netRequest );
}

void CurrencyTicker::netRequestFinished( QNetworkReply* reply )
{
    bool signalUpdates = false;

    if ( reply->error() != QNetworkReply::NetworkError::NoError )
    {
        //fixme: Error handling code here.
        //Note - it is possible the ticker has temporary outages etc. and these are not a major issue
        //We update every ~10s but if we miss a few updates it has no ill-effects
        //So if we do anything here, it should only be after multiple failiures...
    }
    else
    {
        int statusCode = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();

        if ( statusCode != 200 )
        {
            //fixme: Error handling code here.
            //Note - it is possible the ticker has temporary outages etc. and these are not a major issue
            //We update every ~10s but if we miss a few updates it has no ill-effects
            //So if we do anything here, it should only be after multiple failiures...
        }
        else
        {
            QByteArray jsonReply = reply->readAll();
            QString temp = QString::fromStdString( jsonReply.data() );
            QJsonDocument jsonDoc = QJsonDocument::fromJson( jsonReply );
            QJsonArray jsonArray = jsonDoc.object().value( "data" ).toArray();

            for ( auto jsonVal : jsonArray )
            {
                QJsonObject jsonObject = jsonVal.toObject();
                std::string currencyName = jsonObject.value( "name" ).toString().toStdString();
                std::string currencyCode = jsonObject.value( "code" ).toString().toStdString();
                //NB! Possible precision loss here - but in this case it is probably acceptible.
                std::string currencyRate;
                std::ostringstream bufstream;
                bufstream << std::fixed << std::setprecision(8) << jsonObject.value( "rate" ).toDouble();
                currencyRate = bufstream.str();

                m_ExchangeRates[currencyCode] = currencyRate;
                signalUpdates = true;
            }
        }
    }

    if ( signalUpdates )
    {
        Q_EMIT exchangeRatesUpdated();
        if(++count % 20 == 0)
        {
            Q_EMIT exchangeRatesUpdatedLongPoll();
        }
    }

    // Call again every ~10 seconds
    QTimer::singleShot( 10000, this, SLOT(pollTicker()) );
}

void CurrencyTicker::reportSslErrors( QNetworkReply* reply, const QList<QSslError>& errorList )
{
    //fixme: In future (I guess) we should signal in UI somehow that ticker is unavailable - need to decide how to do this in a user friendly way.
    //Note - it is possible the ticker has temporary outages - is switching hosts or whatever other minor thing 
    //We update every ~10s but if we miss a few updates it has no ill-effects
    //So if we do anything here, it should only be after multiple failiures...
}





