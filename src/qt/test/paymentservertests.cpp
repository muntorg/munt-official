// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "paymentservertests.h"

#include "optionsmodel.h"
#include "paymentrequestdata.h"

#include "amount.h"
#include "random.h"
#include "script/script.h"
#include "script/standard.h"
#include "util.h"
#include "utilstrencodings.h"

#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include <QFileOpenEvent>
#include <QTemporaryFile>

X509* parse_b64der_cert(const char* cert_data)
{
    std::vector<unsigned char> data = DecodeBase64(cert_data);
    assert(data.size() > 0);
    const unsigned char* dptr = &data[0];
    X509* cert = d2i_X509(NULL, &dptr, data.size());
    assert(cert);
    return cert;
}

static SendCoinsRecipient handleRequest(PaymentServer* server, std::vector<unsigned char>& data)
{
    RecipientCatcher sigCatcher;
    QObject::connect(server, SIGNAL(receivedPaymentRequest(SendCoinsRecipient)),
                     &sigCatcher, SLOT(getRecipient(SendCoinsRecipient)));

    QTemporaryFile f;
    f.open();
    f.write((const char*)&data[0], data.size());
    f.close();

    QObject object;
    object.installEventFilter(server);
    QFileOpenEvent event(f.fileName());

    QCoreApplication::sendEvent(&object, &event);

    QObject::disconnect(server, SIGNAL(receivedPaymentRequest(SendCoinsRecipient)),
                        &sigCatcher, SLOT(getRecipient(SendCoinsRecipient)));

    return sigCatcher.recipient;
}

void PaymentServerTests::paymentServerTests()
{
    SelectParams(CBaseChainParams::MAIN);
    OptionsModel optionsModel;
    PaymentServer* server = new PaymentServer(NULL, false);
    X509_STORE* caStore = X509_STORE_new();
    X509_STORE_add_cert(caStore, parse_b64der_cert(caCert1_BASE64));
    PaymentServer::LoadRootCAs(caStore);
    server->setOptionsModel(&optionsModel);
    server->uiReady();

    std::vector<unsigned char> data;
    SendCoinsRecipient r;
    QString merchant;

    data = DecodeBase64(paymentrequest1_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString("testmerchant.org"));

    data = DecodeBase64(paymentrequest2_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString(""));

    data = DecodeBase64(paymentrequest3_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString("testmerchant8.org"));

    data = DecodeBase64(paymentrequest4_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString(""));

    data = DecodeBase64(paymentrequest5_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString(""));

    caStore = X509_STORE_new();
    PaymentServer::LoadRootCAs(caStore);
    data = DecodeBase64(paymentrequest1_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString(""));

    caStore = X509_STORE_new();
    X509_STORE_add_cert(caStore, parse_b64der_cert(caCert2_BASE64));
    PaymentServer::LoadRootCAs(caStore);

    QByteArray byteArray;

    data = DecodeBase64(paymentrequest1_cert2_BASE64);
    byteArray = QByteArray((const char*)&data[0], data.size());
    r.paymentRequest.parse(byteArray);

    QVERIFY(r.paymentRequest.IsInitialized());
    QCOMPARE(PaymentServer::verifyNetwork(r.paymentRequest.getDetails()), false);

    data = DecodeBase64(paymentrequest2_cert2_BASE64);
    byteArray = QByteArray((const char*)&data[0], data.size());
    r.paymentRequest.parse(byteArray);

    QVERIFY(r.paymentRequest.IsInitialized());

    QCOMPARE(PaymentServer::verifyExpired(r.paymentRequest.getDetails()), true);

    data = DecodeBase64(paymentrequest3_cert2_BASE64);
    byteArray = QByteArray((const char*)&data[0], data.size());
    r.paymentRequest.parse(byteArray);

    QVERIFY(r.paymentRequest.IsInitialized());

    QCOMPARE(PaymentServer::verifyExpired(r.paymentRequest.getDetails()), false);

    data = DecodeBase64(paymentrequest4_cert2_BASE64);
    byteArray = QByteArray((const char*)&data[0], data.size());
    r.paymentRequest.parse(byteArray);

    QVERIFY(r.paymentRequest.IsInitialized());

    QCOMPARE(PaymentServer::verifyExpired(r.paymentRequest.getDetails()), true);

    unsigned char randData[BIP70_MAX_PAYMENTREQUEST_SIZE + 1];
    GetRandBytes(randData, sizeof(randData));

    QTemporaryFile tempFile;
    tempFile.open();
    tempFile.write((const char*)randData, sizeof(randData));
    tempFile.close();

    QCOMPARE(PaymentServer::verifySize(tempFile.size()), false);

    data = DecodeBase64(paymentrequest5_cert2_BASE64);
    byteArray = QByteArray((const char*)&data[0], data.size());
    r.paymentRequest.parse(byteArray);

    QVERIFY(r.paymentRequest.IsInitialized());

    QList<std::pair<CScript, CAmount> > sendingTos = r.paymentRequest.getPayTo();
    Q_FOREACH (const PAIRTYPE(CScript, CAmount) & sendingTo, sendingTos) {
        CTxDestination dest;
        if (ExtractDestination(sendingTo.first, dest))
            QCOMPARE(PaymentServer::verifyAmount(sendingTo.second), false);
    }

    delete server;
}

void RecipientCatcher::getRecipient(SendCoinsRecipient r)
{
    recipient = r;
}
