// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "accountselectionwidget.h"
#include <qt/forms/ui_accountselectionwidget.h>
#include "walletmodel.h"
#include "accounttablemodel.h"

WitnessSortFilterProxyModel::WitnessSortFilterProxyModel(QObject *parent)
: QSortFilterProxyModel(parent)
{
}

WitnessSortFilterProxyModel::~WitnessSortFilterProxyModel()
{
}

bool WitnessSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);

    // Must be a 'normal' type account.
    std::string sState = sourceModel()->data(index0, AccountTableModel::StateRole).toString().toStdString();
    if (sState != GetAccountStateString(AccountState::Normal))
        return false;

    // Must not be the current account.
    QString sActive = sourceModel()->data(index0, AccountTableModel::ActiveAccountRole).toString();
    if (sActive != AccountTableModel::Inactive)
        return false;

    // Must not be a witness account itself.
    std::string sSubType = sourceModel()->data(index0, AccountTableModel::TypeRole).toString().toStdString();
    if (sSubType == GetAccountTypeString(AccountType::PoW2Witness))
        return false;
    if (sSubType == GetAccountTypeString(AccountType::WitnessOnlyWitnessAccount))
        return false;
    // Or a mining account
    if (sSubType == GetAccountTypeString(AccountType::MiningAccount))
        return false;

    // Must have sufficient balance to fund the operation.
    uint64_t nCompare = sourceModel()->data(index0, AccountTableModel::AvailableBalanceRole).toLongLong();
    if (nCompare < nAmount)
        return false;
    return true;
}

AccountSelectionWidget::AccountSelectionWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AccountSelectionWidget)
{
    ui->setupUi(this);
}

AccountSelectionWidget::~AccountSelectionWidget()
{
    delete ui;
}

void AccountSelectionWidget::setWalletModel(WalletModel *walletModel, CAmount minimumAmount)
{

    WitnessSortFilterProxyModel* proxyFilterByBalanceFund = new WitnessSortFilterProxyModel(this);
    proxyFilterByBalanceFund->setSourceModel(walletModel->getAccountTableModel());
    proxyFilterByBalanceFund->setDynamicSortFilter(true);
    proxyFilterByBalanceFund->setAmount(minimumAmount);

    QSortFilterProxyModel* proxyFilterByBalanceFundSorted = new QSortFilterProxyModel(this);
    proxyFilterByBalanceFundSorted->setSourceModel(proxyFilterByBalanceFund);
    proxyFilterByBalanceFundSorted->setDynamicSortFilter(true);
    proxyFilterByBalanceFundSorted->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyFilterByBalanceFundSorted->setFilterFixedString("");
    proxyFilterByBalanceFundSorted->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyFilterByBalanceFundSorted->setFilterKeyColumn(AccountTableModel::ColumnIndex::Label);
    proxyFilterByBalanceFundSorted->setSortRole(Qt::DisplayRole);
    proxyFilterByBalanceFundSorted->sort(0);

    ui->comboBox->setModel(proxyFilterByBalanceFundSorted);

    // default selection
    if (ui->comboBox->model()->rowCount()>0) {
        ui->comboBox->setCurrentIndex(0);
    }
}

CAccount* AccountSelectionWidget::selectedAccount()
{
    CAccount* account = nullptr;

    int index = ui->comboBox->currentIndex();
    if (index >= 0)
    {
        boost::uuids::uuid accountUUID = getUUIDFromString(ui->comboBox->currentData(AccountTableModel::AccountTableRoles::SelectedAccountRole).toString().toStdString());
        {
            LOCK(pactiveWallet->cs_wallet);
            account = pactiveWallet->mapAccounts[accountUUID];
        }

    }

    return account;
}
