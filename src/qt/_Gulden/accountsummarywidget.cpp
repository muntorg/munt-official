#include "accountsummarywidget.h"
#include "_Gulden/forms/ui_accountsummarywidget.h"

#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "walletmodel.h"

#include <QApplication>

#include "ticker.h"
#include "main.h"
#include <bitcoinunits.h>
#include "wallet/wallet.h"
#include "utilmoneystr.h"
#include "GuldenGUI.h"

class CAccount;

AccountSummaryWidget::AccountSummaryWidget( CurrencyTicker* ticker, QWidget* parent ) :
    QFrame( parent ),
    optionsModel ( NULL ),
    m_ticker( ticker ),
    ui( new Ui::AccountSummaryWidget ),
    m_account ( NULL ),
    m_accountBalance ( 0 )
{
    ui->setupUi( this );


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

AccountSummaryWidget::~AccountSummaryWidget()
{
    delete ui;
}

void AccountSummaryWidget::setActiveAccount(const CAccount* account)
{
    m_account = account;

    ui->accountName->setText( limitString(QString::fromStdString(m_account->getLabel()), 35) );
    
    //fixme: GULDEN - Use AccountTableModel.
    m_accountBalance = pwalletMain->GetAccountBalance(m_account->getUUID(), 0, ISMINE_SPENDABLE, true );
    
    updateExchangeRates();
}

void AccountSummaryWidget::setOptionsModel( OptionsModel* model )
{
    optionsModel = model;
    connect( optionsModel->guldenSettings, SIGNAL(  localCurrencyChanged(QString) ), this, SLOT( updateExchangeRates() ) );
}


void AccountSummaryWidget::balanceChanged()
{
    if (pwalletMain && m_account)
    {
        m_accountBalance = pwalletMain->GetAccountBalance(m_account->getUUID(), 0, ISMINE_SPENDABLE, true );
        updateExchangeRates();
    }
}


void AccountSummaryWidget::updateExchangeRates()
{
    if (optionsModel)
    {
        std::string currencyCode = optionsModel->guldenSettings->getLocalCurrency().toStdString();
        CAmount forexAmount = m_ticker->convertGuldenToForex(m_accountBalance, currencyCode);

        ui->accountBalance->setText( BitcoinUnits::format(BitcoinUnits::Unit::BTC, m_accountBalance, false, BitcoinUnits::separatorAlways, 2) );

        //fixme: (GULDEN) (FUT) (1.6.1) - Show an immature/unconfirmed tooltip.
        /*
        labelBalance->setToolTip("");
        if (immatureBalance>0 || unconfirmedBalance>0)
        {
            QString toolTip;
            if (unconfirmedBalance > 0)
            {
                toolTip += tr("Pending confirmation: %1").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, unconfirmedBalance, false, BitcoinUnits::separatorStandard, 2));
            }
            if (immatureBalance > 0)
            {
                if (!toolTip.isEmpty())
                    toolTip += "\n";
                toolTip += tr("Pending maturity: %1").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, immatureBalance, false, BitcoinUnits::separatorStandard, 2));
            }
            labelBalance->setToolTip(toolTip);
        }
        */
        if (forexAmount > 0)
        {
            ui->accountBalanceForex->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode(currencyCode) + "\u2009") + BitcoinUnits::format(BitcoinUnits::Unit::BTC, forexAmount, false, BitcoinUnits::separatorAlways, 2) + QString(")") );
            ui->accountBalanceForex->setVisible( true );
        }
        else
        {
            ui->accountBalanceForex->setVisible( false );
        }
    }
}
