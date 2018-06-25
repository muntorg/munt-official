// Copyright (c) 2015-2018 The Gulden developers
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "updatecheck.h"
#include "clientversion.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QString>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSysInfo>
#include <QTimer>

#include <ostream>
#include <iomanip>
#include <sstream>

#include <openssl/x509_vfy.h>

UpdateCheck::UpdateCheck() :
  networkReply(nullptr)
{
    netManager = new QNetworkAccessManager(this);
}

UpdateCheck::~UpdateCheck()
{
    cancel();
    delete netManager;
}

void UpdateCheck::start(const QUrl& url)
{
    cancel();

    QNetworkRequest netRequest;
    netRequest.setUrl(url);
    // netRequest.setRawHeader( "User-Agent", QByteArray(UserAgent().c_str()));
    netRequest.setRawHeader( "Accept", "application/json" );

    QSslConfiguration config( QSslConfiguration::defaultConfiguration() );
    netRequest.setSslConfiguration( config );

    networkReply = netManager->get( netRequest );
    connect(networkReply, SIGNAL(finished()), this, SLOT(netRequestFinished()));
}

void UpdateCheck::cancel()
{
    if (networkReply)
    {
        networkReply->disconnect(this);
        networkReply->abort();
        networkReply->deleteLater();
        networkReply = nullptr;
    }
}

void UpdateCheck::netRequestFinished()
{
    QNetworkReply* reply = networkReply;
    if (reply->error() == QNetworkReply::NetworkError::NoError && reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt() == 200)
    {
        QByteArray jsonReply = reply->readAll();
        QString temp = QString::fromStdString( jsonReply.data() );
        QJsonDocument jsonDoc = QJsonDocument::fromJson( jsonReply );
        jsonResponse(jsonDoc);
    }
    else
    {
        Q_EMIT result(false, "const QString& msg", false, noisy);
    }

    networkReply->deleteLater();
    networkReply = nullptr;
}

void UpdateCheck::check(bool _noisy)
{
    noisy = _noisy;
    QUrl url("https://ecyt65hwyc.execute-api.eu-central-1.amazonaws.com/test/updatecheck");
    QUrlQuery query;
    query.addQueryItem("version", QString::fromStdString(Version()));
    query.addQueryItem("systype", QSysInfo::productType());
    query.addQueryItem("sysver", QSysInfo::productVersion());
    query.addQueryItem("sysarch", QSysInfo::currentCpuArchitecture());
    url.setQuery(query);
    start(url);
}

void UpdateCheck::jsonResponse(const QJsonDocument& json)
{
    bool important = json.object().value("important").toBool();
    QString msg = json.object().value("msg").toString("-- no message --");
    Q_EMIT result(true, msg, important, noisy);
}
