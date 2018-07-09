// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_GULDENRECEIVECOINSDIALOG_H
#define GULDEN_QT_GULDENRECEIVECOINSDIALOG_H

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
class QStyle;
class WalletModel;
#if defined(HAVE_WEBENGINE_VIEW)
class QWebEngineView;
#elif defined(HAVE_WEBKIT)
class QWebView;
#endif
class CReserveKeyOrScript;
class QWebEngineNewViewRequest;
class QNetworkReply;

namespace Ui {
    class ReceiveCoinsDialog;
}

class ReceiveCoinsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReceiveCoinsDialog(const QStyle *platformStyle, QWidget *parent = 0);
    ~ReceiveCoinsDialog();

    void setModel(WalletModel *model);
    void updateQRCode(const QString& sAddress);
    void setActiveAccount(CAccount* account);
    void setShowCopyQRAsImageButton(bool showCopyQRAsImagebutton_);
    static bool showCopyQRAsImagebutton;

public Q_SLOTS:
    void updateAddress(const QString& address);
    void showBuyGuldenDialog();
    void gotoRequestPaymentPage();
    void cancelRequestPayment();
    void gotoReceievePage();
    void activeAccountChanged(CAccount* activeAccount);

protected:

private:
    Ui::ReceiveCoinsDialog* ui;
    WalletModel* model;
    const QStyle* platformStyle;
    QString accountAddress;
    #if defined(HAVE_WEBENGINE_VIEW)
    QWebEngineView* buyView;
    #elif defined(HAVE_WEBKIT)
    QWebView* buyView;
    #endif
    CReserveKeyOrScript* buyReceiveAddress;
    CAccount* currentAccount;

private Q_SLOTS:
  void copyAddressToClipboard();
  void saveQRAsImage();
  void loadBuyViewFinished(bool bOk);
  void generateRequest();
  void buyGulden();
  void sslErrorHandler(QNetworkReply* qnr, const QList<QSslError> & errlist);
};

#endif // GULDEN_QT_GULDENRECEIVECOINSDIALOG_H
