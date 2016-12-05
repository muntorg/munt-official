// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "accounttablemodel.h"
#include "walletmodel.h"
#include "bitcoinunits.h"

#include <wallet/wallet.h>

const QString AccountTableModel::Normal = "Normal";
const QString AccountTableModel::Active = "A";
const QString AccountTableModel::Inactive = "I";

AccountTableModel::AccountTableModel(CWallet* wallet, WalletModel* parent)
    : QAbstractTableModel(parent)
    , m_wallet(wallet)
{
    activeAccount = parent->getActiveAccount();
    connect(parent, SIGNAL(activeAccountChanged(CAccount*)), this, SLOT(activeAccountChanged(CAccount*)));
    connect(parent, SIGNAL(accountAdded(CAccount*)), this, SLOT(accountAdded(CAccount*)));
}

int AccountTableModel::rowCount(const QModelIndex& parent) const
{
    LOCK(m_wallet->cs_wallet);

    if (!parent.isValid()) {
        if (m_wallet)
            return m_wallet->mapAccountLabels.size();
    }
    return 0;
}

int AccountTableModel::columnCount(const QModelIndex& parent) const
{
    return 2;
}

QVariant AccountTableModel::data(const QModelIndex& index, int role) const
{
    if (fDebug)
        LogPrintf("AccountTableModel::data\n");

    if (!m_wallet)
        return QVariant();

    {
        LOCK(m_wallet->cs_wallet);

        if (index.row() < 0 || (unsigned int)index.row() >= m_wallet->mapAccountLabels.size()) {
            return QVariant();
        }
    }

    CAccount* account = NULL;
    std::string accountLabel;
    std::string accountUUID;
    {
        LOCK(m_wallet->cs_wallet);
        auto iter = m_wallet->mapAccountLabels.begin();
        std::advance(iter, index.row());
        if (m_wallet->mapAccounts.count(iter->first) == 0)
            return QVariant();

        account = m_wallet->mapAccounts[iter->first];
        accountLabel = iter->second;
        accountUUID = iter->first;
    }

    if (role == Qt::DisplayRole) {
        if (index.column() == 0) {
            return QString::fromStdString(accountLabel.c_str());
        }
        if (index.column() == 1) {
            CAmount balance = pwalletMain->GetAccountBalance(accountUUID, 0, ISMINE_SPENDABLE, true);
            return BitcoinUnits::format(BitcoinUnits::Unit::BTC, balance, false, BitcoinUnits::separatorAlways, 2);
        }
    } else if (role == TypeRole) {
        if (account->m_Type == AccountType::Normal) {
            return Normal;
        }

    } else if (role == ActiveAccountRole) {
        if (account == activeAccount) {
            return Active;
        } else {
            return Inactive;
        }
    } else if (role == SelectedAccountRole) {
        return QString::fromStdString(accountUUID);
    } else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case 0:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    }
    return QVariant();
}

void AccountTableModel::activeAccountChanged(CAccount* account)
{
    LogPrintf("AccountTableModel::activeAccountChanged\n");

    activeAccount = account;

    beginResetModel();
    endResetModel();
}

void AccountTableModel::accountAdded(CAccount* account)
{
    beginResetModel();
    endResetModel();
}
