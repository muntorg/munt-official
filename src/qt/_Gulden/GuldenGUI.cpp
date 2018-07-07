// Copyright (c) 2015-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include "GuldenGUI.h"
#include "gui.h"
#include "units.h"
#include "clickablelabel.h"
#include "receivecoinsdialog.h"
#include "validation/validation.h"
#include "guiutil.h"
#include "init.h"
#include "unity/appmanager.h"
#include "alert.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QProxyStyle>
#include <QLineEdit>
#include <QTextEdit>
#include <QCollator>

#include <_Gulden/accountsummarywidget.h>
#include <_Gulden/newaccountdialog.h>
#include <_Gulden/importprivkeydialog.h>
#include <_Gulden/importwitnessdialog.h>
#include <_Gulden/exchangeratedialog.h>
#include <_Gulden/accountsettingsdialog.h>
#include <_Gulden/witnessdialog.h>
#include <Gulden/util.h>
#include <consensus/consensus.h>
#include "sendcoinsdialog.h"
#include "wallet/wallet.h"
#include "walletframe.h"
#include "walletview.h"
#include "utilmoneystr.h"
#include "passwordmodifydialog.h"
#include "backupdialog.h"
#include "welcomedialog.h"
#include "ticker.h"
#include "nockssettings.h"
#include "units.h"
#include "optionsmodel.h"
#include "askpassphrasedialog.h"


//Font sizes - NB! We specifically use 'px' and not 'pt' for all font sizes, as qt scales 'pt' dynamically in a way that makes our fonts unacceptably small on OSX etc. it doesn't do this with px so we use px instead.
//QString CURRENCY_DECIMAL_FONT_SIZE = "11px"; // For .00 in currency and PND text.
//QString BODY_FONT_SIZE = "12px"; // Standard body font size used in 'most places'.
//QString CURRENCY_FONT_SIZE = "13px"; // For currency
//QString TOTAL_FONT_SIZE = "15px"; // For totals and account names
//QString HEADER_FONT_SIZE = "16px"; // For large headings

const char* LOGO_FONT_SIZE = "28px";
const char* TOOLBAR_FONT_SIZE = "15px";
const char* BUTTON_FONT_SIZE = "15px";
const char* SMALL_BUTTON_FONT_SIZE = "14px";
const char* MINOR_LABEL_FONT_SIZE = "12px";

//Colors
const char* ACCENT_COLOR_1 = "#007aff";
const char* ACCENT_COLOR_2 = "#0067d9";
const char* TEXT_COLOR_1 = "#999";
const char* COLOR_VALIDATION_FAILED = "#FF8080";


//Toolbar constants
unsigned int sideBarWidthNormal = 300;
unsigned int sideBarWidth = sideBarWidthNormal;
const unsigned int horizontalBarHeight = 60;
const char* SIDE_BAR_WIDTH = "300px";
//Extended width for large balances.
unsigned int sideBarWidthExtended = 340;
const char* SIDE_BAR_WIDTH_EXTENDED = "340px";

void setValid(QWidget* control, bool validity)
{
    control->setProperty("valid", validity);
    control->style()->unpolish(control);
    control->style()->polish(control);
}

void burnLineEditMemory(QLineEdit* edit)
{
    // Attempt to overwrite text so that they do not linger around in memory
    edit->setText(QString(" ").repeated(edit->text().size()));
    edit->clear();
}

void burnTextEditMemory(QTextEdit* edit)
{
    // Attempt to overwrite text so that they do not linger around in memory
    edit->setText(QString(" ").repeated(edit->toPlainText().length()));
    edit->clear();
}

static void setAccountLabelSelected(ClickableLabel* checkLabel)
{
    checkLabel->setChecked(true);
    checkLabel->setCursor ( Qt::ArrowCursor );
}

static void setAccountLabelUnselected(ClickableLabel* checkLabel)
{
    checkLabel->setChecked(false);
    checkLabel->setCursor ( Qt::PointingHandCursor );
}

bool requestUnlockDialogAlreadyShowing=false;
void GUI::NotifyRequestUnlock(void* wallet, QString reason)
{
    if (!requestUnlockDialogAlreadyShowing)
    {
        requestUnlockDialogAlreadyShowing = true;
        LogPrint(BCLog::QT, "NotifyRequestUnlock\n");
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this, reason);
        dlg.setModel(new WalletModel(NULL, (CWallet*)wallet, NULL, NULL));
        dlg.exec();
        requestUnlockDialogAlreadyShowing = false;
    }
}

void GUI::NotifyRequestUnlockWithCallback(void* wallet, QString reason, std::function<void (void)> successCallback)
{
    if (!requestUnlockDialogAlreadyShowing)
    {
        requestUnlockDialogAlreadyShowing = true;
        LogPrint(BCLog::QT, "NotifyRequestUnlockWithCallback\n");
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this, reason);
        dlg.setModel(new WalletModel(NULL, (CWallet*)wallet, NULL, NULL));
        int result = dlg.exec();
        if(result == QDialog::Accepted)
            successCallback();
        requestUnlockDialogAlreadyShowing = false;
    }
}

void GUI::handlePaymentAccepted()
{
    LogPrint(BCLog::QT, "GUI::handlePaymentAccepted\n");

    refreshTabVisibilities();
}

void GUI::setBalance(const WalletBalances& balances, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    LogPrint(BCLog::QT, "GUI::setBalance\n");
    if (ShutdownRequested())
        return;

    cachedBalances = balances;
    watchOnlyBalanceCached = watchOnlyBalance;
    watchUnconfBalanceCached = watchUnconfBalance;
    watchImmatureBalanceCached = watchImmatureBalance;

    if (!labelBalance || !labelBalanceForex)
        return;

    CAmount displayBalanceAvailable = balances.availableExcludingLocked;
    CAmount displayBalanceLocked = balances.totalLocked;
    CAmount displayBalanceImmatureOrUnconfirmed = balances.immatureExcludingLocked + balances.unconfirmedExcludingLocked;
    CAmount displayBalanceTotal = displayBalanceLocked + displayBalanceAvailable + displayBalanceImmatureOrUnconfirmed;

    labelBalance->setText(GuldenUnits::format(GuldenUnits::NLG, displayBalanceTotal, false, GuldenUnits::separatorStandard, 2));
    if (displayBalanceTotal > 0 && optionsModel)
    {
        labelBalanceForex->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + GuldenUnits::format(GuldenUnits::NLG, ticker->convertGuldenToForex(displayBalanceAvailable, optionsModel->guldenSettings->getLocalCurrency().toStdString()), false, GuldenUnits::separatorAlways, 2) + QString(")"));
        if (labelBalance->isVisible())
            labelBalanceForex->setVisible(true);
    }
    else
    {
        labelBalanceForex->setVisible(false);
    }

    if (accountScrollArea && displayBalanceTotal > 999999 * COIN && sideBarWidth != sideBarWidthExtended)
    {
        sideBarWidth = sideBarWidthExtended;
        doApplyStyleSheet();
        resizeToolBarsGulden();
    }
    else if (accountScrollArea && displayBalanceTotal < 999999 * COIN && sideBarWidth == sideBarWidthExtended)
    {
        sideBarWidth = sideBarWidthNormal;
        doApplyStyleSheet();
        resizeToolBarsGulden();
    }

    QString toolTip = QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Total funds: ")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, displayBalanceTotal, false, GuldenUnits::separatorStandard, 2));
    toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Locked funds: ")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, displayBalanceLocked, false, GuldenUnits::separatorStandard, 2));
    toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Funds awaiting confirmation: ")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, displayBalanceImmatureOrUnconfirmed, false, GuldenUnits::separatorStandard, 2));
    toolTip += QString("<tr><td style=\"white-space: nowrap;\" align=\"left\">%1</td><td style=\"white-space: nowrap;\" align=\"right\">%2</td></tr>").arg(tr("Spendable funds: ")).arg(GuldenUnits::formatWithUnit(GuldenUnits::NLG, displayBalanceAvailable, false, GuldenUnits::separatorStandard, 2));
    labelBalance->setToolTip(toolTip);
}

void GUI::updateExchangeRates()
{
    LogPrint(BCLog::QT, "GUI::updateExchangeRates\n");
    setBalance(cachedBalances, watchOnlyBalanceCached, watchUnconfBalanceCached, watchImmatureBalanceCached);
}

void GUI::doRequestRenewWitness(CAccount* funderAccount, CAccount* targetWitnessAccount)
{
    LogPrint(BCLog::QT, "GUI::doRequestRenewWitness\n");

    std::string strError;
    CMutableTransaction tx(CURRENT_TX_VERSION_POW2);
    CReserveKeyOrScript changeReserveKey(pactiveWallet, funderAccount, KEYCHAIN_EXTERNAL);
    CAmount txFee;
    if (!pactiveWallet->PrepareRenewWitnessAccountTransaction(funderAccount, targetWitnessAccount, changeReserveKey, tx, txFee, strError))
    {
        std::string strAlert = "Failed to create witness renew transaction: " + strError;
        CAlert::Notify(strAlert, true, true);
        LogPrintf("%s", strAlert.c_str());
        return;
    }

    QString questionString = tr("Renewing witness account will incur a transaction fee: ");
    questionString.append("<span style='color:#aa0000;'>");
    questionString.append(GuldenUnits::formatHtmlWithUnit(optionsModel->getDisplayUnit(), txFee));
    questionString.append("</span> ");
    QDialog* d = createDialog(this, questionString, tr("Send"), tr("Cancel"), 600, 360);

    int result = d->exec();
    if(result != QDialog::Accepted)
    {
        return;
    }

    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        if (!pactiveWallet->SignAndSubmitTransaction(changeReserveKey, tx, strError))
        {
            std::string strAlert = "Failed to sign witness renewal transaction:" + strError;
            CAlert::Notify(strAlert, true, true);
            LogPrintf("%s", strAlert.c_str());
            return;
        }
    }

    // Clear the failed flag in UI, and remove the 'renew' button for immediate user feedback.
    targetWitnessAccount->SetWarningState(AccountStatus::WitnessPending);
    static_cast<const CGuldenWallet*>(pactiveWallet)->NotifyAccountWarningChanged(pactiveWallet, targetWitnessAccount);
    walletFrame->currentWalletView()->witnessDialogPage->update();
}

