// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying file COPYING

#ifndef GULDEN_QT_MINING_ACCOUNTDIALOG_H
#define GULDEN_QT_MINING_ACCOUNTDIALOG_H

#include "guiutil.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <QDialog>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>


class OptionsModel;
class QStyle;
class WalletModel;
class ClientModel;
class CReserveKeyOrScript;

class CAccount;

namespace Ui {
    class MiningAccountDialog;
}

class MiningAccountDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MiningAccountDialog(const QStyle *platformStyle, QWidget *parent = 0);
    ~MiningAccountDialog();

    void setModel(WalletModel *model);
    void setClientModel(ClientModel* clientModel_);
    void setActiveAccount(CAccount* account);

    void updateSliderLabels();
    void startMining();
    static void startMining(CAccount* forAccount, uint64_t numThreads, uint64_t mineMemoryKb, std::string overrideAddress);
    void restartMiningIfNeeded();
public Q_SLOTS:
    void updateAddress(const QString& address);
    void activeAccountChanged(CAccount* activeAccount);

protected:

private:
    Ui::MiningAccountDialog* ui;
    WalletModel* model;
    ClientModel* clientModel;
    const QStyle* platformStyle;
    QString accountAddress;
    QString overrideAddress;
    CReserveKeyOrScript* buyReceiveAddress;
    CAccount* currentAccount;

private Q_SLOTS:
  void slotCopyAddressToClipboard();
  void slotEditMiningAddress();
  void slotResetMiningAddress();
  void slotStopMining();
  void slotStartMining();
  void slotKeepOpenWhenMining();
  void slotMineAtStartup();
  void slotMiningMemorySettingChanged();
  void slotMiningThreadSettingChanged();
  void slotMiningMemorySettingChanging(int val);
  void slotMiningThreadSettingChanging(int val);
  void slotUpdateMiningStats();
};

#endif
