// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_GULDENNEWACCOUNTDIALOG_H
#define GULDEN_QT_GULDENNEWACCOUNTDIALOG_H

#include "guiutil.h"

#include <QFrame>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QVariant>

class QStyle;
class WalletModel;
class CAccountHD;
class WalletModel;

namespace Ui {
    class NewAccountDialog;
}

enum NewAccountType
{
    Transactional,
    FixedDeposit,
    WitnessOnly,
    ImportKey
};

class NewAccountDialog : public QFrame
{
    Q_OBJECT

public:
    explicit NewAccountDialog(const QStyle *platformStyle, QWidget *parent, WalletModel* model);
    ~NewAccountDialog();

    QString getAccountName();
    NewAccountType getAccountType() { return m_Type; }

    virtual void resizeEvent(QResizeEvent* event);

Q_SIGNALS:
      void cancel();
      void accountAdded();
      void addAccountMobile();

public Q_SLOTS:

protected:
    void updateTextForSize(const QSize& size);

private:
    Ui::NewAccountDialog *ui;
    const QStyle *platformStyle;
    CAccountHD* newAccount;
    WalletModel* walletModel;
    NewAccountType m_Type = Transactional;

    QString toolTipTransactionalAccount;
    QString toolTipMobileAccount;
    QString toolTipWitnessAccount;
    QString toolTipImportWitnessAccount;
    QString toolTipImportPrivKey;
    int verticalCachedDisplayType=0;
    int horizontalCachedDisplayType=0;

private Q_SLOTS:
    void connectToMobile();
    void cancelMobile();
    void valueChanged();
    void addAccount();
    void addWitnessAccount();
    void importWitnessOnly();
    void importPrivateKey();
    void showSyncQr();
};

#endif // GULDEN_QT_GULDENNEWACCOUNTDIALOG_H
