// Copyright (c) 2015-2018 The Gulden developers
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "updatecheck.h"
#include "clientversion.h"
#include "hash.h"
#include "key.h"
#include "pubkey.h"
#include "utilstrencodings.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

/* Update server REST protocol

Query string parameters accepted by the UPDATE_CHECK_ENDPOINT:
- version, currently running Gulden version
- systype, operating system type as returned by QSysInfo::productType() , ie. windows, osx, debian etc
- sysver, operating system version as return by QSysInfo::productVersion(), ie. 10 (for Windows 10)
- sysarch, system CPU architecture as returned by QSysInfo::currentCpuArchitecture(), ie. x86_64, arm etc.

For documentation of these function see http://doc.qt.io/qt-5/qsysinfo.html#currentCpuArchitecture

The server responds with a json object with fields:
- msg, rich text that can be shown to the user this may include a link to a download page
- important, bool if the message should be shown to the user or that it is ok to silently ignore it. Typically
             if availability of a new version is important, while a message that the latest version is already
             running would not be important.

Repsonse signing

The update server signs the response. The hex encoding of the der signature of the Sha256 hash of
the response body is put in the ECSignature header.
*/

//! Update server REST API endpoint
const char* UPDATE_CHECK_ENDPOINT = "https://api.guldentools.com/v1/updatecheck";

//! Update server public key to verify ECSignature
const std::vector<unsigned char> UPDATE_PUB_KEY =
        ParseHex("042c788c9f3ade6818f4ff4a553a7ffeceac40ca0c413fa6deb71e65b258e2e52b9069c937f573faf55f1f4ac7ba69d9b356f4385a8c81378d5d26daae421e187e");

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
    netRequest.setHeader(QNetworkRequest::UserAgentHeader, QByteArray::fromStdString(UserAgent()));
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
    bool succes = false;
    QString msg = "Unknown failure checking for update";
    bool important = false;

    QNetworkReply* reply = networkReply;
    if (reply->error() == QNetworkReply::NetworkError::NoError && reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt() == 200)
    {
        QByteArray headerName = QByteArray::fromStdString("ECSignature");
        if (reply->hasRawHeader(headerName))
        {
            auto signature = ParseHex(reply->rawHeader(headerName).toStdString());

            QByteArray jsonReply = reply->readAll();

            std::vector<unsigned char> hash;
            hash.resize(CSHA256::OUTPUT_SIZE);
            CSHA256().Write((unsigned char*)jsonReply.data(), jsonReply.length()).Finalize(&hash[0]);

            CPubKey pubKey(UPDATE_PUB_KEY);
            if (pubKey.Verify(uint256(hash), signature))
            {
                QJsonDocument jsonDoc = QJsonDocument::fromJson( jsonReply );
                jsonResponse(jsonDoc);
                succes = true;
            }
            else
                msg = "Update server invalid signature.";
        }
        else
            msg = "Update server did not provide a signature.";
    }
    else
        msg = "Error requesting update ";


    if (!succes)
    {
        Q_EMIT result(succes, msg, important, noisy);
    }

    networkReply->deleteLater();
    networkReply = nullptr;
}

void UpdateCheck::check(bool _noisy)
{
    noisy = _noisy;
    QUrl url(UPDATE_CHECK_ENDPOINT);
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
