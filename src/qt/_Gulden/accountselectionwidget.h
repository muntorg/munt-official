// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef ACCOUNTSELECTIONWIDGET_H
#define ACCOUNTSELECTIONWIDGET_H

#include <QWidget>
#include <QSortFilterProxyModel>
#include "amount.h"

namespace Ui {
class AccountSelectionWidget;
}

class WalletModel;
class CAccount;

class WitnessSortFilterProxyModel : public QSortFilterProxyModel
{
Q_OBJECT
public:
    explicit WitnessSortFilterProxyModel(QObject *parent = 0);
    virtual ~WitnessSortFilterProxyModel();
    void setAmount(uint64_t nAmount_) {nAmount = nAmount_;}
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const;
private:
    uint64_t nAmount;
};

class AccountSelectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AccountSelectionWidget(QWidget *parent = nullptr);
    ~AccountSelectionWidget();

    void setWalletModel(WalletModel *model, CAmount minimumAmount);
    CAccount* selectedAccount();

private:
    Ui::AccountSelectionWidget *ui;
};

#endif // ACCOUNTSELECTIONWIDGET_H
