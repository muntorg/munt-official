// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_ACCOUNT_SUMMARY_WIDGET_H
#define GULDEN_QT_ACCOUNT_SUMMARY_WIDGET_H

#include "walletmodel.h"
#include "amount.h"
#include <QStackedWidget>
#include <string>

class WalletModel;
class QStyle;
class QSortFilterProxyModel;
class CurrencyTicker;
class CAccount;

namespace Ui {
    class AccountSummaryWidget;
}

class AccountSummaryWidget : public QFrame
{
    Q_OBJECT

public:
    explicit AccountSummaryWidget(CurrencyTicker* ticker, QWidget *parent = 0);
    ~AccountSummaryWidget();

    void disconnectSlots();

    void setActiveAccount(const CAccount* account);
    void setOptionsModel(OptionsModel* model);
    void hideBalances();
    void showBalances();

    void showForexBalance(bool showForexBalance_);
public Q_SLOTS:


Q_SIGNALS:

    void requestExchangeRateDialog();
    void requestAccountSettings();

private Q_SLOTS:

    void balanceChanged();
    void updateExchangeRates();

private:
    OptionsModel* optionsModel;
    CurrencyTicker* m_ticker;
    Ui::AccountSummaryWidget* ui;
    const CAccount* m_account;
    std::string m_accountName;
    CAmount m_accountBalance = 0;
    CAmount m_accountBalanceLocked = 0;
    CAmount m_accountBalanceImmatureOrUnconfirmed = 0;
    bool m_showForexBalance = true;
};

#endif // GULDEN_QT_ACCOUNT_SUMMARY_WIDGET_H
