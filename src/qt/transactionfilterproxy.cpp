// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "transactionfilterproxy.h"

#include "transactiontablemodel.h"
#include "transactionrecord.h"
#include "account.h"
#include "wallet/wallet.h"

#include <cstdlib>

#include <QDateTime>

// Earliest date that can be represented (far in the past)
const QDateTime TransactionFilterProxy::MIN_DATE = QDateTime::fromTime_t(0);

const QDateTime TransactionFilterProxy::MAX_DATE = QDateTime::fromTime_t(0xFFFFFFFF);

TransactionFilterProxy::TransactionFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
    , dateFrom(MIN_DATE)
    , dateTo(MAX_DATE)
    , addrPrefix()
    , typeFilter(ALL_TYPES)
    , watchOnlyFilter(WatchOnlyFilter_All)
    , minAmount(0)
    , limitRows(-1)
    , showInactive(true)
    , account(NULL)
{
    setSortRole(TransactionTableModel::RoleIndex::SortRole);
}

bool TransactionFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    int type = index.data(TransactionTableModel::TypeRole).toInt();
    QDateTime datetime = index.data(TransactionTableModel::DateRole).toDateTime();
    bool involvesWatchAddress = index.data(TransactionTableModel::WatchonlyRole).toBool();
    QString address = index.data(TransactionTableModel::AddressRole).toString();
    QString accountUUID = index.data(TransactionTableModel::AccountRole).toString();
    QString accountParentUUID = index.data(TransactionTableModel::AccountParentRole).toString();
    QString label = index.data(TransactionTableModel::LabelRole).toString();
    qint64 amount = llabs(index.data(TransactionTableModel::AmountRole).toLongLong());
    int status = index.data(TransactionTableModel::StatusRole).toInt();

    if (account && account->getUUID() != accountUUID.toStdString() && (fShowChildAccountsSeperately || account->getUUID() != accountParentUUID.toStdString()))
        return false;

    if (!showInactive && status == TransactionStatus::Conflicted)
        return false;
    if (!(TYPE(type) & typeFilter))
        return false;
    if (involvesWatchAddress && watchOnlyFilter == WatchOnlyFilter_No)
        return false;
    if (!involvesWatchAddress && watchOnlyFilter == WatchOnlyFilter_Yes)
        return false;
    if (datetime < dateFrom || datetime > dateTo)
        return false;
    if (!address.contains(addrPrefix, Qt::CaseInsensitive) && !label.contains(addrPrefix, Qt::CaseInsensitive))
        return false;
    if (amount < minAmount)
        return false;

    return true;
}

void TransactionFilterProxy::setDateRange(const QDateTime& from, const QDateTime& to)
{
    this->dateFrom = from;
    this->dateTo = to;
    invalidateFilter();
}

void TransactionFilterProxy::setAddressPrefix(const QString& addrPrefix)
{
    this->addrPrefix = addrPrefix;
    invalidateFilter();
}

void TransactionFilterProxy::setTypeFilter(quint32 modes)
{
    this->typeFilter = modes;
    invalidateFilter();
}

void TransactionFilterProxy::setMinAmount(const CAmount& minimum)
{
    this->minAmount = minimum;
    invalidateFilter();
}

void TransactionFilterProxy::setWatchOnlyFilter(WatchOnlyFilter filter)
{
    this->watchOnlyFilter = filter;
    invalidateFilter();
}

void TransactionFilterProxy::setAccountFilter(CAccount* account)
{
    this->account = account;
    invalidateFilter();
}

void TransactionFilterProxy::setLimit(int limit)
{
    this->limitRows = limit;
}

void TransactionFilterProxy::setShowInactive(bool showInactive)
{
    this->showInactive = showInactive;
    invalidateFilter();
}

int TransactionFilterProxy::rowCount(const QModelIndex& parent) const
{
    if (limitRows != -1) {
        return std::min(QSortFilterProxyModel::rowCount(parent), limitRows);
    } else {
        return QSortFilterProxyModel::rowCount(parent);
    }
}
