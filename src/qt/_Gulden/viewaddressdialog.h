// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_GULDENVIEWADDRESSDIALOG_H
#define GULDEN_QT_GULDENVIEWADDRESSDIALOG_H

#include "guiutil.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <QDialog>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QVariant>

class OptionsModel;
class QStyle;
class WalletModel;
class CReserveKeyOrScript;


class CAccount;

namespace Ui {
    class ViewAddressDialog;
}

class ViewAddressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ViewAddressDialog(const QStyle *platformStyle, QWidget *parent = 0);
    ~ViewAddressDialog();

    void setModel(WalletModel *model);
    void updateQRCode(const QString& sAddress);
    void setActiveAccount(CAccount* account);

public Q_SLOTS:
    void updateAddress(const QString& address);
    void activeAccountChanged(CAccount* activeAccount);

protected:

private:
    Ui::ViewAddressDialog* ui = nullptr;
    WalletModel* model = nullptr;
    const QStyle* platformStyle = nullptr;
    QString accountAddress;
    CAccount* currentAccount = nullptr;

private Q_SLOTS:
  void copyAddressToClipboard();
};

#endif
