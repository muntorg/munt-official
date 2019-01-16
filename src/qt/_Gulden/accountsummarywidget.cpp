// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include "accountsummarywidget.h"
#include "_Gulden/forms/ui_accountsummarywidget.h"

#include "guiutil.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include <QApplication>

#include "ticker.h"
#include "validation/validation.h"
#include <units.h>
#include "wallet/wallet.h"
#include "utilmoneystr.h"
#include "GuldenGUI.h"

class CAccount;

AccountSummaryWidget::AccountSummaryWidget( CurrencyTicker* ticker, QWidget* parent )
: QFrame( parent )
, optionsModel ( NULL )
, m_ticker( ticker )
, ui( new Ui::AccountSummaryWidget )
, m_account ( NULL )
{
    ui->setupUi( this );

    ui->accountSettings->setTextFormat( Qt::RichText );
    ui->accountSettings->setText(GUIUtil::fontAwesomeRegular("\uf013"));

    //Zero out all margins so that we can handle whitespace in stylesheet instead.
    ui->accountBalance->setContentsMargins(0, 0, 0, 0);
    ui->accountBalanceForex->setContentsMargins(0, 0, 0, 0);
    ui->accountBalanceForexIndicator->setContentsMargins(0, 0, 0, 0);
    ui->accountName->setContentsMargins(0, 0, 0, 0);
    ui->accountSettings->setContentsMargins(0, 0, 0, 0);

    ui->accountBalanceForexIndicator->setTextFormat( Qt::RichText );
    ui->accountBalanceForexIndicator->setText( GUIUtil::fontAwesomeRegular("\uf05a") );

    //Hand cursor for clickable elements.
    ui->accountSettings->setCursor(Qt::PointingHandCursor);
    ui->accountBalanceForex->setCursor(Qt::PointingHandCursor);
    ui->accountBalanceForexIndicator->setCursor(Qt::PointingHandCursor);

    //We hide the forex value until we get a response from ticker
    ui->accountBalanceForex->setVisible(false);
    ui->accountBalanceForexIndicator->setVisible(false);


    //Signals.
    connect( m_ticker, SIGNAL( exchangeRatesUpdated() ), this, SLOT( updateExchangeRates() ) );
    connect( ui->accountSettings, SIGNAL( clicked() ), this, SIGNAL( requestAccountSettings() ) );
    connect( ui->accountBalanceForex, SIGNAL( clicked() ), this, SIGNAL( requestExchangeRateDialog() ) );
    connect( ui->accountBalanceForexIndicator, SIGNAL( clicked() ), this, SIGNAL( requestExchangeRateDialog() ) );
}

void AccountSummaryWidget::disconnectSlots()
{
    disconnect( ui->accountBalanceForex, SIGNAL( clicked() ), this, SIGNAL( requestExchangeRateDialog() ) );
    disconnect( ui->accountBalanceForexIndicator, SIGNAL( clicked() ), this, SIGNAL( requestExchangeRateDialog() ) );
    disconnect( ui->accountSettings, SIGNAL( clicked() ), this, SIGNAL( requestAccountSettings() ) );
    disconnect( m_ticker, SIGNAL( exchangeRatesUpdated() ), this, SLOT( updateExchangeRates() ) );
    if (optionsModel && optionsModel->guldenSettings)
        disconnect( optionsModel->guldenSettings, SIGNAL(  localCurrencyChanged(QString) ), this, SLOT( updateExchangeRates() ) );
}

AccountSummaryWidget::~AccountSummaryWidget()
{
    delete ui;
}

void AccountSummaryWidget::setActiveAccount(const CAccount* account)
{
    m_account = account;

    ui->accountName->setText( limitString(QString::fromStdString(m_account->getLabel()), 35) );

    balanceChanged();

    updateExchangeRates();
}

