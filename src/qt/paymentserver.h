// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PAYMENTSERVER_H
#define BITCOIN_QT_PAYMENTSERVER_H

// This class handles payment requests from clicking on
// Gulden: URIs
//
// This is somewhat tricky, because we have to deal with
// the situation where the user clicks on a link during
// startup/initialization, when the splash-screen is up
// but the main window (and the Send Coins tab) is not.
//
// So, the strategy is:
//
// Create the server, and register the event handler,
// when the application is created. Save any URIs
// received at or during startup in a list.
//
// When startup is finished and the main window is
// shown, a signal is sent to slot uiReady(), which
// emits a receivedURL() signal for any payment

#include "paymentrequestplus.h"
#include "walletmodel.h"

#include <QObject>
#include <QString>

class OptionsModel;

class CWallet;

QT_BEGIN_NAMESPACE
class QApplication;
class QByteArray;
class QLocalServer;
class QNetworkAccessManager;
class QNetworkReply;
class QSslError;
class QUrl;
QT_END_NAMESPACE

extern const qint64 BIP70_MAX_PAYMENTREQUEST_SIZE;

class PaymentServer : public QObject {
    Q_OBJECT

public:
    static void ipcParseCommandLine(int argc, char* argv[]);

    static bool ipcSendCommandLine();

    PaymentServer(QObject* parent, bool startLocalServer = true);
    ~PaymentServer();

    static void LoadRootCAs(X509_STORE* store = NULL);

    static X509_STORE* getCertStore() { return certStore; }

    void setOptionsModel(OptionsModel* optionsModel);

    static bool verifyNetwork(const payments::PaymentDetails& requestDetails);

    static bool verifyExpired(const payments::PaymentDetails& requestDetails);

    static bool verifySize(qint64 requestSize);

    static bool verifyAmount(const CAmount& requestAmount);

Q_SIGNALS:

    void receivedPaymentRequest(SendCoinsRecipient);

    void receivedPaymentACK(const QString& paymentACKMsg);

    void message(const QString& title, const QString& message, unsigned int style);

public Q_SLOTS:

    void uiReady();

    void fetchPaymentACK(CWallet* wallet, SendCoinsRecipient recipient, QByteArray transaction);

    void handleURIOrFile(const QString& s);

private Q_SLOTS:
    void handleURIConnection();
    void netRequestFinished(QNetworkReply*);
    void reportSslErrors(QNetworkReply*, const QList<QSslError>&);
    void handlePaymentACK(const QString& paymentACKMsg);

protected:
    bool eventFilter(QObject* object, QEvent* event);

private:
    static bool readPaymentRequestFromFile(const QString& filename, PaymentRequestPlus& request);
    bool processPaymentRequest(const PaymentRequestPlus& request, SendCoinsRecipient& recipient);
    void fetchRequest(const QUrl& url);

    void initNetManager();

    bool saveURIs; // true during startup
    QLocalServer* uriServer;

    static X509_STORE* certStore; // Trusted root certificates
    static void freeCertStore();

    QNetworkAccessManager* netManager; // Used to fetch payment requests

    OptionsModel* optionsModel;
};

#endif // BITCOIN_QT_PAYMENTSERVER_H