void GUI::requestRenewWitness(CAccount* funderAccount)
{
    LogPrint(BCLog::QT, "GUI::requestRenewWitness\n");

    CAccount* targetWitnessAccount = pactiveWallet->getActiveAccount();

    std::function<void (void)> successCallback = [=](){doRequestRenewWitness(funderAccount, targetWitnessAccount);};
    if (pactiveWallet->IsLocked())
    {
        uiInterface.RequestUnlockWithCallback(pactiveWallet, _("Wallet unlock required to renew witness"), successCallback);
    }
    else
    {
        successCallback();
    }
}

void GUI::requestFundWitness(CAccount* funderAccount)
{
    LogPrint(BCLog::QT, "GUI::requestFundWitness\n");

    CAccount* targetWitnessAccount = pactiveWallet->getActiveAccount();
    pactiveWallet->setActiveAccount(funderAccount);
    gotoSendCoinsPage();
    walletFrame->currentWalletView()->sendCoinsPage->gotoWitnessTab(targetWitnessAccount);
}

void GUI::requestEmptyWitness()
{
    LogPrint(BCLog::QT, "GUI::requestEmptyWitness\n");

    CAccount* fromWitnessAccount = pactiveWallet->getActiveAccount();
    CAmount availableAmount = pactiveWallet->GetBalance(fromWitnessAccount, false, true);
    if (availableAmount > 0)
    {
        walletFrame->gotoSendCoinsPage();
        walletFrame->currentWalletView()->sendCoinsPage->setAmount(availableAmount);
    }
    else
    {
        QString message = tr("The funds in this account are currently locked for witnessing and cannot be transfered, please wait until lock expires or for earnings to accumulate before trying again.");
        QDialog* d = createDialog(this, message, tr("Okay"), QString(""), 400, 180);
        d->exec();
    }
}

