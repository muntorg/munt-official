// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef BITCOIN_QT_ACCOUNT_SUMMARY_WIDGET_H
#define BITCOIN_QT_ACCOUNT_SUMMARY_WIDGET_H

#include "walletmodel.h"
#include "amount.h"
#include <QStackedWidget>
#include <string>

class WalletModel;
class PlatformStyle;
class QSortFilterProxyModel;
class CurrencyTicker;
class CAccount;

namespace Ui {
class AccountSummaryWidget;
}

class AccountSummaryWidget : public QFrame {
    Q_OBJECT

public:
    explicit AccountSummaryWidget(CurrencyTicker* ticker, QWidget* parent = 0);
    ~AccountSummaryWidget();

    void setActiveAccount(const CAccount* account);
    void setOptionsModel(OptionsModel* model);

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
    CAmount m_accountBalance;
};

#endif // BITCOIN_QT_ACCOUNT_SUMMARY_WIDGET_H
