// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef BITCOIN_QT_GULDENRECEIVECOINSDIALOG_H
#define BITCOIN_QT_GULDENRECEIVECOINSDIALOG_H

#include "guiutil.h"
#include "wallet/wallet.h"

#include <QDialog>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QVariant>
#include <QSslError>

class OptionsModel;
class PlatformStyle;
class WalletModel;
#define HAVE_WEBKIT
#if defined(HAVE_WEBENGINE_VIEW)
class QWebEngineView;
#elif defined(HAVE_WEBKIT)
class QWebView;
#endif
class CReserveKey;
class QWebEngineNewViewRequest;
class QNetworkReply;

namespace Ui {
class ReceiveCoinsDialog;
}

class ReceiveCoinsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ReceiveCoinsDialog(const PlatformStyle* platformStyle, QWidget* parent = 0);
    ~ReceiveCoinsDialog();

    void setModel(WalletModel* model);
    void updateQRCode(const QString& uri);
    void setActiveAccount(CAccount* account);

public Q_SLOTS:
    void updateAddress(const QString& address);
    void showBuyGuldenDialog();
    void gotoRequestPaymentPage();
    void cancelRequestPayment();
    void gotoReceievePage();
    void activeAccountChanged();

protected:
private:
    Ui::ReceiveCoinsDialog* ui;
    WalletModel* model;
    const PlatformStyle* platformStyle;
    QString accountAddress;
#if defined(HAVE_WEBENGINE_VIEW)
    QWebEngineView* buyView;
#elif defined(HAVE_WEBKIT)
    QWebView* buyView;
#endif
    CReserveKey* buyReceiveAddress;
    CAccount* currentAccount;

private Q_SLOTS:
    void copyAddressToClipboard();
    void saveQRAsImage();
    void loadBuyViewFinished(bool bOk);
    void generateRequest();
    void buyGulden();
    void sslErrorHandler(QNetworkReply* qnr, const QList<QSslError>& errlist);
};

#endif // BITCOIN_QT_GULDENRECEIVECOINSDIALOG_H
