// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_ACCOUNTSETTINGSDIALOG_H
#define GULDEN_QT_ACCOUNTSETTINGSDIALOG_H

#include "guiutil.h"

#include <QStackedWidget>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QVariant>

class OptionsModel;
class QStyle;
class WalletModel;
class CAccount;

namespace Ui {
    class AccountSettingsDialog;
}

class AccountSettingsDialog : public QStackedWidget
{
    Q_OBJECT

public:
    explicit AccountSettingsDialog(const QStyle *platformStyle, QWidget *parent, CAccount* activeAccount, WalletModel* model);
    ~AccountSettingsDialog();

Q_SIGNALS:
      void cancel();
      void dismissAccountSettings();

public Q_SLOTS:
    void activeAccountChanged(CAccount* account);
    void applyChanges();
    void deleteAccount();
    void showSyncQr();
    void copyQr();
    void rotateClicked();

protected:

private:
    WalletModel* walletModel;
    Ui::AccountSettingsDialog* ui;
    const QStyle* platformStyle;
    CAccount* activeAccount;

    void pushDialog(QWidget *dialog);

private Q_SLOTS:
    void popDialog(QWidget* dialog);
};

#endif // GULDEN_QT_GULDENNEWACCOUNTDIALOG_H