void GUI::setOptionsModel(OptionsModel* optionsModel_)
{
    LogPrint(BCLog::QT, "GUI::setOptionsModel\n");

    if (optionsModel_)
    {
        optionsModel = optionsModel_;
        optionsModel->setTicker(ticker);
        optionsModel->setNocksSettings(nocksSettings);
        if (accountSummaryWidget)
            accountSummaryWidget->setOptionsModel(optionsModel);
        connect( optionsModel->guldenSettings, SIGNAL(  localCurrencyChanged(QString) ), this, SLOT( updateExchangeRates() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        updateExchangeRates();
    }
    else
    {
        optionsModel = nullptr;
        if (accountSummaryWidget)
            accountSummaryWidget->setOptionsModel(nullptr);
    }
}

void GUI::createToolBars()
{
    LogPrint(BCLog::QT, "GUI::createToolBars\n");

    if (!walletFrame)
        return;

    QToolBar* toolbar = addToolBar(tr("Tabs toolbar"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(witnessDialogAction);
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    overviewAction->setChecked(true);

    //Filler for right of menu bar.
    #ifndef MAC_OSX
    menuBarSpaceFiller = new QFrame( this );
    menuBarSpaceFiller->setObjectName( "menuBarSpaceFiller" );
    menuBarSpaceFiller->move(sideBarWidth, 0);
    menuBarSpaceFiller->setFixedSize(20000, 21);
    #endif

    //Add the 'Account bar' - vertical bar on the left
    accountBar = new QToolBar( QCoreApplication::translate( "toolbar", "Account toolbar" ) );
    accountBar->setObjectName( "account_bar" );
    accountBar->setMovable( false );
    accountBar->setFixedWidth( sideBarWidth );
    accountBar->setMinimumWidth( sideBarWidth );
    accountBar->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );

    //Horizontally lay out 'My accounts' text and 'wallet settings' button side by side.
    {
        QFrame* myAccountsFrame = new QFrame( this );
        myAccountsFrame->setObjectName( "frameMyAccounts" );
        QHBoxLayout* layoutMyAccounts = new QHBoxLayout;
        layoutMyAccounts->setObjectName("my_accounts_layout");
        myAccountsFrame->setLayout(layoutMyAccounts);
        myAccountsFrame->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        accountBar->addWidget( myAccountsFrame );
        myAccountsFrame->setContentsMargins( 0, 0, 0, 0 );
        layoutMyAccounts->setSpacing(0);
        layoutMyAccounts->setContentsMargins( 0, 0, 0, 0 );

        ClickableLabel* myAccountLabel = new ClickableLabel( myAccountsFrame );
        myAccountLabel->setObjectName( "labelMyAccounts" );
        myAccountLabel->setText( tr("My accounts") );
        layoutMyAccounts->addWidget( myAccountLabel );
        myAccountLabel->setContentsMargins( 0, 0, 0, 0 );

        //Spacer to fill width
        {
            QWidget* spacerMid = new QWidget( myAccountsFrame );
            spacerMid->setObjectName("my_accounts_mid_spacer");
            spacerMid->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
            layoutMyAccounts->addWidget( spacerMid );
        }

        ClickableLabel* labelWalletSettings = new ClickableLabel( myAccountsFrame );
        labelWalletSettings->setTextFormat( Qt::RichText );
        labelWalletSettings->setText( GUIUtil::fontAwesomeRegular("\uf013") );
        labelWalletSettings->setObjectName( "labelWalletSettings" );
        labelWalletSettings->setCursor ( Qt::PointingHandCursor );
        layoutMyAccounts->addWidget( labelWalletSettings );
        labelWalletSettings->setContentsMargins( 0, 0, 0, 0 );

        connect( labelWalletSettings, SIGNAL( clicked() ), this, SLOT( gotoPasswordDialog() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    }

    //Spacer to fill height
    {
        QScrollArea* scrollArea = new QScrollArea ( this );
        scrollArea->setObjectName("account_scroll_area");
        accountScrollArea = new QFrame( scrollArea );
        accountScrollArea->setObjectName("account_scroll_area_frame");
        scrollArea->setContentsMargins( 0, 0, 0, 0);
        scrollArea->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
        scrollArea->setWidget(accountScrollArea);
        scrollArea->setWidgetResizable(true);

        accountScrollArea->setContentsMargins( 0, 0, 0, 0);
        accountScrollArea->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Maximum );


        accountBar->addWidget( scrollArea );

        QVBoxLayout* vbox = new QVBoxLayout(accountScrollArea);
        vbox->setObjectName("account_scroll_area_frame");
        vbox->setSpacing(0);
        vbox->setContentsMargins( 0, 0, 0, 0 );

        accountScrollArea->setLayout( vbox );
    }

    ClickableLabel* addAccButton = new ClickableLabel( this );
    addAccButton->setTextFormat( Qt::RichText );
    addAccButton->setObjectName( "add_account_button" );
    addAccButton->setText( GUIUtil::fontAwesomeRegular("\uf067 ")+tr("Add account") );
    addAccButton->setCursor( Qt::PointingHandCursor );
    accountBar->addWidget( addAccButton );
    addToolBar( Qt::LeftToolBarArea, accountBar );
    connect( addAccButton, SIGNAL( clicked() ), this, SLOT( gotoNewAccountDialog() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );








    //Add the 'Gulden bar' - on the left with the Gulden sign and balance
    guldenBar = new QToolBar( QCoreApplication::translate( "toolbar", "Overview toolbar" ) );
    guldenBar->setObjectName( "gulden_bar" );
    guldenBar->setFixedHeight( horizontalBarHeight );
    guldenBar->setFixedWidth( sideBarWidth );
    guldenBar->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    guldenBar->setMinimumWidth( sideBarWidth );
    guldenBar->setMovable( false );
    guldenBar->setToolButtonStyle( Qt::ToolButtonIconOnly );
    guldenBar->setIconSize( QSize( 18, 18 ) );


    // We place all the widgets for this action bar inside a frame of fixed width - otherwise the sizing comes out wrong
    {
        balanceContainer = new QFrame(this);
        balanceContainer->setObjectName("balance_container");
        QHBoxLayout* layoutBalance = new QHBoxLayout(balanceContainer);
        layoutBalance->setObjectName("balance_layout");
        balanceContainer->setLayout(layoutBalance);
        balanceContainer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        balanceContainer->setContentsMargins( 0, 0, 0, 0 );
        layoutBalance->setContentsMargins( 0, 0, 0, 0 );
        layoutBalance->setSpacing(0);
        guldenBar->addWidget( balanceContainer );


        //Left margin
        {
            QWidget* spacerL = new QWidget(this);
            spacerL->setObjectName("gulden_bar_left_margin");
            spacerL->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
            layoutBalance->addWidget( spacerL );
        }

        QLabel* homeIcon = new ClickableLabel( this );
        homeIcon->setText("\u0120");
        layoutBalance->addWidget( homeIcon );
        homeIcon->setObjectName( "home_button" );
        homeIcon->setCursor( Qt::PointingHandCursor );
        connect( homeIcon, SIGNAL( clicked() ), this, SLOT( gotoWebsite() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );

        // Use spacer to push balance label to the right
        {
            QWidget* spacerMid = new QWidget(this);
            spacerMid->setObjectName("layout_balance_mid_spacer");
            spacerMid->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
            layoutBalance->addWidget( spacerMid );
        }

        labelBalance = new ClickableLabel( balanceContainer );
        labelBalance->setObjectName( "gulden_label_balance" );
        labelBalance->setText( "" );
        layoutBalance->addWidget( labelBalance );

        labelBalanceForex = new ClickableLabel( this );
        labelBalanceForex->setObjectName( "gulden_label_balance_forex" );
        labelBalanceForex->setText( "" );
        labelBalanceForex->setCursor( Qt::PointingHandCursor );
        connect( labelBalanceForex, SIGNAL( clicked() ), this, SLOT( showExchangeRateDialog() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        labelBalanceForex->setVisible(false);
        layoutBalance->addWidget( labelBalanceForex );

        //Right margin
        {
            QWidget* spacerR = new QWidget(this);
            spacerR->setObjectName("gulden_bar_right_margin");
            spacerR->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
            layoutBalance->addWidget( spacerR );
        }

        balanceContainer->setMinimumWidth( sideBarWidth );
    }
    addToolBar( guldenBar );




    //Add spacer bar
    spacerBarL = new QToolBar( QCoreApplication::translate( "toolbar", "Spacer  toolbar" ) );
    spacerBarL->setFixedHeight( horizontalBarHeight );
    spacerBarL->setObjectName( "spacer_bar_left" );
    spacerBarL->setMovable( false );
    //Spacer to fill width
    {
        QWidget* spacerL = new QWidget(this);
        spacerL->setObjectName( "spacer_bar_left_spacer" );
        spacerL->setMinimumWidth( 40 );
        spacerL->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        spacerBarL->addWidget( spacerL );
        spacerBarL->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    }
    addToolBar( spacerBarL );


    tabsBar = findChildren<QToolBar*>( "" )[0];
    //Add the main toolbar - middle (tabs)
    tabsBar->setFixedHeight( horizontalBarHeight );
    tabsBar->setObjectName( "navigation_bar" );
    tabsBar->setMovable( false );
    tabsBar->setToolButtonStyle( Qt::ToolButtonTextOnly );
    //Remove all the actions so we can add them again in a different order
    tabsBar->removeAction( historyAction );
    tabsBar->removeAction( overviewAction );
    tabsBar->removeAction( sendCoinsAction );
    tabsBar->removeAction( receiveCoinsAction );
    tabsBar->removeAction( witnessDialogAction );
    //Setup the tab toolbar
    tabsBar->addAction( witnessDialogAction );
    tabsBar->addAction( receiveCoinsAction );
    tabsBar->addAction( sendCoinsAction );
    tabsBar->addAction( historyAction );

    passwordAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf084), tr("&Password"), this);
    passwordAction->setObjectName("action_password");
    passwordAction->setStatusTip(tr("Change wallet password"));
    passwordAction->setToolTip(passwordAction->statusTip());
    passwordAction->setCheckable(true);
    passwordAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabsBar->addAction(passwordAction);

    backupAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf0c7), tr("&Backup"), this);
    backupAction->setObjectName("action_backup");
    backupAction->setStatusTip(tr("Backup wallet"));
    backupAction->setToolTip(backupAction->statusTip());
    backupAction->setCheckable(true);
    backupAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    tabsBar->addAction(backupAction);

    connect(passwordAction, SIGNAL(triggered()), this, SLOT(gotoPasswordDialog()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
    connect(backupAction, SIGNAL(triggered()), this, SLOT(gotoBackupDialog()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));


    receiveCoinsAction->setChecked( true );

    tabsBar->widgetForAction( historyAction )->setCursor( Qt::PointingHandCursor );

    tabsBar->widgetForAction( passwordAction )->setObjectName( "password_button" );
    tabsBar->widgetForAction( passwordAction )->setContentsMargins( 0, 0, 0, 0 );
    tabsBar->widgetForAction( passwordAction )->setCursor( Qt::PointingHandCursor );
    tabsBar->widgetForAction( backupAction )->setObjectName( "backup_button" );
    tabsBar->widgetForAction( backupAction )->setContentsMargins( 0, 0, 0, 0 );
    tabsBar->widgetForAction( backupAction )->setCursor( Qt::PointingHandCursor );
    tabsBar->widgetForAction( receiveCoinsAction )->setObjectName( "receive_coins_button" );
    tabsBar->widgetForAction( receiveCoinsAction )->setContentsMargins( 0, 0, 0, 0 );
    tabsBar->widgetForAction( receiveCoinsAction )->setCursor( Qt::PointingHandCursor );
    tabsBar->widgetForAction( sendCoinsAction )->setObjectName( "send_coins_button" );
    tabsBar->widgetForAction( sendCoinsAction )->setContentsMargins( 0, 0, 0, 0 );
    tabsBar->widgetForAction( sendCoinsAction )->setCursor( Qt::PointingHandCursor );
    tabsBar->widgetForAction( historyAction )->setObjectName( "history_button" );
    tabsBar->widgetForAction( historyAction )->setContentsMargins( 0, 0, 0, 0 );
    tabsBar->widgetForAction( witnessDialogAction )->setObjectName( "witness_button" );
    tabsBar->widgetForAction( witnessDialogAction )->setContentsMargins( 0, 0, 0, 0 );
    tabsBar->widgetForAction( witnessDialogAction )->setCursor( Qt::PointingHandCursor );
    tabsBar->setContentsMargins( 0, 0, 0, 0 );

    //Spacer to fill width
    {
        QWidget* spacerR = new QWidget(this);
        spacerR->setObjectName("navigation_bar_right_spacer");
        spacerR->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        //Delibritely large amount - to push the next toolbar as far right as possible.
        spacerR->setMinimumWidth( 500000 );
        tabsBar->addWidget( spacerR );
        tabsBar->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    }
    //Only show the actions we want
    receiveCoinsAction->setVisible( true );
    sendCoinsAction->setVisible( true );
    historyAction->setVisible( true );
    passwordAction->setVisible( false );
    backupAction->setVisible( false );
    overviewAction->setVisible( false );
    tabsBar->setWindowTitle( QCoreApplication::translate( "toolbar", "Navigation toolbar" ) );
    addToolBar( tabsBar );






    //Setup the account info bar
    accountInfoBar = new QToolBar( QCoreApplication::translate( "toolbar", "Account info toolbar" ) );
    accountInfoBar->setFixedHeight( horizontalBarHeight );
    accountInfoBar->setObjectName( "account_info_bar" );
    accountInfoBar->setMovable( false );
    accountInfoBar->setToolButtonStyle( Qt::ToolButtonIconOnly );
    accountSummaryWidget = new AccountSummaryWidget( ticker, this );
    accountSummaryWidget->setObjectName( "settings_button" );
    accountSummaryWidget->setContentsMargins( 0, 0, 0, 0 );
    accountInfoBar->setContentsMargins( 0, 0, 0, 0 );
    accountSummaryWidget->setObjectName( "accountSummaryWidget" );
    accountInfoBar->addWidget( accountSummaryWidget );
    addToolBar( accountInfoBar );
    connect(accountSummaryWidget, SIGNAL( requestAccountSettings() ), this, SLOT( showAccountSettings() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    connect(accountSummaryWidget, SIGNAL( requestExchangeRateDialog() ), this, SLOT( showExchangeRateDialog() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );


    //Add spacer bar
    spacerBarR = new QToolBar( QCoreApplication::translate( "toolbar", "Spacer  toolbar" ) );
    spacerBarR->setFixedHeight( horizontalBarHeight );
    spacerBarR->setObjectName( "spacer_bar_right" );
    spacerBarR->setMovable( false );
    //Spacer to fill width
    {
        QWidget* spacerR = new QWidget(this);
        spacerR->setObjectName("spacer_bar_right_spacer");
        spacerR->setMinimumWidth( 40 );
        spacerR->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        spacerBarR->addWidget( spacerR );
        spacerBarR->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    }
    addToolBar( spacerBarR );

    //Hide all toolbars and menus until UI fully loaded
    hideToolBars();
    appMenuBar->setVisible(false);


    //Init the welcome dialog inside walletFrame
    welcomeScreen = new WelcomeDialog(platformStyle, this);
    walletFrame->walletStack->addWidget(welcomeScreen);
    walletFrame->walletStack->setCurrentWidget(welcomeScreen);
    connect(welcomeScreen, SIGNAL( loadWallet() ), walletFrame, SIGNAL( loadWallet() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
}

void GUI::hideToolBars()
{
    LogPrint(BCLog::QT, "GUI::hideToolBars\n");

    if (accountBar) accountBar->setVisible(false);
    if (guldenBar) guldenBar->setVisible(false);
    if (spacerBarL) spacerBarL->setVisible(false);
    if (tabsBar) tabsBar->setVisible(false);
    if (spacerBarR) spacerBarR->setVisible(false);
    if (accountInfoBar) accountInfoBar->setVisible(false);
    if (statusToolBar) statusToolBar->setVisible(false);
}

void GUI::showToolBars()
{
    LogPrint(BCLog::QT, "GUI::showToolBars\n");

    welcomeScreen = NULL;
    if (appMenuBar) appMenuBar->setStyleSheet("");
    if (accountBar) accountBar->setVisible(true);
    if (guldenBar) guldenBar->setVisible(true);
    if (spacerBarL) spacerBarL->setVisible(true);
    if (tabsBar) tabsBar->setVisible(true);
    if (spacerBarR) spacerBarR->setVisible(true);
    if (accountInfoBar) accountInfoBar->setVisible(true);
    if (statusToolBar) statusToolBar->setVisible(progressBarLabel ? progressBarLabel->isVisible() : false);
}


void GUI::doApplyStyleSheet()
{
    LogPrint(BCLog::QT, "GUI::doApplyStyleSheet\n");

    if(!enableWallet || !enableFullUI)
        return;

    //Load our own QSS stylesheet template for 'whole app'
    QFile styleFile( ":Gulden/qss" );
    styleFile.open( QFile::ReadOnly );

    //Replace variables in the 'template' with actual values
    QString style( styleFile.readAll() );
    style.replace( "ACCENT_COLOR_1", QString(ACCENT_COLOR_1) );
    style.replace( "ACCENT_COLOR_2", QString(ACCENT_COLOR_2) );
    style.replace( "TEXT_COLOR_1", QString(TEXT_COLOR_1) );
    style.replace( "COLOR_VALIDATION_FAILED", QString(COLOR_VALIDATION_FAILED) );

    if (sideBarWidth == sideBarWidthExtended)
    {
        style.replace( "SIDE_BAR_WIDTH", SIDE_BAR_WIDTH_EXTENDED );
    }
    else
    {
        style.replace( "SIDE_BAR_WIDTH", SIDE_BAR_WIDTH );
    }

    style.replace( "LOGO_FONT_SIZE", QString(LOGO_FONT_SIZE) );
    style.replace( "TOOLBAR_FONT_SIZE", QString(TOOLBAR_FONT_SIZE) );
    style.replace( "BUTTON_FONT_SIZE", QString(BUTTON_FONT_SIZE) );
    style.replace( "SMALL_BUTTON_FONT_SIZE", QString(SMALL_BUTTON_FONT_SIZE) );
    style.replace( "MINOR_LABEL_FONT_SIZE", QString(MINOR_LABEL_FONT_SIZE) );

    //Apply the final QSS - after making the 'template substitutions'
    //NB! This should happen last after all object IDs etc. are set.
    setStyleSheet( style );
}

void GUI::resizeToolBarsGulden()
{
    LogPrint(BCLog::QT, "GUI::resizeToolBarsGulden\n");

    //Filler for right of menu bar.
    #ifndef MAC_OSX
    menuBarSpaceFiller->move(sideBarWidth, 0);
    #endif
    accountBar->setFixedWidth( sideBarWidth );
    accountBar->setMinimumWidth( sideBarWidth );
    guldenBar->setFixedWidth( sideBarWidth );
    guldenBar->setMinimumWidth( sideBarWidth ); 
    balanceContainer->setMinimumWidth( sideBarWidth );
}

void GUI::doPostInit()
{
    LogPrint(BCLog::QT, "GUI::doPostInit\n");

    //Fonts
    // We 'abuse' the translation system here to allow different 'font stacks' for different languages.
    //QString MAIN_FONTSTACK = QObject::tr("Arial, 'Helvetica Neue', Helvetica, sans-serif");

    appMenuBar->setStyleSheet("QMenuBar{background-color: rgba(255, 255, 255, 0%);} QMenu{background-color: #f3f4f6; border: 1px solid #999; color: black;} QMenu::item { color: black; } QMenu::item:disabled {color: #999;} QMenu::separator{background-color: #999; height: 1px; margin-left: 10px; margin-right: 5px;}");


    {
        // Qt status bar sucks - it is impossible to style nicely, so we just rip the thing out and use a toolbar instead.
        statusBar()->setVisible(false);

        //Allow us to target the progress label for easy styling
        //statusBar()->removeWidget(progressBarLabel);
        //statusBar()->removeWidget(progressBar);
        //statusBar()->removeWidget(frameBlocks);

        //Add a spacer to the frameBlocks so that we can force them to expand to same size as the progress text (Needed for proper centering of progress bar)
        /*QFrame* frameBlocksSpacerL = new QFrame(frameBlocks);
        frameBlocksSpacerL->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        frameBlocksSpacerL->setContentsMargins( 0, 0, 0, 0);
        ((QHBoxLayout*)frameBlocks->layout())->insertWidget(0, frameBlocksSpacerL);*/

        frameBlocks->layout()->setContentsMargins( 0, 0, 0, 0 );
        frameBlocks->layout()->setSpacing( 0 );

        //Hide some of the 'task items' we don't need
        //labelBlocksIcon->setVisible( false );

        //Status bar
        {
            statusToolBar = new QToolBar( QCoreApplication::translate( "toolbar", "Status toolbar" ), this);
            statusToolBar->setObjectName( "status_bar" );
            statusToolBar->setMovable( false );

            QFrame* statusBarStatusArea = new QFrame(statusToolBar);
            statusBarStatusArea->setObjectName("status_bar_status_area");
            statusBarStatusArea->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            statusBarStatusArea->setContentsMargins( 0, 0, 0, 0);
            QHBoxLayout* statusBarStatusAreaLayout = new QHBoxLayout();
            statusBarStatusArea->setLayout(statusBarStatusAreaLayout);
            statusBarStatusAreaLayout->setSpacing(0);
            statusBarStatusAreaLayout->setContentsMargins( 0, 0, 0, 0 );
            statusToolBar->addWidget(statusBarStatusArea);

            statusBarStatusAreaLayout->addWidget(progressBarLabel);

            QFrame* statusProgressSpacerL = new QFrame(statusToolBar);
            statusProgressSpacerL->setObjectName("progress_bar_spacer_left");
            statusProgressSpacerL->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            statusProgressSpacerL->setContentsMargins( 0, 0, 0, 0);
            statusToolBar->addWidget(statusProgressSpacerL);

            QFrame* progressBarWrapper = new QFrame(statusToolBar);
            progressBarWrapper->setObjectName("progress_bar");
            progressBarWrapper->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Preferred);
            progressBarWrapper->setContentsMargins( 0, 0, 0, 0);
            QHBoxLayout* layoutProgressBarWrapper = new QHBoxLayout;
            progressBarWrapper->setLayout(layoutProgressBarWrapper);
            layoutProgressBarWrapper->addWidget(progressBar);
            statusToolBar->addWidget(progressBarWrapper);
            progressBar->setVisible(false);

            QFrame* statusProgressSpacerR = new QFrame(statusToolBar);
            statusProgressSpacerR->setObjectName("progress_bar_spacer_right");
            statusProgressSpacerR->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            statusProgressSpacerR->setContentsMargins( 0, 0, 0, 0);
            statusToolBar->addWidget(statusProgressSpacerR);

            frameBlocks->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
            frameBlocks->setObjectName("status_bar_frame_blocks");
            //Use spacer to push all the icons to the right
            QFrame* frameBlocksSpacerL = new QFrame(frameBlocks);
            frameBlocksSpacerL->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            frameBlocksSpacerL->setContentsMargins( 0, 0, 0, 0);
            ((QHBoxLayout*)frameBlocks->layout())->insertWidget(0, frameBlocksSpacerL, 1);
            statusToolBar->addWidget(frameBlocks);
            //Right margin to match rest of UI
            QFrame* frameBlocksSpacerR = new QFrame(frameBlocks);
            frameBlocksSpacerR->setObjectName("rightMargin");
            frameBlocksSpacerR->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
            frameBlocksSpacerR->setContentsMargins( 0, 0, 0, 0);
            ((QHBoxLayout*)frameBlocks->layout())->addWidget(frameBlocksSpacerR);

            //Use our own styling - clear the styling that is already applied
            progressBar->setStyleSheet("");
            //Hide text we don't want it as it looks cluttered.
            progressBar->setTextVisible(false);
            progressBar->setCursor(Qt::PointingHandCursor);

            addToolBar( Qt::BottomToolBarArea, statusToolBar );

            statusToolBar->setVisible(false);
        }
    }

    backupWalletAction->setIconText( QCoreApplication::translate( "toolbar", "Backup" ) );

    //Change shortcut keys because we have hidden overview pane and changed tab orders
    overviewAction->setShortcut( QKeySequence( Qt::ALT + Qt::Key_0 ) );
    sendCoinsAction->setShortcut( QKeySequence( Qt::ALT + Qt::Key_2 ) );
    receiveCoinsAction->setShortcut( QKeySequence( Qt::ALT + Qt::Key_3 ) );
    historyAction->setShortcut( QKeySequence( Qt::ALT + Qt::Key_1 ) );

    doApplyStyleSheet();

    //Force font antialiasing
    QFont f = QApplication::font();
    f.setStyleStrategy( QFont::PreferAntialias );
    QApplication::setFont( f );

    openAction->setVisible(false);

    setContextMenuPolicy(Qt::NoContextMenu);

    setMinimumSize(860, 520);

    disconnect(backupWalletAction, SIGNAL(triggered()), 0, 0);
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(gotoBackupDialog()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));

    encryptWalletAction->setCheckable(false);
    disconnect(encryptWalletAction, SIGNAL(triggered()), 0, 0);
    connect(encryptWalletAction, SIGNAL(triggered()), this, SLOT(gotoPasswordDialog()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));

    disconnect(changePassphraseAction, SIGNAL(triggered()), 0, 0);
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(gotoPasswordDialog()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));

    if (labelBalance)
        labelBalance->setVisible(false);
}

void GUI::hideProgressBarLabel()
{
    LogPrint(BCLog::QT, "GUI::hideProgressBarLabel\n");

    if (progressBarLabel)
    {
        progressBarLabel->setText("");
        progressBarLabel->setVisible(false);
    }
    if(statusToolBar)
        statusToolBar->setVisible(false);
}

void GUI::showProgressBarLabel()
{
    LogPrint(BCLog::QT, "GUI::showProgressBarLabel\n");

    if (progressBarLabel)
        progressBarLabel->setVisible(true);
    if(statusToolBar)
        statusToolBar->setVisible(true);
}

void GUI::hideBalances()
{
    LogPrint(BCLog::QT, "GUI::hideBalances\n");

    if (!labelBalance)
        return;

    labelBalance->setVisible(false);
    labelBalanceForex->setVisible(false);
    accountSummaryWidget->hideBalances();
}

void GUI::showBalances()
{
    LogPrint(BCLog::QT, "GUI::showBalances\n");

    if (!labelBalance)
        return;

    labelBalance->setVisible(true);
    // Give forex label a chance to update if appropriate.
    updateExchangeRates();
    accountSummaryWidget->showBalances();
}

bool GUI::welcomeScreenIsVisible()
{
    return welcomeScreen != NULL;
}

QDialog* GUI::createDialog(QWidget* parent, QString message, QString confirmLabel, QString cancelLabel, int minWidth, int minHeight, QString objectName)
{
    LogPrint(BCLog::QT, "GUI::createDialog\n");

    QDialog* d = new QDialog(parent);
    d->setWindowFlags(Qt::Dialog);
    d->setMinimumSize(QSize(minWidth, minHeight));
    QVBoxLayout* vbox = new QVBoxLayout();
    vbox->setSpacing(0);
    vbox->setContentsMargins( 0, 0, 0, 0 );

    if (!objectName.isEmpty())
        d->setObjectName(objectName);

    QLabel* labelDialogMessage = new QLabel(d);
    labelDialogMessage->setText(message);
    labelDialogMessage->setObjectName("labelDialogMessage");
    labelDialogMessage->setContentsMargins( 0, 0, 0, 0 );
    labelDialogMessage->setIndent(0);
    labelDialogMessage->setWordWrap(true);
    vbox->addWidget(labelDialogMessage);

    QWidget* spacer = new QWidget(d);
    spacer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    vbox->addWidget(spacer);

    QFrame* horizontalLine = new QFrame(d);
    horizontalLine->setFrameStyle(QFrame::HLine);
    horizontalLine->setFixedHeight(1);
    horizontalLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    horizontalLine->setStyleSheet(GULDEN_DIALOG_HLINE_STYLE);
    vbox->addWidget(horizontalLine);

    QDialogButtonBox::StandardButtons buttons;
    if (!confirmLabel.isEmpty())
    {
        buttons |= QDialogButtonBox::Ok;
    }
    if (!cancelLabel.isEmpty())
    {
        // We use reset button because it shows on the left where we want it.
        buttons |= QDialogButtonBox::Reset;
    }

    QDialogButtonBox* buttonBox = new QDialogButtonBox(buttons, d);
    vbox->addWidget(buttonBox);
    buttonBox->setContentsMargins( 0, 0, 0, 0 );

    if(!confirmLabel.isEmpty())
    {
        buttonBox->button(QDialogButtonBox::Ok)->setObjectName("dialogConfirmButton");
        buttonBox->button(QDialogButtonBox::Ok)->setText(confirmLabel);
        buttonBox->button(QDialogButtonBox::Ok)->setCursor(Qt::PointingHandCursor);
        buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(GULDEN_DIALOG_CONFIRM_BUTTON_STYLE);
        QObject::connect(buttonBox, SIGNAL(accepted()), d, SLOT(accept()));
    }

    if (!cancelLabel.isEmpty())
    {
        buttonBox->button(QDialogButtonBox::Reset)->setObjectName("dialogCancelButton");
        buttonBox->button(QDialogButtonBox::Reset)->setText(cancelLabel);
        buttonBox->button(QDialogButtonBox::Reset)->setCursor(Qt::PointingHandCursor);
        buttonBox->button(QDialogButtonBox::Reset)->setStyleSheet(GULDEN_DIALOG_CANCEL_BUTTON_STYLE);
        QObject::connect(buttonBox->button(QDialogButtonBox::Reset), SIGNAL(clicked()), d, SLOT(reject()));
    }

    d->setLayout(vbox);

    return d;
}

const QString ELLIPSIS("\u2026");
QString limitString(const QString& string, int maxLength)
{
    if (string.length() <= maxLength)
        return string;

    float spacePerPart = (maxLength - ELLIPSIS.length()) / 2.0;
    auto beforeEllipsis = string.left(std::ceil(spacePerPart));
    auto afterEllipsis = string.right(std::floor(spacePerPart));

    return beforeEllipsis + GUIUtil::fontAwesomeLight(ELLIPSIS) + afterEllipsis;
}

static QString superscriptSpan(const QString& sText)
{
    return QString("<span style='font-size: 8px;'>%1</span>").arg(sText);
}

static QString colourSpan(QString sColour, const QString& sText)
{
    return QString("<span style='color: %1;'>%2</span>").arg(sColour).arg(sText);
}

static QString getAccountLabel(CAccount* account)
{
    QString accountName = QString::fromStdString( account->getLabel() );
    accountName = limitString(accountName, 26);

    QString accountNamePrefixIndicator; //Large prefix icon (e.g. building for witness, credit card for normal account)
    QString accountNamePrefixIndicatorQualifier; //small superscript prefix icon in addition to large icon (e.g. warning icon for warning condition, lock for locked witness, eye for read only account)
    if (account->IsReadOnly())
    {
        accountNamePrefixIndicatorQualifier = GUIUtil::fontAwesomeLight("\uf06e"); // Eye
    }
    if (account->IsMobi())
    {
        accountNamePrefixIndicator = GUIUtil::fontAwesomeLight("\uf10b"); // Mobile phone
    }
    else if (account->m_Type == ImportedPrivateKeyAccount)
    {
        accountNamePrefixIndicator = GUIUtil::fontAwesomeLight("\uf084"); // Key
    }
    else if (account->IsPoW2Witness())
    {
        accountNamePrefixIndicator = GUIUtil::fontAwesomeLight("\uf19c"); // Institutional building
        if (account->m_Type == WitnessOnlyWitnessAccount)
            accountNamePrefixIndicator = GUIUtil::fontAwesomeLight("\uf06e"); // Eye
        switch (account->GetWarningState())
        {
            case AccountStatus::WitnessEmpty: break;
            case AccountStatus::Default: accountNamePrefixIndicatorQualifier += GUIUtil::fontAwesomeSolid("\uf023"); break; // Lock
            case AccountStatus::WitnessPending: accountNamePrefixIndicatorQualifier += GUIUtil::fontAwesomeSolid("\uf251"); break; // Hourglass
            case AccountStatus::WitnessExpired: accountNamePrefixIndicatorQualifier += colourSpan("#c97676", GUIUtil::fontAwesomeSolid("\uf12a")); break; // Exclamation
            case AccountStatus::WitnessEnded: accountNamePrefixIndicatorQualifier += GUIUtil::fontAwesomeSolid("\uf11e"); break; // Checkered flag
        }
    }
    else if (!account->IsHD())
    {
        accountNamePrefixIndicator = GUIUtil::fontAwesomeLight("\uf187"); // Archival box
    }
    else
    {
        accountNamePrefixIndicator = GUIUtil::fontAwesomeLight("\uf09d"); // Credit card
    }

    QString accountNamePrefix = QString("<table cellspacing=0 padding=0><tr><td>%1</td><td valign=top>%2</td><table>").arg(accountNamePrefixIndicator).arg(superscriptSpan(accountNamePrefixIndicatorQualifier));
    accountName = QString("<table cellspacing=0 padding=0><tr><td width=28 align=left>%1</td><td width=2></td><td>%2</td></tr></table>").arg(accountNamePrefix).arg(accountName);

    return accountName;
}

void GUI::refreshTabVisibilities()
{
    LogPrint(BCLog::QT, "GUI::refreshTabVisibilities\n");

    receiveCoinsAction->setVisible( true );
    sendCoinsAction->setVisible( true );

    //Required here because when we open wallet and it is already on a read only account restoreCachedWidgetIfNeeded is not called.
    if (pactiveWallet->getActiveAccount()->IsReadOnly())
        sendCoinsAction->setVisible( false );

    if (pactiveWallet->getActiveAccount()->IsPoW2Witness())
    {
        receiveCoinsAction->setVisible( false );
        sendCoinsAction->setVisible( false );
        witnessDialogAction->setVisible( true );
        if ( walletFrame->currentWalletView()->currentWidget() == (QWidget*)walletFrame->currentWalletView()->receiveCoinsPage || walletFrame->currentWalletView()->currentWidget() == (QWidget*)walletFrame->currentWalletView()->sendCoinsPage )
        {
            showWitnessDialog();
        }
    }
    else
    {
        witnessDialogAction->setVisible( false );
        if (walletFrame->currentWalletView()->currentWidget() == (QWidget*)walletFrame->currentWalletView()->witnessDialogPage)
        {
            gotoReceiveCoinsPage();
        }
    }
}


static std::map<QString, CAccount*, std::function<bool(const QString&, const QString&)>> getSortedAccounts()
{
    QCollator collateAccountsNumerically;
    collateAccountsNumerically.setNumericMode(true);
    std::function<bool(const QString&, const QString&)> cmpAccounts = [collateAccountsNumerically](const QString& s1, const QString& s2){ return collateAccountsNumerically.compare(s1, s2) < 0; };
    std::map<QString, CAccount*, std::function<bool(const QString&, const QString&)>> sortedAccounts(cmpAccounts);
    {
        for ( const auto& accountPair : pactiveWallet->mapAccounts )
        {
            if (accountPair.second->m_State == AccountState::Normal || (fShowChildAccountsSeperately && accountPair.second->m_State == AccountState::ShadowChild) )
                sortedAccounts[QString::fromStdString(accountPair.second->getLabel())] = accountPair.second;
        }
    }
    return sortedAccounts;
}

void GUI::refreshAccountControls()
{
    LogPrint(BCLog::QT, "GUI::refreshAccountControls\n");

    refreshTabVisibilities();

    if (pactiveWallet)
    {
        //Disable layout to prevent updating to changes immediately
        accountScrollArea->layout()->setEnabled(false);
        {
            // Get an ordered list of old account labels.
            std::vector<ClickableLabel*> allLabels;
            for (int32_t i=0; i<accountScrollArea->layout()->count(); ++i)
            {
                allLabels.push_back(dynamic_cast<ClickableLabel*>((accountScrollArea->layout()->itemAt(i)->widget())));
            }
            m_accountMap.clear();

            // Sort the accounts.
            auto sortedAccounts = getSortedAccounts();
            {
                //NB! Mutex scope here is important to avoid deadlock inside setActiveAccountButton
                LOCK(pactiveWallet->cs_wallet);

                // Update to the sorted list
                uint32_t nCount = 0;
                for (const auto& sortedIter : sortedAccounts)
                {
                    ClickableLabel* accLabel = nullptr;
                    QString label = getAccountLabel(sortedIter.second);
                    if (allLabels.size() >= nCount+1)
                    {
                        accLabel = allLabels[nCount];
                        accLabel->setText( label );
                    }
                    else
                    {
                        accLabel = createAccountButton( label );
                        accountScrollArea->layout()->addWidget( accLabel );
                    }
                    m_accountMap[accLabel] = sortedIter.second;
                    if (!walletFrame->currentWalletView()->walletModel)
                        return;

                    if (sortedIter.second->getUUID() == walletFrame->currentWalletView()->walletModel->getActiveAccount()->getUUID())
                        setAccountLabelSelected(accLabel);
                    else
                        setAccountLabelUnselected(accLabel);

                    ++nCount;
                }

                // Remove any excess widgets that are still in UI if required.
                for (;nCount < allLabels.size();++nCount)
                {
                    accountScrollArea->layout()->removeWidget(allLabels[nCount]);
                }
            }
        }
        // Force layout to update now that all the changes are made.
        accountScrollArea->layout()->setEnabled(true);
    }
}


ClickableLabel* GUI::createAccountButton( const QString& accountName )
{
    ClickableLabel* newAccountButton = new ClickableLabel( this );
    newAccountButton->setObjectName(QString("account_selection_button_%1").arg(rand()));
    newAccountButton->setTextFormat( Qt::RichText );
    newAccountButton->setText( accountName );
    newAccountButton->setCursor( Qt::PointingHandCursor );
    connect( newAccountButton, SIGNAL( clicked() ), this, SLOT( accountButtonPressed() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    return newAccountButton;
}

void GUI::setActiveAccountButton( ClickableLabel* activeButton )
{
    LogPrint(BCLog::QT, "GUI::setActiveAccountButton\n");

    for ( const auto & button : accountBar->findChildren<ClickableLabel*>( "" ) )
    {
        setAccountLabelUnselected(button);
    }
    setAccountLabelSelected(activeButton);

     // Update the account
    if ( walletFrame->currentWalletView() )
    {
        if ( walletFrame->currentWalletView()->receiveCoinsPage )
        {
            if (!walletFrame->currentWalletView()->walletModel)
                return;

            accountSummaryWidget->setActiveAccount( m_accountMap[activeButton] );
            walletFrame->currentWalletView()->walletModel->setActiveAccount( m_accountMap[activeButton] );

            updateAccount(m_accountMap[activeButton]);
        }
    }
}

void GUI::updateAccount(CAccount* account)
{
    LogPrint(BCLog::QT, "GUI::updateAccount\n");
    LOCK2(cs_main, pactiveWallet->cs_wallet);

    CReserveKeyOrScript* receiveAddress = new CReserveKeyOrScript(pactiveWallet, account, KEYCHAIN_EXTERNAL);
    CPubKey pubKey;
    if (receiveAddress->GetReservedKey(pubKey))
    {
        CKeyID keyID = pubKey.GetID();
        walletFrame->currentWalletView()->receiveCoinsPage->updateAddress( QString::fromStdString(CGuldenAddress(keyID).ToString()) );
    }
    else
    {
        LogPrint(BCLog::ALL, "Keypool exhausted for account.\n");
        walletFrame->currentWalletView()->receiveCoinsPage->updateAddress( "error" );
    }
    walletFrame->currentWalletView()->receiveCoinsPage->setActiveAccount( account );
    receiveAddress->ReturnKey();
    delete receiveAddress;
}

void GUI::balanceChanged()
{
    LogPrint(BCLog::QT, "GUI::balanceChanged\n");

    if (!walletFrame || !walletFrame->currentWalletView() || !walletFrame->currentWalletView()->walletModel)
        return;

    // Force receive Qr code to update on balance change.
    updateAccount( walletFrame->currentWalletView()->walletModel->getActiveAccount() );
}


void GUI::accountNameChanged(CAccount* account)
{
    LogPrint(BCLog::QT, "GUI::accountNameChanged\n");

    //Disable layout to prevent updating to changes immediately
    accountScrollArea->layout()->setEnabled(false);
    {
        accountDeleted(account);
        ClickableLabel* added = accountAddedHelper(account);

        if (!walletFrame || !walletFrame->currentWalletView() || !walletFrame->currentWalletView()->walletModel)
            return;

        if (account->getUUID() == walletFrame->currentWalletView()->walletModel->getActiveAccount()->getUUID())
            setAccountLabelSelected(added);
    }
    // Force layout to update now that all the changes are made.
    accountScrollArea->layout()->setEnabled(true);
}

void GUI::accountWarningChanged(CAccount* account)
{
    LogPrint(BCLog::QT, "GUI::accountWarningChanged\n");

    if (!account)
        return;

    boost::uuids::uuid searchUUID = account->getUUID();
    for (auto& iter : m_accountMap)
    {
        if (iter.second->getUUID() == searchUUID)
        {
            iter.first->setText( getAccountLabel(account) );
            break;
        }
    }
}

void GUI::activeAccountChanged(CAccount* account)
{
    LogPrint(BCLog::QT, "GUI::activeAccountChanged\n");

    if (accountSummaryWidget)
        accountSummaryWidget->setActiveAccount(account);

    refreshTabVisibilities();
    if ( walletFrame)
        walletFrame->currentWalletView()->witnessDialogPage->update();

    //Update account name 'in place' in account list
    bool haveAccount=false;
    if (pactiveWallet)
    {
        for ( const auto& accountPair : m_accountMap )
        {
            if (accountPair.second == account)
            {
                haveAccount = true;
                if (!accountPair.first->isChecked())
                {
                    setAccountLabelSelected(accountPair.first);
                }
                accountPair.first->setText( getAccountLabel(account) );
            }
            else if (accountPair.first->isChecked())
            {
                setAccountLabelUnselected(accountPair.first);
            }
        }
    }

    if(!haveAccount)
    {
        refreshAccountControls();
    }
}

ClickableLabel* GUI::accountAddedHelper(CAccount* addedAccount)
{
    if (pactiveWallet)
    {
        LOCK(pactiveWallet->cs_wallet);

        QString addedAccountLabel = QString::fromStdString(addedAccount->getLabel());
        auto sortedAccounts = getSortedAccounts();
        if (sortedAccounts.find(addedAccountLabel) != sortedAccounts.end())
        {
            for (const auto& [accountLabel, account] : m_accountMap)
            {
                if (account->getUUID() == addedAccount->getUUID())
                {
                    return accountLabel;
                }
            }
        }

        sortedAccounts[addedAccountLabel] = addedAccount;
        uint32_t nCount = 0;
        for (const auto& [sortedLabel, sortedAccount] : sortedAccounts)
        {
            (unused)sortedLabel;
            if (sortedAccount->getUUID() == addedAccount->getUUID())
            {
                QString decoratedAccountlabel = getAccountLabel(sortedAccount);
                ClickableLabel* accLabel = createAccountButton(decoratedAccountlabel);
                m_accountMap[accLabel] = sortedAccount;
                ((QVBoxLayout*)accountScrollArea->layout())->insertWidget(nCount, accLabel);
                return accLabel;
            }
            nCount++;
        }
    }
    return nullptr;
}

void GUI::accountAdded(CAccount* addedAccount)
{
    LogPrint(BCLog::QT, "GUI::accountAdded\n");

    if (!addedAccount || (addedAccount->m_State != AccountState::Normal && !(fShowChildAccountsSeperately && addedAccount->m_State == AccountState::ShadowChild)) )
        return;

    //Disable layout to prevent updating to changes immediately
    accountScrollArea->layout()->setEnabled(false);
    {
        if (!walletFrame || !walletFrame->currentWalletView() || !walletFrame->currentWalletView()->walletModel)
            return;

        ClickableLabel* added = accountAddedHelper(addedAccount);
        if (addedAccount->getUUID() == walletFrame->currentWalletView()->walletModel->getActiveAccount()->getUUID())
            setAccountLabelSelected(added);
    }
    // Force layout to update now that all the changes are made.
    accountScrollArea->layout()->setEnabled(true);
}

void GUI::accountDeleted(CAccount* account)
{
    LogPrint(BCLog::QT, "GUI::accountDeleted\n");

    if (!account)
        return;

    boost::uuids::uuid searchUUID = account->getUUID();
    for (auto iter = m_accountMap.begin(); iter != m_accountMap.end(); ++iter)
    {
        if (searchUUID == iter->second->getUUID())
        {
            accountScrollArea->layout()->removeWidget(iter->first);
            iter->first->deleteLater();
            m_accountMap.erase(iter);
            break;
        }
    }
}

void GUI::accountButtonPressed()
{
    QObject* sender = this->sender();
    ClickableLabel* accButton = qobject_cast<ClickableLabel*>( sender );
    setActiveAccountButton( accButton );
    restoreCachedWidgetIfNeeded();
}


void GUI::promptImportPrivKey(const QString accountName)
{
    LogPrint(BCLog::QT, "GUI::promptImportPrivKey\n");

    std::string adjustedAccountName = accountName.toStdString();

    if (adjustedAccountName.empty())
        adjustedAccountName = tr("Imported key").toStdString();

    ImportPrivKeyDialog dlg(this);
    if (dlg.exec())
    {
        // Temporarily unlock for account generation.
        SecureString encodedPrivKey = dlg.getPrivKey();
        std::function<void (void)> successCallback = [=](){ pactiveWallet->importPrivKey(encodedPrivKey, adjustedAccountName); };
        if (pactiveWallet->IsLocked())
        {
            uiInterface.RequestUnlockWithCallback(pactiveWallet, _("Wallet unlock required to import private key"), successCallback);
        }
        else
        {
            successCallback();
        }
        return;
    }
}

void GUI::promptImportWitnessOnlyAccount(QString accountName)
{
    LogPrint(BCLog::QT, "GUI::promptImportWitnessOnlyAccount\n");

    if (accountName.isEmpty())
        accountName = tr("Imported witness");

    ImportWitnessDialog dlg(this);
    if (dlg.exec())
    {
        // Temporarily unlock for account generation.
        SecureString witnessURL = dlg.getWitnessURL();
        std::function<void (void)> successCallback = [=](){pactiveWallet->importWitnessOnlyAccountFromURL(witnessURL, accountName.toStdString());};
        if (pactiveWallet->IsLocked())
        {
            uiInterface.RequestUnlockWithCallback(pactiveWallet, _("Wallet unlock required to import witness-only account"), successCallback);
        }
        else
        {
            successCallback();
        }
    }
}

void GUI::promptRescan()
{
    LogPrint(BCLog::QT, "GUI::promptRescan\n");

    // Whenever a key is imported, we need to scan the whole chain - do so now
    pactiveWallet->nTimeFirstKey = 1;
    boost::thread t(rescanThread); // thread runs free
}

void GUI::gotoWebsite()
{
    LogPrint(BCLog::QT, "GUI::gotoWebsite\n");

    QDesktopServices::openUrl( QUrl( "http://www.Gulden.com/" ) );
}

void GUI::restoreCachedWidgetIfNeeded()
{
    LogPrint(BCLog::QT, "GUI::restoreCachedWidgetIfNeeded\n");

    bool stateReceiveCoinsAction = true;
    bool stateSendCoinsAction = true;

    walletFrame->currentWalletView()->sendCoinsPage->update();
    walletFrame->currentWalletView()->witnessDialogPage->update();

    if (pactiveWallet->getActiveAccount()->IsReadOnly())
    {
        stateSendCoinsAction = false;
        if ( walletFrame->currentWalletView()->currentWidget() == (QWidget*)walletFrame->currentWalletView()->sendCoinsPage )
        {
            gotoReceiveCoinsPage();
        }
    }
    if (pactiveWallet->getActiveAccount()->IsPoW2Witness())
    {
        witnessDialogAction->setVisible( true );
        stateReceiveCoinsAction = false;
        stateSendCoinsAction = false;
        if ( walletFrame->currentWalletView()->currentWidget() == (QWidget*)walletFrame->currentWalletView()->receiveCoinsPage || walletFrame->currentWalletView()->currentWidget() == (QWidget*)walletFrame->currentWalletView()->sendCoinsPage )
        {
            showWitnessDialog();
        }
    }
    else
    {
        if (cacheCurrentWidget && cacheCurrentWidget == (QWidget*)walletFrame->currentWalletView()->witnessDialogPage)
            cacheCurrentWidget = nullptr;

        witnessDialogAction->setVisible( false );
        if ( walletFrame->currentWalletView()->currentWidget() == (QWidget*)walletFrame->currentWalletView()->witnessDialogPage )
        {
            gotoReceiveCoinsPage();
        }
    }

    historyAction->setVisible( true );
    passwordAction->setVisible( false );
    backupAction->setVisible( false );
    overviewAction->setVisible( true );

    if (dialogPasswordModify)
    {
        walletFrame->currentWalletView()->removeWidget( dialogPasswordModify );
        dialogPasswordModify->deleteLater();
        dialogPasswordModify = NULL;
    }
    if (dialogBackup)
    {
        walletFrame->currentWalletView()->removeWidget( dialogBackup );
        dialogBackup->deleteLater();
        dialogBackup = NULL;
    }
    if (dialogNewAccount)
    {
        walletFrame->currentWalletView()->removeWidget( dialogNewAccount );
        dialogNewAccount->deleteLater();
        dialogNewAccount = NULL;
    }
    if (dialogAccountSettings)
    {
        walletFrame->currentWalletView()->removeWidget( dialogAccountSettings );
        dialogAccountSettings->deleteLater();
        dialogAccountSettings = NULL;
    }
    if (cacheCurrentWidget)
    {
        walletFrame->currentWalletView()->setCurrentWidget( cacheCurrentWidget );
        cacheCurrentWidget = NULL;
    }

    if (receiveCoinsAction)
        receiveCoinsAction->setVisible( stateReceiveCoinsAction );
    if (sendCoinsAction)
        sendCoinsAction->setVisible( stateSendCoinsAction );
}

void GUI::gotoNewAccountDialog()
{
    LogPrint(BCLog::QT, "GUI::gotoNewAccountDialog\n");

    if ( walletFrame )
    {
        restoreCachedWidgetIfNeeded();

        dialogNewAccount = new NewAccountDialog( platformStyle, walletFrame->currentWalletView(), walletFrame->currentWalletView()->walletModel);
        connect( dialogNewAccount, SIGNAL( cancel() ), this, SLOT( cancelNewAccountDialog() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        connect( dialogNewAccount, SIGNAL( accountAdded() ), this, SLOT( acceptNewAccount() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        connect( dialogNewAccount, SIGNAL( addAccountMobile() ), this, SLOT( acceptNewAccountMobile() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        cacheCurrentWidget = walletFrame->currentWalletView()->currentWidget();
        walletFrame->currentWalletView()->addWidget( dialogNewAccount );
        walletFrame->currentWalletView()->setCurrentWidget( dialogNewAccount );
    }
}

void GUI::gotoPasswordDialog()
{
    LogPrint(BCLog::QT, "GUI::gotoPasswordDialog\n");

    if ( walletFrame )
    {
        restoreCachedWidgetIfNeeded();

        passwordAction->setVisible( true );
        backupAction->setVisible( true );
        passwordAction->setChecked(true);
        backupAction->setChecked(false);

        receiveCoinsAction->setVisible( false );
        sendCoinsAction->setVisible( false );
        historyAction->setVisible( false );
        overviewAction->setVisible( false );
        witnessDialogAction->setVisible( false );

        dialogPasswordModify = new PasswordModifyDialog( platformStyle, walletFrame->currentWalletView() );
        connect( dialogPasswordModify, SIGNAL( dismiss() ), this, SLOT( dismissPasswordDialog() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        cacheCurrentWidget = walletFrame->currentWalletView()->currentWidget();
        walletFrame->currentWalletView()->addWidget( dialogPasswordModify );
        walletFrame->currentWalletView()->setCurrentWidget( dialogPasswordModify );
    }
}

void GUI::gotoBackupDialog()
{
    LogPrint(BCLog::QT, "GUI::gotoBackupDialog\n");

    if ( walletFrame )
    {
        restoreCachedWidgetIfNeeded();

        passwordAction->setVisible( true );
        backupAction->setVisible( true );
        passwordAction->setChecked(false);
        backupAction->setChecked(true);

        receiveCoinsAction->setVisible( false );
        sendCoinsAction->setVisible( false );
        historyAction->setVisible( false );
        overviewAction->setVisible( false );
        witnessDialogAction->setVisible( false );

        dialogBackup = new BackupDialog( platformStyle, walletFrame->currentWalletView(), walletFrame->currentWalletView()->walletModel);
        connect( dialogBackup, SIGNAL( saveBackupFile() ), walletFrame, SLOT( backupWallet() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        connect( dialogBackup, SIGNAL( dismiss() ), this, SLOT( dismissBackupDialog() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        cacheCurrentWidget = walletFrame->currentWalletView()->currentWidget();
        walletFrame->currentWalletView()->addWidget( dialogBackup );
        walletFrame->currentWalletView()->setCurrentWidget( dialogBackup );
    }
}

void GUI::dismissBackupDialog()
{
    LogPrint(BCLog::QT, "GUI::dismissBackupDialog\n");

    restoreCachedWidgetIfNeeded();
}

void GUI::dismissPasswordDialog()
{
    LogPrint(BCLog::QT, "GUI::dismissPasswordDialog\n");

    restoreCachedWidgetIfNeeded();
}

void GUI::cancelNewAccountDialog()
{
    LogPrint(BCLog::QT, "GUI::cancelNewAccountDialog\n");

    restoreCachedWidgetIfNeeded();
}

void GUI::acceptNewAccount()
{
    LogPrint(BCLog::QT, "GUI::acceptNewAccount\n");

    const auto newAccountType = dialogNewAccount->getAccountType();
    if (newAccountType == NewAccountType::ImportKey)
    {
        gotoReceiveCoinsPage();
        promptImportPrivKey(dialogNewAccount->getAccountName());
        return;
    }
    else if(newAccountType == NewAccountType::WitnessOnly)
    {
        gotoReceiveCoinsPage();
        promptImportWitnessOnlyAccount(dialogNewAccount->getAccountName());
        return;
    }

    if ( !dialogNewAccount->getAccountName().simplified().isEmpty() )
    {
        //fixme: (2.1) This can be improved; we don't really need to unlock for every single creation only sometimes
        //This is here to stop the weird effect of shadow thread requesting password -after- account creation though
        //This should tie in better with the shadow thread..

        // Temporarily unlock for account generation.
        std::function<void (void)> successCallback = [=]()
        {
            CAccount* newAccount = nullptr;
            if (newAccountType == NewAccountType::FixedDeposit)
            {
                newAccount = pactiveWallet->GenerateNewAccount(dialogNewAccount->getAccountName().toStdString(), AccountState::Normal, AccountType::PoW2Witness);
            }
            else
            {
                newAccount = pactiveWallet->GenerateNewAccount(dialogNewAccount->getAccountName().toStdString(), AccountState::Normal, AccountType::Desktop);
            }

            if (!newAccount)
            {
                std::string strAlert = "Failed to create new account";
                CAlert::Notify(strAlert, true, true);
                LogPrintf("%s", strAlert.c_str());
                return;
            }
            restoreCachedWidgetIfNeeded();
            if (newAccountType == NewAccountType::FixedDeposit)
            {
                newAccount->SetWarningState(AccountStatus::WitnessEmpty);
                static_cast<const CGuldenWallet*>(pactiveWallet)->NotifyAccountWarningChanged(pactiveWallet, newAccount);
                showWitnessDialog();
            }
            else
            {
                gotoReceiveCoinsPage();
            }
        };
        if (pactiveWallet->IsLocked())
        {
            uiInterface.RequestUnlockWithCallback(pactiveWallet, _("Wallet unlock required for account creation"), successCallback);
        }
        else
        {
            successCallback();
        }
        return;
    }
    else
    {
        //fixme: (2.1) Mark invalid.
    }
}

void GUI::acceptNewAccountMobile()
{
    LogPrint(BCLog::QT, "GUI::acceptNewAccountMobile\n");

    restoreCachedWidgetIfNeeded();
}

void GUI::showAccountSettings()
{
    LogPrint(BCLog::QT, "GUI::showAccountSettings\n");

    if ( walletFrame )
    {
        restoreCachedWidgetIfNeeded();

        dialogAccountSettings = new AccountSettingsDialog( platformStyle, walletFrame->currentWalletView(), walletFrame->currentWalletView()->walletModel->getActiveAccount(), walletFrame->currentWalletView()->walletModel);
        connect( dialogAccountSettings, SIGNAL( dismissAccountSettings() ), this, SLOT( dismissAccountSettings() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        connect( walletFrame->currentWalletView()->walletModel, SIGNAL( activeAccountChanged(CAccount*) ), dialogAccountSettings, SLOT( activeAccountChanged(CAccount*) ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        cacheCurrentWidget = walletFrame->currentWalletView()->currentWidget();
        walletFrame->currentWalletView()->addWidget( dialogAccountSettings );
        walletFrame->currentWalletView()->setCurrentWidget( dialogAccountSettings );
    }
}

void GUI::dismissAccountSettings()
{
    LogPrint(BCLog::QT, "GUI::dismissAccountSettings\n");

    restoreCachedWidgetIfNeeded();
}

void GUI::showExchangeRateDialog()
{
    LogPrint(BCLog::QT, "GUI::showExchangeRateDialog\n");

    if (!dialogExchangeRate)
    {
        CurrencyTableModel* currencyTabelmodel = ticker->GetCurrencyTableModel();
        currencyTabelmodel->setBalance( walletFrame->currentWalletView()->walletModel->getBalance() );
        connect( walletFrame->currentWalletView()->walletModel, SIGNAL( balanceChanged(WalletBalances, CAmount, CAmount, CAmount) ), currencyTabelmodel , SLOT( balanceChanged(WalletBalances, CAmount, CAmount, CAmount) ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        dialogExchangeRate = new ExchangeRateDialog( platformStyle, this, currencyTabelmodel );
        dialogExchangeRate->setOptionsModel( optionsModel );
    }
    dialogExchangeRate->show();
}





std::string CurrencySymbolForCurrencyCode(const std::string& currencyCode)
{
    static std::map<std::string, std::string> currencyCodeSymbolMap = {
        {"ALL", "Lek"},
        {"AFN", "؋"},
        {"ARS", "$"},
        {"AWG", "ƒ"},
        {"AUD", "$"},
        {"AZN", "ман"},
        {"BSD", "$"},
        {"BBD", "$"},
        {"BYN", "Br"},
        {"BZD", "BZ$"},
        {"BMD", "$"},
        {"BOB", "$b"},
        {"BAM", "KM"},
        {"BWP", "P"},
        {"BGN", "лв"},
        {"BRL", "R$"},
        {"BND", "$"},
        {"BTC", GUIUtil::fontAwesomeBrand("\uF15A").toStdString()},
        {"KHR", "៛"},
        {"CAD", "$"},
        {"KYD", "$"},
        {"CLP", "$"},
        {"CNY", "¥"},
        {"COP", "$"},
        {"CRC", "₡"},
        {"HRK", "kn"},
        {"CUP", "₱"},
        {"CZK", "Kč"},
        {"DKK", "kr"},
        {"DOP", "RD$"},
        {"XCD", "$"},
        {"EGP", "£"},
        {"SVC", "$"},
        {"EUR", "€"},
        {"FKP", "£"},
        {"FJD", "$"},
        {"GHS", "¢"},
        {"GIP", "£"},
        {"GTQ", "Q"},
        {"GGP", "£"},
        {"GYD", "$"},
        {"HNL", "L"},
        {"HKD", "$"},
        {"HUF", "Ft"},
        {"ISK", "kr"},
        {"INR", ""},
        {"IDR", "Rp"},
        {"IRR", "﷼"},
        {"IMP", "£"},
        {"ILS", "₪"},
        {"JMD", "J$"},
        {"JPY", "¥"},
        {"JEP", "£"},
        {"KZT", "лв"},
        {"KPW", "₩"},
        {"KRW", "₩"},
        {"KGS", "лв"},
        {"LAK", "₭"},
        {"LBP", "£"},
        {"LRD", "$"},
        {"MKD", "ден"},
        {"MYR", "RM"},
        {"MUR", "₨"},
        {"MXN", "$"},
        {"MNT", "₮"},
        {"MZN", "MT"},
        {"NAD", "$"},
        {"NPR", "₨"},
        {"ANG", "ƒ"},
        {"NZD", "$"},
        {"NIO", "C$"},
        {"NGN", "₦"},
        {"KPW", "₩"},
        {"NOK", "kr"},
        {"OMR", "﷼"},
        {"PKR", "₨"},
        {"PAB", "B/."},
        {"PYG", "Gs"},
        {"PEN", "S/."},
        {"PHP", "₱"},
        {"PLN", "zł"},
        {"QAR", "﷼"},
        {"RON", "lei"},
        {"RUB", "₽"},
        {"SHP", "£"},
        {"SAR", "﷼"},
        {"RSD", "Дин."},
        {"SCR", "₨"},
        {"SGD", "$"},
        {"SBD", "$"},
        {"SOS", "S"},
        {"ZAR", "R"},
        {"KRW", "₩"},
        {"LKR", "₨"},
        {"SEK", "kr"},
        {"CHF", "CHF"},
        {"SRD", "$"},
        {"SYP", "£"},
        {"TWD", "NT$"},
        {"THB", "฿"},
        {"TTD", "TT$"},
        {"TRY", ""},
        {"TVD", "$"},
        {"UAH", "₴"},
        {"GBP", "£"},
        {"USD", "$"},
        {"UYU", "$U"},
        {"UZS", "лв"},
        {"VEF", "Bs"},
        {"VND", "₫"},
        {"YER", "﷼"},
        {"ZWD", "Z$"},
        {"NLG", "\u0120"}
    };

    if (currencyCodeSymbolMap.find(currencyCode) != currencyCodeSymbolMap.end())
    {
        return currencyCodeSymbolMap[currencyCode];
    }
    return "";
}

