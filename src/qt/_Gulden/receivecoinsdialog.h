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

//Uncomment the below to build without either webkit or webengine - see https://github.com/Gulden/gulden-official/issues/96
//#undef HAVE_WEBKIT
//#undef HAVE_WEBENGINE_VIEW

//fixme: (Post-2.1) - mingw doesn't work with web engine view - for now we just use webkit everywhere but in future we should use web engine view where available and only webkit where absolutely needed.
#ifndef WIN32
//#define HAVE_WEBENGINE_VIEW
#endif

class OptionsModel;
class QStyle;
class WalletModel;
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

class ReceiveCoinsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReceiveCoinsDialog(const QStyle *platformStyle, QWidget *parent = 0);
    ~ReceiveCoinsDialog();

    void setModel(WalletModel *model);
    void updateQRCode(const QString& uri);
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
    CReserveKey* buyReceiveAddress;
    CAccount* currentAccount;

private Q_SLOTS:
  void copyAddressToClipboard();
  void saveQRAsImage();
  void loadBuyViewFinished(bool bOk);
  void generateRequest();
  void buyGulden();
  #ifdef HAVE_WEBKIT
  void sslErrorHandler(QNetworkReply* qnr, const QList<QSslError> & errlist);
  #endif
};

#endif // GULDEN_QT_GULDENRECEIVECOINSDIALOG_H
