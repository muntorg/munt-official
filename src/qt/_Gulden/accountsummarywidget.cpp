// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "accountsummarywidget.h"
#include "_Gulden/forms/ui_accountsummarywidget.h"

#include "guiutil.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include <QApplication>

#include "ticker.h"
#include "validation.h"
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

    ui->accountSettings->setText(GUIUtil::fontAwesomeRegular("\uf013"));

    //Zero out all margins so that we can handle whitespace in stylesheet instead.
    ui->accountBalance->setContentsMargins( 0, 0, 0, 0 );
    ui->accountBalanceForex->setContentsMargins( 0, 0, 0, 0 );
    ui->accountName->setContentsMargins( 0, 0, 0, 0 );
    ui->accountSettings->setContentsMargins( 0, 0, 0, 0 );


    //Hand cursor for clickable elements.
    ui->accountSettings->setCursor( Qt::PointingHandCursor );
    ui->accountBalanceForex->setCursor( Qt::PointingHandCursor );

    //We hide the forex value until we get a response from ticker
    ui->accountBalanceForex->setVisible( false );


    //Signals.
    connect( m_ticker, SIGNAL( exchangeRatesUpdated() ), this, SLOT( updateExchangeRates() ) );
    connect( ui->accountSettings, SIGNAL( clicked() ), this, SIGNAL( requestAccountSettings() ) );
    connect( ui->accountBalanceForex, SIGNAL( clicked() ), this, SIGNAL( requestExchangeRateDialog() ) );
}

void AccountSummaryWidget::disconnectSlots()
{
    disconnect( ui->accountBalanceForex, SIGNAL( clicked() ), this, SIGNAL( requestExchangeRateDialog() ) );
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
    optionsModel = model;
    connect( optionsModel->guldenSettings, SIGNAL(  localCurrencyChanged(QString) ), this, SLOT( updateExchangeRates() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
}

void AccountSummaryWidget::hideBalances()
{
    ui->accountBalance->setVisible(false);
    ui->accountBalanceForex->setVisible(false);
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
        ui->accountBalanceForex->setVisible( false );
    updateExchangeRates();
}


void AccountSummaryWidget::balanceChanged()
{
    if (pactiveWallet && m_account)
    {
        m_accountBalance = pactiveWallet->GetBalance(m_account, true, true);
        CAmount immatureBalance = pactiveWallet->GetImmatureBalance(m_account);
        m_accountBalance += immatureBalance;
        m_accountBalanceImmatureOrUnconfirmed = pactiveWallet->GetUnconfirmedBalance(m_account, true) + immatureBalance;
        if (m_account->IsPoW2Witness())
            m_accountBalanceLocked = m_accountBalance - pactiveWallet->GetBalance(m_account, false, true); 
        else
            m_accountBalanceLocked = 0;
        updateExchangeRates();
    }
}


void AccountSummaryWidget::updateExchangeRates()
{
    if (optionsModel)
    {
        std::string currencyCode = optionsModel->guldenSettings->getLocalCurrency().toStdString();
        CAmount forexAmount = m_ticker->convertGuldenToForex(m_accountBalance, currencyCode);

        ui->accountBalance->setText( GuldenUnits::format(GuldenUnits::NLG, m_accountBalance, false, GuldenUnits::separatorAlways, 2) );

        if (m_account)
        {
            QString toolTip = QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Total funds: ")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, m_accountBalance, false, GuldenUnits::separatorStandard, 2));

            if (m_account->IsPoW2Witness())
            {
                toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Locked funds: ")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, m_accountBalanceLocked, false, GuldenUnits::separatorStandard, 2));
            }
            toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Funds awaiting confirmation: ")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, m_accountBalanceImmatureOrUnconfirmed, false, GuldenUnits::separatorStandard, 2));
            toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Spendable funds: ")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, m_accountBalance - m_accountBalanceLocked - m_accountBalanceImmatureOrUnconfirmed, false, GuldenUnits::separatorStandard, 2));

            ui->accountBalance->setToolTip(toolTip);
        }

        if (forexAmount > 0)
        {
            ui->accountBalanceForex->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode(currencyCode) + "\u2009") + GuldenUnits::format(GuldenUnits::NLG, forexAmount, false, GuldenUnits::separatorAlways, 2) + QString(")") );
            if (m_showForexBalance && ui->accountBalance->isVisible())
            {
                ui->accountBalanceForex->setVisible( true );
            }
        }
        else
        {
            ui->accountBalanceForex->setVisible( false );
        }
    }
}