void AccountSummaryWidget::setOptionsModel( OptionsModel* model )
{
    if (model)
    {
        optionsModel = model;
        connect( optionsModel->guldenSettings, SIGNAL(  localCurrencyChanged(QString) ), this, SLOT( updateExchangeRates() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    }
    else
    {
        optionsModel = nullptr;
    }
}

void AccountSummaryWidget::hideBalances()
{
    ui->accountBalance->setVisible(false);
    ui->accountBalanceForex->setVisible(false);
    ui->accountBalanceForexIndicator->setVisible(false);
}

void AccountSummaryWidget::showBalances()
{
    ui->accountBalance->setVisible(true);
    updateExchangeRates();
}

void AccountSummaryWidget::showForexBalance(bool showForexBalance_)
{
    m_showForexBalance = showForexBalance_;
    if (!m_showForexBalance)
    {
        ui->accountBalanceForex->setVisible(false);
        ui->accountBalanceForexIndicator->setVisible(true);
    }
    else
    {
        ui->accountBalanceForex->setVisible(true);
        ui->accountBalanceForexIndicator->setVisible(false);
    }
    updateExchangeRates();
}


void AccountSummaryWidget::balanceChanged()
{
    //fixme: (2.1) Double check how we want to display this.
    if (pactiveWallet && m_account)
    {
        //fixme: (2.1) rather cache this somewhere central where it can be shared with e.g. the witness dialog?
        WalletBalances balances;
        pactiveWallet->GetBalances(balances, m_account, true);

        m_accountBalanceAvailable = balances.availableExcludingLocked;
        m_accountBalanceLocked = balances.totalLocked;
        m_accountBalanceImmatureOrUnconfirmed = balances.immatureExcludingLocked + balances.unconfirmedExcludingLocked;
        m_accountBalanceTotal = m_accountBalanceLocked + m_accountBalanceAvailable + m_accountBalanceImmatureOrUnconfirmed;
        updateExchangeRates();
    }
}


void AccountSummaryWidget::updateExchangeRates()
{
    if (optionsModel)
    {
        std::string currencyCode = optionsModel->guldenSettings->getLocalCurrency().toStdString();
        CAmount forexBalanceTotal = m_ticker->convertGuldenToForex(m_accountBalanceTotal, currencyCode);
        CAmount forexBalanceLocked = m_ticker->convertGuldenToForex(m_accountBalanceLocked, currencyCode);
        CAmount forexBalanceImmatureOrUnconfirmed = m_ticker->convertGuldenToForex(m_accountBalanceImmatureOrUnconfirmed, currencyCode);
        CAmount forexBalanceAvailable = m_ticker->convertGuldenToForex(m_accountBalanceAvailable, currencyCode);

        ui->accountBalance->setText( GuldenUnits::format(GuldenUnits::NLG, m_accountBalanceTotal, false, GuldenUnits::separatorAlways, 2) );

        if (m_account)
        {
            QString toolTip;
            toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\"><b>%1</b>&nbsp;&nbsp;&nbsp;&nbsp;</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Account balances")).arg("");
            toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\"></td><td style=\"white-space: nowrap;\" align=\"right\"></td></tr>");
            toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1&nbsp;&nbsp;&nbsp;&nbsp;</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Total")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, m_accountBalanceTotal, false, GuldenUnits::separatorStandard, 2));
            if (m_account->IsPoW2Witness())
            {
                toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1&nbsp;&nbsp;&nbsp;&nbsp;</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Locked")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, m_accountBalanceLocked, false, GuldenUnits::separatorStandard, 2));
            }
            toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1&nbsp;&nbsp;&nbsp;&nbsp;</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Pending")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, m_accountBalanceImmatureOrUnconfirmed, false, GuldenUnits::separatorStandard, 2));
            toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1&nbsp;&nbsp;&nbsp;&nbsp;</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Spendable")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, m_accountBalanceAvailable, false, GuldenUnits::separatorStandard, 2));


            ui->accountBalance->setToolTip(toolTip);
        }

        if (forexBalanceTotal > 0)
        {
            QString forexToolTip;
            forexToolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\"><b>%1</b>&nbsp;&nbsp;&nbsp;&nbsp;</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Conversion estimate")).arg("");
            forexToolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\"></td><td style=\"white-space: nowrap;\" align=\"right\"></td></tr>");
            forexToolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1&nbsp;&nbsp;&nbsp;&nbsp;</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Total")).arg(QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + GuldenUnits::format(GuldenUnits::NLG, m_ticker->convertGuldenToForex(forexBalanceTotal, optionsModel->guldenSettings->getLocalCurrency().toStdString()), false, GuldenUnits::separatorAlways, 2));
            if (m_account->IsPoW2Witness())
            {
                forexToolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1&nbsp;&nbsp;&nbsp;&nbsp;</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Locked")).arg(QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + GuldenUnits::format(GuldenUnits::NLG, m_ticker->convertGuldenToForex(forexBalanceLocked, optionsModel->guldenSettings->getLocalCurrency().toStdString()), false, GuldenUnits::separatorAlways, 2));
            }
            forexToolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1&nbsp;&nbsp;&nbsp;&nbsp;</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Pending")).arg(QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + GuldenUnits::format(GuldenUnits::NLG, m_ticker->convertGuldenToForex(forexBalanceImmatureOrUnconfirmed, optionsModel->guldenSettings->getLocalCurrency().toStdString()), false, GuldenUnits::separatorAlways, 2));
            forexToolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1&nbsp;&nbsp;&nbsp;&nbsp;</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Spendable")).arg(QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + GuldenUnits::format(GuldenUnits::NLG, m_ticker->convertGuldenToForex(forexBalanceAvailable, optionsModel->guldenSettings->getLocalCurrency().toStdString()), false, GuldenUnits::separatorAlways, 2));

            ui->accountBalanceForex->setToolTip(forexToolTip);
            ui->accountBalanceForexIndicator->setToolTip(forexToolTip+QString("<tr><td></td><td></td></tr><tr><td>%1</td></tr>").arg("Expand window to always show estimate"));
            ui->accountBalanceForex->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode(currencyCode) + "\u2009") + GuldenUnits::format(GuldenUnits::NLG, forexBalanceTotal, false, GuldenUnits::separatorAlways, 2) + QString(")") );
            if (ui->accountBalance->isVisible())
            {
                if (m_showForexBalance)
                {
                    ui->accountBalanceForex->setVisible(true);
                    ui->accountBalanceForexIndicator->setVisible(false);
                }
                else
                {
                    ui->accountBalanceForex->setVisible(false);
                    ui->accountBalanceForexIndicator->setVisible(true);
                }
            }
        }
        else
        {
            ui->accountBalanceForex->setVisible(false);
            ui->accountBalanceForexIndicator->setVisible(false);
        }
    }
}
