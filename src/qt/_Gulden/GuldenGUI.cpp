// Copyright (c) 2015 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "GuldenGUI.h"
#include "bitcoingui.h"
#include "platformstyle.h"
#include "bitcoinunits.h"
#include "clickablelabel.h"
#include "receivecoinsdialog.h"
#include "main.h"

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

#include <_Gulden/accountsummarywidget.h>
#include <_Gulden/newaccountdialog.h>
#include <_Gulden/exchangeratedialog.h>
#include <_Gulden/accountsettingsdialog.h>
#include "wallet/wallet.h"
#include "walletframe.h"
#include "walletview.h"
#include "utilmoneystr.h"
#include "passwordmodifydialog.h"
#include "backupdialog.h"
#include "welcomedialog.h"
#include "ticker.h"
#include "nockssettings.h"
#include "bitcoinunits.h"
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

//Toolbar constants
unsigned int sideBarWidthNormal = 300;
unsigned int sideBarWidth = sideBarWidthNormal;
const unsigned int horizontalBarHeight = 60;
const char* SIDE_BAR_WIDTH = "300px";
//Extended width for large balances.
unsigned int sideBarWidthExtended = 340;
const char* SIDE_BAR_WIDTH_EXTENDED = "340px";



GuldenProxyStyle::GuldenProxyStyle()
: QProxyStyle("windows") //Use the same style on all platforms to simplify skinning
{
    altDown = false;
}

void GuldenProxyStyle::drawItemText(QPainter *painter, const QRect &rectangle, int alignment, const QPalette &palette, bool enabled, const QString &text, QPalette::ColorRole textRole) const
{
    // Only draw underline hints on buttons etc. if the alt key is pressed.
    if (altDown)
    {
        alignment |= Qt::TextShowMnemonic;
        alignment &= ~(Qt::TextHideMnemonic);
    }
    else
    {
        alignment |= Qt::TextHideMnemonic;
        alignment &= ~(Qt::TextShowMnemonic);
    }
    
    QProxyStyle::drawItemText(painter, rectangle, alignment, palette, enabled, text, textRole);
}


bool GuldenEventFilter::eventFilter(QObject *obj, QEvent *evt)
{
    if (evt->type() == QEvent::KeyPress || evt->type() == QEvent::KeyRelease)
    {
        QKeyEvent *KeyEvent = (QKeyEvent*)evt;
        if (KeyEvent->key() == Qt::Key_Alt)
        {
            guldenProxyStyle->altDown = (evt->type() == QEvent::KeyPress);
            parentObject->repaint();
        }
    }
    else if(evt->type() == QEvent::ApplicationDeactivate || evt->type() == QEvent::ApplicationStateChange || evt->type() == QEvent::Leave)
    {
        if(guldenProxyStyle->altDown)
        {
            guldenProxyStyle->altDown = false;
            parentObject->repaint();
        }
    }
    return QObject::eventFilter(obj, evt);
}


    
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

static void NotifyRequestUnlockS(GuldenGUI* parent, CWallet* wallet, std::string reason)
{
    QMetaObject::invokeMethod(parent, "NotifyRequestUnlock", Qt::QueuedConnection, Q_ARG(void*, wallet), Q_ARG(QString, QString::fromStdString(reason)));
}

GuldenGUI::GuldenGUI( BitcoinGUI* pImpl )
: QObject()
, m_pImpl( pImpl )
, accountBar( NULL )
, guldenBar( NULL )
, spacerBarL( NULL )
, spacerBarR( NULL )
, tabsBar( NULL )
, accountInfoBar( NULL )
, statusBar( NULL )
, menuBarSpaceFiller( NULL )
, balanceContainer( NULL )
, welcomeScreen( NULL )
, accountScrollArea( NULL )
, accountSummaryWidget( NULL )   
, dialogNewAccount( NULL )
, dialogAccountSettings( NULL )
, dialogBackup( NULL )
, dialogPasswordModify( NULL )
, dialogExchangeRate ( NULL )
, cacheCurrentWidget( NULL )
, ticker( NULL )
, nocksSettings( NULL )
, labelBalance ( NULL )
, labelBalanceForex ( NULL )
, passwordAction( NULL )
, backupAction( NULL )
, optionsModel( NULL )
, receiveAddress( NULL )
, balanceCached( 0 )
, unconfirmedBalanceCached( 0 )
, immatureBalanceCached( 0 )
, watchOnlyBalanceCached( 0 )
, watchUnconfBalanceCached( 0 )
, watchImmatureBalanceCached( 0 )
{
    ticker = new CurrencyTicker( this );
    nocksSettings = new NocksSettings( this );

    //Start the ticker polling off - after the initial call the ticker will schedule the subsequent ones internally.
    ticker->pollTicker();
    nocksSettings->pollSettings();
    
    connect( ticker, SIGNAL( exchangeRatesUpdated() ), this, SLOT( updateExchangeRates() ) );
    
    uiInterface.RequestUnlock.connect(boost::bind(NotifyRequestUnlockS, this, _1, _2));
}


void GuldenGUI::NotifyRequestUnlock(void* wallet, QString reason)
{
    static bool once=false;
    if (!once)
    {
        once = true;
        LogPrintf("NotifyRequestUnlock\n");
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, m_pImpl);
        dlg.setModel(new WalletModel(NULL, (CWallet*)wallet, NULL, NULL));
        dlg.exec();
        once = false;
    }
}

GuldenGUI::~GuldenGUI()
{

}

void GuldenGUI::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    balanceCached = balance;
    unconfirmedBalanceCached = unconfirmedBalance;
    immatureBalanceCached = immatureBalance;
    watchOnlyBalanceCached = watchOnlyBalance;
    watchUnconfBalanceCached = watchUnconfBalance;
    watchImmatureBalanceCached = watchImmatureBalance;
    
    if (!labelBalance || !labelBalanceForex)
        return;
    
    CAmount displayBalance = balance + unconfirmedBalance + immatureBalance;
    labelBalance->setText(BitcoinUnits::format(BitcoinUnits::BTC, displayBalance, false, BitcoinUnits::separatorStandard, 2));
    if (displayBalance > 0 && optionsModel)
    {
        labelBalanceForex->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + BitcoinUnits::format(BitcoinUnits::Unit::BTC, ticker->convertGuldenToForex(displayBalance, optionsModel->guldenSettings->getLocalCurrency().toStdString()), false, BitcoinUnits::separatorAlways, 2) + QString(")"));
        labelBalanceForex->setVisible(true);
    }
    else
    {
        labelBalanceForex->setVisible(false);
    }
    
    if (accountScrollArea && displayBalance > 999999 * COIN && sideBarWidth != sideBarWidthExtended)
    {
        sideBarWidth = sideBarWidthExtended;
        doApplyStyleSheet();
        resizeToolBarsGulden();
    }
    else if (accountScrollArea && displayBalance < 999999 * COIN && sideBarWidth == sideBarWidthExtended)
    {
        sideBarWidth = sideBarWidthNormal;
        doApplyStyleSheet();
        resizeToolBarsGulden();
    }
    
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
    
}

void GuldenGUI::updateExchangeRates()
{
    setBalance(balanceCached, unconfirmedBalanceCached, immatureBalanceCached, watchOnlyBalanceCached, watchUnconfBalanceCached, watchImmatureBalanceCached);
}

void GuldenGUI::setOptionsModel(OptionsModel* optionsModel_)
{
    optionsModel = optionsModel_;
    ticker->setOptionsModel(optionsModel);
    optionsModel->setTicker(ticker);
    optionsModel->setNocksSettings(nocksSettings);
    accountSummaryWidget->setOptionsModel(optionsModel);
    connect( optionsModel->guldenSettings, SIGNAL(  localCurrencyChanged(QString) ), this, SLOT( updateExchangeRates() ) );
    updateExchangeRates();
    
}

void GuldenGUI::createToolBarsGulden()
{
    //Filler for right of menu bar.
    #ifndef MAC_OSX
    menuBarSpaceFiller = new QFrame( m_pImpl );
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
        QFrame* myAccountsFrame = new QFrame( m_pImpl );
        myAccountsFrame->setObjectName( "frameMyAccounts" );
        QHBoxLayout* layoutMyAccounts = new QHBoxLayout;
        myAccountsFrame->setLayout(layoutMyAccounts);
        myAccountsFrame->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        accountBar->addWidget( myAccountsFrame );
        myAccountsFrame->setContentsMargins( 0, 0, 0, 0 );
        layoutMyAccounts->setSpacing(0);
        layoutMyAccounts->setContentsMargins( 0, 0, 0, 0 );
        
        
        ClickableLabel* myAccountLabel = new ClickableLabel( myAccountsFrame );
        myAccountLabel->setObjectName( "labelMyAccounts" );
        myAccountLabel->setText( "My accounts" );
        layoutMyAccounts->addWidget( myAccountLabel );
        myAccountLabel->setContentsMargins( 0, 0, 0, 0 );
        
        //Spacer to fill width
        {
            QWidget* spacerMid = new QWidget( myAccountsFrame );
            spacerMid->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
            layoutMyAccounts->addWidget( spacerMid );
        }
        
        ClickableLabel* labelWalletSettings = new ClickableLabel( myAccountsFrame );
        labelWalletSettings->setText( "" );
        labelWalletSettings->setObjectName( "labelWalletSettings" );
        labelWalletSettings->setCursor ( Qt::PointingHandCursor );
        layoutMyAccounts->addWidget( labelWalletSettings );
        labelWalletSettings->setContentsMargins( 0, 0, 0, 0 );
        
        connect( labelWalletSettings, SIGNAL( clicked() ), this, SLOT( gotoPasswordDialog() ) );
    }

    //Spacer to fill height
    {
        QScrollArea* scrollArea = new QScrollArea ( m_pImpl );
        accountScrollArea = new QFrame( scrollArea );
        scrollArea->setContentsMargins( 0, 0, 0, 0);
        scrollArea->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
        scrollArea->setWidget(accountScrollArea);
        scrollArea->setWidgetResizable(true);
        
        accountScrollArea->setContentsMargins( 0, 0, 0, 0);
        accountScrollArea->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Maximum );
        
        
        accountBar->addWidget( scrollArea );
        
        QVBoxLayout* vbox = new QVBoxLayout();
        vbox->setSpacing(0);
        vbox->setContentsMargins( 0, 0, 0, 0 );
        
        accountScrollArea->setLayout( vbox );
    }

    QPushButton* addAccButton = new QPushButton( m_pImpl );
    addAccButton->setText( " Add account" );
    addAccButton->setObjectName( "add_account_button" );
    addAccButton->setCursor( Qt::PointingHandCursor );
    accountBar->addWidget( addAccButton );
    m_pImpl->addToolBar( Qt::LeftToolBarArea, accountBar );
    connect( addAccButton, SIGNAL( clicked() ), this, SLOT( gotoNewAccountDialog() ) );








    //Add the 'Gulden bar' - on the left with the Gulden sign and balance
    guldenBar = new QToolBar( QCoreApplication::translate( "toolbar", "Overview toolbar" ) );
    guldenBar->setFixedHeight( horizontalBarHeight );
    guldenBar->setFixedWidth( sideBarWidth );
    guldenBar->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    guldenBar->setMinimumWidth( sideBarWidth );
    guldenBar->setObjectName( "gulden_bar" );
    guldenBar->setMovable( false );
    guldenBar->setToolButtonStyle( Qt::ToolButtonIconOnly );
    guldenBar->setIconSize( QSize( 18, 18 ) );
        
    
    // We place all the widgets for this action bar inside a frame of fixed width - otherwise the sizing comes out wrong
    {
        balanceContainer = new QFrame();
        QHBoxLayout* layoutBalance = new QHBoxLayout;
        balanceContainer->setLayout(layoutBalance);
        balanceContainer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        balanceContainer->setContentsMargins( 0, 0, 0, 0 );
        layoutBalance->setContentsMargins( 0, 0, 0, 0 );
        layoutBalance->setSpacing(0);
        guldenBar->addWidget( balanceContainer );
        
        
        //Left margin
        {
            QWidget* spacerL = new QWidget();
            spacerL->setObjectName("gulden_bar_left_margin");
            spacerL->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
            layoutBalance->addWidget( spacerL );
        }
        
        QLabel* homeIcon = new ClickableLabel( m_pImpl );
        homeIcon->setText("Ġ");
        layoutBalance->addWidget( homeIcon );
        homeIcon->setObjectName( "home_button" );
        homeIcon->setCursor( Qt::PointingHandCursor );
        connect( homeIcon, SIGNAL( clicked() ), this, SLOT( gotoWebsite() ) );
        
        // Use spacer to push balance label to the right
        {
            QWidget* spacerMid = new QWidget();
            spacerMid->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
            layoutBalance->addWidget( spacerMid );
        }
        
        labelBalance = new ClickableLabel( m_pImpl );
        labelBalance->setObjectName( "gulden_label_balance" );
        labelBalance->setText( "" );
        layoutBalance->addWidget( labelBalance );
        
        labelBalanceForex = new ClickableLabel( m_pImpl );
        labelBalanceForex->setObjectName( "gulden_label_balance_forex" );
        labelBalanceForex->setText( "" );
        labelBalanceForex->setCursor( Qt::PointingHandCursor );
        connect( labelBalanceForex, SIGNAL( clicked() ), this, SLOT( showExchangeRateDialog() ) );
        labelBalanceForex->setVisible(false);
        layoutBalance->addWidget( labelBalanceForex );

        //Right margin
        {
            QWidget* spacerR = new QWidget();
            spacerR->setObjectName("gulden_bar_right_margin");
            spacerR->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
            layoutBalance->addWidget( spacerR );
        }
    
        balanceContainer->setMinimumWidth( sideBarWidth );
    }
    
    
    
    m_pImpl->addToolBar( guldenBar );




    //Add spacer bar
    spacerBarL = new QToolBar( QCoreApplication::translate( "toolbar", "Spacer  toolbar" ) );
    spacerBarL->setFixedHeight( horizontalBarHeight );
    spacerBarL->setObjectName( "spacer_bar" );
    spacerBarL->setMovable( false );
    //Spacer to fill width
    {
        QWidget* spacerR = new QWidget();
        spacerR->setMinimumWidth( 40 );
        spacerR->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        spacerBarL->addWidget( spacerR );
        spacerBarL->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    }
    m_pImpl->addToolBar( spacerBarL );



    tabsBar = m_pImpl->findChildren<QToolBar*>( "" )[0];
    //Add the main toolbar - middle (tabs)
    tabsBar->setFixedHeight( horizontalBarHeight );
    tabsBar->setObjectName( "navigation_bar" );
    tabsBar->setMovable( false );
    tabsBar->setToolButtonStyle( Qt::ToolButtonTextOnly );
    //Remove all the actions so we can add them again in a different order
    tabsBar->removeAction( m_pImpl->historyAction );
    tabsBar->removeAction( m_pImpl->overviewAction );
    tabsBar->removeAction( m_pImpl->sendCoinsAction );
    tabsBar->removeAction( m_pImpl->receiveCoinsAction );
    //Setup the tab toolbar
    tabsBar->addAction( m_pImpl->receiveCoinsAction );
    tabsBar->addAction( m_pImpl->sendCoinsAction );
    tabsBar->addAction( m_pImpl->historyAction );
    
    passwordAction = new QAction(m_pImpl->platformStyle->SingleColorIcon(":/icons/password"), tr("&Password"), this);
    passwordAction->setStatusTip(tr("Change wallet password"));
    passwordAction->setToolTip(passwordAction->statusTip());
    passwordAction->setCheckable(true);
    passwordAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabsBar->addAction(passwordAction);
    
    backupAction = new QAction(m_pImpl->platformStyle->SingleColorIcon(":/icons/backup"), tr("&Backup"), this);
    backupAction->setStatusTip(tr("Backup wallet"));
    backupAction->setToolTip(backupAction->statusTip());
    backupAction->setCheckable(true);
    backupAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    tabsBar->addAction(backupAction);
    
    connect(passwordAction, SIGNAL(triggered()), this, SLOT(gotoPasswordDialog()));
    connect(backupAction, SIGNAL(triggered()), this, SLOT(gotoBackupDialog()));
    
        
    m_pImpl->receiveCoinsAction->setChecked( true );
    
    tabsBar->widgetForAction( m_pImpl->historyAction )->setCursor( Qt::PointingHandCursor );
    tabsBar->widgetForAction( m_pImpl->sendCoinsAction )->setCursor( Qt::PointingHandCursor );
    tabsBar->widgetForAction( m_pImpl->receiveCoinsAction )->setCursor( Qt::PointingHandCursor );
    tabsBar->widgetForAction( passwordAction )->setCursor( Qt::PointingHandCursor );
    tabsBar->widgetForAction( backupAction )->setCursor( Qt::PointingHandCursor );
    tabsBar->widgetForAction( m_pImpl->receiveCoinsAction )->setObjectName( "receive_coins_button" );
    tabsBar->widgetForAction( m_pImpl->receiveCoinsAction )->setContentsMargins( 0, 0, 0, 0 );
    tabsBar->widgetForAction( m_pImpl->receiveCoinsAction )->setContentsMargins( 0, 0, 0, 0 );
    tabsBar->widgetForAction( m_pImpl->sendCoinsAction )->setContentsMargins( 0, 0, 0, 0 );
    tabsBar->widgetForAction( m_pImpl->historyAction )->setContentsMargins( 0, 0, 0, 0 );
    tabsBar->setContentsMargins( 0, 0, 0, 0 );
    
    //Spacer to fill width
    {
        QWidget* spacerR = new QWidget();
        spacerR->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        //Delibritely large amount - to push the next toolbar as far right as possible.
        spacerR->setMinimumWidth( 500000 );
        tabsBar->addWidget( spacerR );
        tabsBar->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    }
    //Only show the actions we want
    m_pImpl->receiveCoinsAction->setVisible( true );
    m_pImpl->sendCoinsAction->setVisible( true );
    m_pImpl->historyAction->setVisible( true );
    passwordAction->setVisible( false );
    backupAction->setVisible( false );
    m_pImpl->overviewAction->setVisible( false );
    tabsBar->setWindowTitle( QCoreApplication::translate( "toolbar", "Navigation toolbar" ) );
    m_pImpl->addToolBar( tabsBar );






    //Setup the account info bar
    accountInfoBar = new QToolBar( QCoreApplication::translate( "toolbar", "Account info toolbar" ) );
    accountInfoBar->setFixedHeight( horizontalBarHeight );
    accountInfoBar->setObjectName( "account_info_bar" );
    accountInfoBar->setMovable( false );
    accountInfoBar->setToolButtonStyle( Qt::ToolButtonIconOnly );
    accountSummaryWidget = new AccountSummaryWidget( ticker, m_pImpl );
    accountSummaryWidget->setObjectName( "settings_button" );
    accountSummaryWidget->setContentsMargins( 0, 0, 0, 0 );
    accountInfoBar->setContentsMargins( 0, 0, 0, 0 );
    accountSummaryWidget->setObjectName( "accountSummaryWidget" );
    accountInfoBar->addWidget( accountSummaryWidget );
    m_pImpl->addToolBar( accountInfoBar );
    connect(accountSummaryWidget, SIGNAL( requestAccountSettings() ), this, SLOT( showAccountSettings() ) );
    connect(accountSummaryWidget, SIGNAL( requestExchangeRateDialog() ), this, SLOT( showExchangeRateDialog() ) );
    



    //Add spacer bar
    spacerBarR = new QToolBar( QCoreApplication::translate( "toolbar", "Spacer  toolbar" ) );
    spacerBarR->setFixedHeight( horizontalBarHeight );
    spacerBarR->setObjectName( "spacer_bar" );
    spacerBarR->setMovable( false );
    //Spacer to fill width
    {
        QWidget* spacerR = new QWidget();
        spacerR->setMinimumWidth( 40 );
        spacerR->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        spacerBarR->addWidget( spacerR );
        spacerBarR->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    }
    m_pImpl->addToolBar( spacerBarR );
    
    //Hide all toolbars until UI fully loaded
    hideToolBars();
    
    
    //Init the welcome dialog inside walletFrame
    welcomeScreen = new WelcomeDialog(m_pImpl->platformStyle, m_pImpl);
    m_pImpl->walletFrame->walletStack->addWidget(welcomeScreen);
    m_pImpl->walletFrame->walletStack->setCurrentWidget(welcomeScreen);
    connect(welcomeScreen, SIGNAL( loadWallet() ), m_pImpl->walletFrame, SIGNAL( loadWallet() ));
}

void GuldenGUI::hideToolBars()
{
    if(accountBar) accountBar->setVisible(false);
    if(guldenBar) guldenBar->setVisible(false);
    if(spacerBarL) spacerBarL->setVisible(false);
    if(tabsBar) tabsBar->setVisible(false);
    if(spacerBarR) spacerBarR->setVisible(false);
    if(accountInfoBar) accountInfoBar->setVisible(false);
    if(statusBar) statusBar->setVisible(false);
}

void GuldenGUI::showToolBars()
{
    welcomeScreen = NULL;
    m_pImpl->appMenuBar->setStyleSheet("");
    
    if(accountBar) accountBar->setVisible(true);
    if(guldenBar) guldenBar->setVisible(true);
    if(spacerBarL) spacerBarL->setVisible(true);
    if(tabsBar) tabsBar->setVisible(true);
    if(spacerBarR) spacerBarR->setVisible(true);
    if(accountInfoBar) accountInfoBar->setVisible(true);
    if(statusBar) statusBar->setVisible(m_pImpl->progressBarLabel->isVisible());
}


void GuldenGUI::doApplyStyleSheet()
{
    //Load our own QSS stylesheet template for 'whole app'
    QFile styleFile( ":Gulden/qss" );
    styleFile.open( QFile::ReadOnly );

    //Use the same style on all platforms to simplify skinning
    GuldenProxyStyle* guldenStyle = new GuldenProxyStyle();
    QApplication::setStyle( guldenStyle );
    
    m_pImpl->installEventFilter(new GuldenEventFilter(m_pImpl->style(), m_pImpl, guldenStyle));

    //Replace variables in the 'template' with actual values
    QString style( styleFile.readAll() );
    style.replace( "ACCENT_COLOR_1", QString(ACCENT_COLOR_1) );
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
    m_pImpl->setStyleSheet( style );
}

void GuldenGUI::resizeToolBarsGulden()
{
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

void GuldenGUI::doPostInit()
{  
    //Fonts
    // We 'abuse' the translation system here to allow different 'font stacks' for different languages.
    //QString MAIN_FONTSTACK = QObject::tr("Arial, 'Helvetica Neue', Helvetica, sans-serif");

    m_pImpl->appMenuBar->setStyleSheet("QMenuBar{background-color: rgba(255, 255, 255, 0%);} QMenu{background-color: #f3f4f6; border: 1px solid #999; color: black;} QMenu::item { color: black; } QMenu::item:disabled {color: #999;} QMenu::separator{background-color: #999; height: 1px; margin-left: 10px; margin-right: 5px;}");

    
    

    {
        // Qt status bar sucks - it is impossible to style nicely, so we just rip the thing out and use a toolbar instead.
        m_pImpl->statusBar()->setVisible(false);
        
        //Allow us to target the progress label for easy styling
        //m_pImpl->statusBar()->removeWidget(m_pImpl->progressBarLabel);
        //m_pImpl->statusBar()->removeWidget(m_pImpl->progressBar);
        //m_pImpl->statusBar()->removeWidget(m_pImpl->frameBlocks);
        
        
          
        //Add a spacer to the frameBlocks so that we can force them to expand to same size as the progress text (Needed for proper centering of progress bar)
        /*QFrame* frameBlocksSpacerL = new QFrame(m_pImpl->frameBlocks);
        frameBlocksSpacerL->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        frameBlocksSpacerL->setContentsMargins( 0, 0, 0, 0);
        ((QHBoxLayout*)m_pImpl->frameBlocks->layout())->insertWidget(0, frameBlocksSpacerL);*/

            
        m_pImpl->frameBlocks->layout()->setContentsMargins( 0, 0, 0, 0 );
        m_pImpl->frameBlocks->layout()->setSpacing( 0 );
        
        //Hide some of the 'task items' we don't need
        m_pImpl->unitDisplayControl->setVisible( false );
        //m_pImpl->labelBlocksIcon->setVisible( false );
        
        
        //Status bar
        {
            statusBar = new QToolBar( QCoreApplication::translate( "toolbar", "Status toolbar" ) , m_pImpl);
            statusBar->setObjectName( "status_bar" );
            statusBar->setMovable( false );
            
            QFrame* statusBarStatusArea = new QFrame(statusBar);
            statusBarStatusArea->setObjectName("status_bar_status_area");
            statusBarStatusArea->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            statusBarStatusArea->setContentsMargins( 0, 0, 0, 0);
            QHBoxLayout* statusBarStatusAreaLayout = new QHBoxLayout();
            statusBarStatusArea->setLayout(statusBarStatusAreaLayout);
            statusBarStatusAreaLayout->setSpacing(0);
            statusBarStatusAreaLayout->setContentsMargins( 0, 0, 0, 0 );
            statusBar->addWidget(statusBarStatusArea);
            
            m_pImpl->progressBarLabel->setObjectName("progress_bar_label");
            statusBarStatusAreaLayout->addWidget(m_pImpl->progressBarLabel);
                       
            QFrame* statusProgressSpacerL = new QFrame(statusBar);
            statusProgressSpacerL->setObjectName("progress_bar_spacer_left");
            statusProgressSpacerL->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            statusProgressSpacerL->setContentsMargins( 0, 0, 0, 0);
            statusBar->addWidget(statusProgressSpacerL);
            
            
            QFrame* progressBarWrapper = new QFrame(statusBar);
            progressBarWrapper->setObjectName("progress_bar");
            progressBarWrapper->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Preferred);
            progressBarWrapper->setContentsMargins( 0, 0, 0, 0);
            QHBoxLayout* layoutProgressBarWrapper = new QHBoxLayout;
            progressBarWrapper->setLayout(layoutProgressBarWrapper);
            layoutProgressBarWrapper->addWidget(m_pImpl->progressBar);
            statusBar->addWidget(progressBarWrapper);
            m_pImpl->progressBar->setVisible(false);
            
            
            
            QFrame* statusProgressSpacerR = new QFrame(statusBar);
            statusProgressSpacerR->setObjectName("progress_bar_spacer_right");
            statusProgressSpacerR->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            statusProgressSpacerR->setContentsMargins( 0, 0, 0, 0);
            statusBar->addWidget(statusProgressSpacerR);
            
            m_pImpl->frameBlocks->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
            m_pImpl->frameBlocks->setObjectName("status_bar_frame_blocks");
            //Use spacer to push all the icons to the right
            QFrame* frameBlocksSpacerL = new QFrame(m_pImpl->frameBlocks);
            frameBlocksSpacerL->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            frameBlocksSpacerL->setContentsMargins( 0, 0, 0, 0);
            ((QHBoxLayout*)m_pImpl->frameBlocks->layout())->insertWidget(0, frameBlocksSpacerL, 1);
            statusBar->addWidget(m_pImpl->frameBlocks);
            //Right margin to match rest of UI
            QFrame* frameBlocksSpacerR = new QFrame(m_pImpl->frameBlocks);
            frameBlocksSpacerR->setObjectName("rightMargin");
            frameBlocksSpacerR->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
            frameBlocksSpacerR->setContentsMargins( 0, 0, 0, 0);
            ((QHBoxLayout*)m_pImpl->frameBlocks->layout())->addWidget(frameBlocksSpacerR);
            
            //Use our own styling - clear the styling that is already applied
            m_pImpl->progressBar->setStyleSheet("");
            //Hide text we don't want it as it looks cluttered.
            m_pImpl->progressBar->setTextVisible(false);
            
            
            m_pImpl->addToolBar( Qt::BottomToolBarArea, statusBar );
            
            statusBar->setVisible(false);
        }
    }

    m_pImpl->backupWalletAction->setIconText( QCoreApplication::translate( "toolbar", "Backup" ) );

    //Change shortcut keys because we have hidden overview pane and changed tab orders
    m_pImpl->overviewAction->setShortcut( QKeySequence( Qt::ALT + Qt::Key_0 ) );
    m_pImpl->sendCoinsAction->setShortcut( QKeySequence( Qt::ALT + Qt::Key_2 ) );
    m_pImpl->receiveCoinsAction->setShortcut( QKeySequence( Qt::ALT + Qt::Key_3 ) );
    m_pImpl->historyAction->setShortcut( QKeySequence( Qt::ALT + Qt::Key_1 ) );

    doApplyStyleSheet();
    
    //Force font antialiasing
    QFont f = QApplication::font();
    f.setStyleStrategy( QFont::PreferAntialias );
    QApplication::setFont( f );
    
    m_pImpl->openAction->setVisible(false);
    m_pImpl->signMessageAction->setVisible(false);
    m_pImpl->verifyMessageAction->setVisible(false);
    
    m_pImpl->setContextMenuPolicy(Qt::NoContextMenu);
    
    m_pImpl->setMinimumSize(860, 520);
    
    disconnect(m_pImpl->backupWalletAction, SIGNAL(triggered()), 0, 0);
    connect(m_pImpl->backupWalletAction, SIGNAL(triggered()), this, SLOT(gotoBackupDialog()));
    
    m_pImpl->encryptWalletAction->setCheckable(false);
    disconnect(m_pImpl->encryptWalletAction, SIGNAL(triggered()), 0, 0);
    connect(m_pImpl->encryptWalletAction, SIGNAL(triggered()), this, SLOT(gotoPasswordDialog()));
    
    disconnect(m_pImpl->changePassphraseAction, SIGNAL(triggered()), 0, 0);
    connect(m_pImpl->changePassphraseAction, SIGNAL(triggered()), this, SLOT(gotoPasswordDialog()));
}

void GuldenGUI::hideProgressBarLabel()
{
    m_pImpl->progressBarLabel->setText("");
    m_pImpl->progressBarLabel->setVisible(false);
    if(statusBar)
        statusBar->setVisible(false);
}

void GuldenGUI::showProgressBarLabel()
{
    m_pImpl->progressBarLabel->setVisible(true);
    if(statusBar)
        statusBar->setVisible(true);
}

bool GuldenGUI::welcomeScreenIsVisible()
{
    return welcomeScreen != NULL;
}

QDialog* GuldenGUI::createDialog(QWidget* parent, QString message, QString confirmLabel, QString cancelLabel, int minWidth, int minHeight)
{
    QDialog* d = new QDialog(parent);
    d->setWindowFlags(Qt::Dialog);
    d->setMinimumSize(QSize(minWidth, minHeight));
    QVBoxLayout* vbox = new QVBoxLayout();
    vbox->setSpacing(0);
    vbox->setContentsMargins( 0, 0, 0, 0 );
    
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
        buttonBox->button(QDialogButtonBox::Ok)->setText(confirmLabel);
        buttonBox->button(QDialogButtonBox::Ok)->setCursor(Qt::PointingHandCursor);
        buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(GULDEN_DIALOG_CONFIRM_BUTTON_STYLE);
        QObject::connect(buttonBox, SIGNAL(accepted()), d, SLOT(accept()));
    }
    
    if (!cancelLabel.isEmpty())
    {
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

    return beforeEllipsis + ELLIPSIS + afterEllipsis;
}

QString getAccountLabel(CAccount* account)
{
    QString accountName = QString::fromStdString( account->getLabel() );
    if ( account->IsMobi() )
    {
        accountName.append(QString::fromUtf8(" \uf10b"));
    }
    else if ( !account->IsHD() )
    {
        accountName.append(QString::fromUtf8(" \uf187"));
    }
    /*else
    {
        accountName.append(QString("%1").arg(((CAccountHD*)account)->getIndex()));
    }*/
    return limitString(accountName, 35);
}

void GuldenGUI::refreshAccountControls()
{
    LogPrintf("GuldenGUI::refreshAccountControls\n");
    
    for ( const auto& controlPair : m_accountMap )
    {
        controlPair.first->deleteLater();
    }
    m_accountMap.clear();
    if (pwalletMain)
    {
        // Sort the accounts.
        std::map<QString, ClickableLabel*> sortedAccounts;
        ClickableLabel* makeActive = NULL;
        {
            //NB! Mutex scope here is important to avoid deadlock inside setActiveAccountButton
            LOCK(pwalletMain->cs_wallet);
            
            for ( const auto& accountPair : pwalletMain->mapAccounts )
            {    
                if (accountPair.second->m_Type == AccountType::Normal || (fShowChildAccountsSeperately && accountPair.second->m_Type == AccountType::ShadowChild) )
                {
                    QString label = getAccountLabel(accountPair.second);
                    ClickableLabel* accLabel = createAccountButton( label );
                    m_accountMap[accLabel] = accountPair.second;
                    sortedAccounts[label] = accLabel;
                    
                    if (accountPair.second == m_pImpl->walletFrame->currentWalletView()->walletModel->getActiveAccount())
                        makeActive = accLabel;
                }
            }
        }
        for ( const auto& iter : sortedAccounts)
        {
            accountScrollArea->layout()->addWidget( iter.second );                
        }
        if (makeActive)
        {
            setActiveAccountButton( makeActive );
        }
    }
}

bool GuldenGUI::setCurrentWallet( const QString& name )
{
    LogPrintf("GuldenGUI::setCurrentWallet %s\n", name.toStdString());
    
    showToolBars();
    refreshAccountControls();
    
    connect( m_pImpl->walletFrame->currentWalletView()->walletModel, SIGNAL( balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount) ), accountSummaryWidget , SLOT( balanceChanged() ) );
    connect( m_pImpl->walletFrame->currentWalletView()->walletModel, SIGNAL( balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount) ), this , SLOT( balanceChanged() ) );
    connect( m_pImpl->walletFrame->currentWalletView()->walletModel, SIGNAL( accountListChanged() ), this , SLOT( accountListChanged() ) );
    connect( m_pImpl->walletFrame->currentWalletView()->walletModel, SIGNAL( activeAccountChanged(CAccount*) ), this , SLOT( activeAccountChanged(CAccount*) ) );
    connect( m_pImpl->walletFrame->currentWalletView()->walletModel, SIGNAL( accountDeleted(CAccount*) ), this , SLOT( accountDeleted(CAccount*) ) );
    connect( m_pImpl->walletFrame->currentWalletView()->walletModel, SIGNAL( accountAdded(CAccount*) ), this , SLOT( accountAdded(CAccount*) ) );
    
    return true;
}


ClickableLabel* GuldenGUI::createAccountButton( const QString& accountName )
{
    ClickableLabel* newAccountButton = new ClickableLabel( m_pImpl );
    newAccountButton->setText( accountName );
    newAccountButton->setCursor( Qt::PointingHandCursor );
    m_pImpl->connect( newAccountButton, SIGNAL( clicked() ), this, SLOT( accountButtonPressed() ) );
    return newAccountButton;
}

void GuldenGUI::setActiveAccountButton( ClickableLabel* activeButton )
{
    LogPrintf("GuldenGUI::setActiveAccountButton\n");
    
    for ( const auto & button : accountBar->findChildren<ClickableLabel*>( "" ) )
    {
        button->setChecked( false );
        button->setCursor( Qt::PointingHandCursor );
    }
    activeButton->setChecked( true );
    activeButton->setCursor( Qt::ArrowCursor );

     // Update the account    
    if ( m_pImpl->walletFrame->currentWalletView() )
    {
        if ( m_pImpl->walletFrame->currentWalletView()->receiveCoinsPage )
        {
            accountSummaryWidget->setActiveAccount( m_accountMap[activeButton] );
            m_pImpl->walletFrame->currentWalletView()->walletModel->setActiveAccount( m_accountMap[activeButton] );
            
            updateAccount(m_accountMap[activeButton]);
        }
    }
}

void GuldenGUI::updateAccount(CAccount* account)
{
    LOCK(pwalletMain->cs_wallet);
            
    if (receiveAddress)
        delete receiveAddress;
    
    receiveAddress = new CReserveKey(pwalletMain, account, KEYCHAIN_EXTERNAL);
    CPubKey pubKey;
    receiveAddress->GetReservedKey(pubKey);
    CKeyID keyID = pubKey.GetID();
    m_pImpl->walletFrame->currentWalletView()->receiveCoinsPage->updateAddress( QString::fromStdString(CBitcoinAddress(keyID).ToString()) );
    m_pImpl->walletFrame->currentWalletView()->receiveCoinsPage->setActiveAccount( account) ;
}

void GuldenGUI::balanceChanged()
{
    // Force receive Qr code to update on balance change.
    if (m_pImpl && m_pImpl->walletFrame && m_pImpl->walletFrame->currentWalletView() && m_pImpl->walletFrame->currentWalletView()->walletModel)
        updateAccount( m_pImpl->walletFrame->currentWalletView()->walletModel->getActiveAccount() );
}


void GuldenGUI::accountListChanged()
{    
    refreshAccountControls();    
}

void GuldenGUI::activeAccountChanged(CAccount* account)
{
    if (accountSummaryWidget)
        accountSummaryWidget->setActiveAccount(account);
    
    //Update account name 'in place' in account list
    bool haveAccount=false;
    if (pwalletMain)
    {
        for ( const auto& accountPair : m_accountMap )
        {
            if (accountPair.second == account)
            {
                haveAccount = true;
                if (!accountPair.first->isChecked())
                {
                    setActiveAccountButton(accountPair.first);
                }
                accountPair.first->setText( getAccountLabel(account) );
            }
        }
    }
    
    if(!haveAccount)
    {
        refreshAccountControls();
    }
}

//fixme: Gulden - lazy accountAdded/accountDeleted should instead update only the specific controls affected...
void GuldenGUI::accountAdded(CAccount* account)
{
    refreshAccountControls();
}

void GuldenGUI::accountDeleted(CAccount* account)
{
    refreshAccountControls();
}

void GuldenGUI::accountButtonPressed()
{
    QObject* sender = this->sender();
    ClickableLabel* accButton = qobject_cast<ClickableLabel*>( sender );
    restoreCachedWidgetIfNeeded();
    setActiveAccountButton( accButton );
}

void GuldenGUI::gotoWebsite()
{
    QDesktopServices::openUrl( QUrl( "http://www.Gulden.com/" ) );
}


void GuldenGUI::restoreCachedWidgetIfNeeded()
{
    m_pImpl->receiveCoinsAction->setVisible( true );
    m_pImpl->sendCoinsAction->setVisible( true );
    m_pImpl->historyAction->setVisible( true );
    passwordAction->setVisible( false );
    backupAction->setVisible( false );
    m_pImpl->overviewAction->setVisible( true );
    
    if (dialogPasswordModify)
    {
        m_pImpl->walletFrame->currentWalletView()->removeWidget( dialogPasswordModify );
        dialogPasswordModify->deleteLater();
        dialogPasswordModify = NULL;
    }
    if (dialogBackup)
    {
        m_pImpl->walletFrame->currentWalletView()->removeWidget( dialogBackup );
        dialogBackup->deleteLater();
        dialogBackup = NULL;
    }
    if (dialogNewAccount)
    {
        m_pImpl->walletFrame->currentWalletView()->removeWidget( dialogNewAccount );
        dialogNewAccount->deleteLater();
        dialogNewAccount = NULL;
    }
    if (dialogAccountSettings)
    {
        m_pImpl->walletFrame->currentWalletView()->removeWidget( dialogAccountSettings );
        dialogAccountSettings->deleteLater();
        dialogAccountSettings = NULL;
    }
    m_pImpl->walletFrame->currentWalletView()->setCurrentWidget( cacheCurrentWidget );
    cacheCurrentWidget = NULL;
}

void GuldenGUI::gotoNewAccountDialog()
{
    if ( m_pImpl->walletFrame )
    {
        restoreCachedWidgetIfNeeded();
            
        dialogNewAccount = new NewAccountDialog( m_pImpl->platformStyle, m_pImpl->walletFrame->currentWalletView(), m_pImpl->walletFrame->currentWalletView()->walletModel);
        connect( dialogNewAccount, SIGNAL( cancel() ), this, SLOT( cancelNewAccountDialog() ) );
        connect( dialogNewAccount, SIGNAL( accountAdded() ), this, SLOT( acceptNewAccount() ) );
        connect( dialogNewAccount, SIGNAL( addAccountMobile() ), this, SLOT( acceptNewAccountMobile() ) );
        cacheCurrentWidget = m_pImpl->walletFrame->currentWalletView()->currentWidget();
        m_pImpl->walletFrame->currentWalletView()->addWidget( dialogNewAccount );
        m_pImpl->walletFrame->currentWalletView()->setCurrentWidget( dialogNewAccount );
    }
}

void GuldenGUI::gotoPasswordDialog()
{
    if ( m_pImpl->walletFrame )
    {
        restoreCachedWidgetIfNeeded();
        
        m_pImpl->receiveCoinsAction->setVisible( false );
        m_pImpl->sendCoinsAction->setVisible( false );
        m_pImpl->historyAction->setVisible( false );
        passwordAction->setVisible( true );
        backupAction->setVisible( true );
        m_pImpl->overviewAction->setVisible( false );
        
        passwordAction->setChecked(true);
        backupAction->setChecked(false);
    
    
        dialogPasswordModify = new PasswordModifyDialog( m_pImpl->platformStyle, m_pImpl->walletFrame->currentWalletView() );
        connect( dialogPasswordModify, SIGNAL( dismiss() ), this, SLOT( dismissPasswordDialog() ) );
        cacheCurrentWidget = m_pImpl->walletFrame->currentWalletView()->currentWidget();
        m_pImpl->walletFrame->currentWalletView()->addWidget( dialogPasswordModify );
        m_pImpl->walletFrame->currentWalletView()->setCurrentWidget( dialogPasswordModify );
    }
}

void GuldenGUI::gotoBackupDialog()
{
    if ( m_pImpl->walletFrame )
    {
        restoreCachedWidgetIfNeeded();
        
        m_pImpl->receiveCoinsAction->setVisible( false );
        m_pImpl->sendCoinsAction->setVisible( false );
        m_pImpl->historyAction->setVisible( false );
        passwordAction->setVisible( true );
        backupAction->setVisible( true );
        m_pImpl->overviewAction->setVisible( false );
        
        passwordAction->setChecked(false);
        backupAction->setChecked(true);
    
        dialogBackup = new BackupDialog( m_pImpl->platformStyle, m_pImpl->walletFrame->currentWalletView(), m_pImpl->walletFrame->currentWalletView()->walletModel);
        connect( dialogBackup, SIGNAL( saveBackupFile() ), m_pImpl->walletFrame, SLOT( backupWallet() ) );
        connect( dialogBackup, SIGNAL( dismiss() ), this, SLOT( dismissBackupDialog() ) );
        cacheCurrentWidget = m_pImpl->walletFrame->currentWalletView()->currentWidget();
        m_pImpl->walletFrame->currentWalletView()->addWidget( dialogBackup );
        m_pImpl->walletFrame->currentWalletView()->setCurrentWidget( dialogBackup );
    }
}

void GuldenGUI::dismissBackupDialog()
{
    restoreCachedWidgetIfNeeded();
}

void GuldenGUI::dismissPasswordDialog()
{
    restoreCachedWidgetIfNeeded();
}



void GuldenGUI::cancelNewAccountDialog()
{
    restoreCachedWidgetIfNeeded();
}

void GuldenGUI::acceptNewAccount()
{ 
    if ( !dialogNewAccount->getAccountName().simplified().isEmpty() )
    {
        pwalletMain->GenerateNewAccount(dialogNewAccount->getAccountName().toStdString(), AccountType::Normal, AccountSubType::Desktop);
        restoreCachedWidgetIfNeeded();
    }
    else
    {
        //fixme: Mark invalid.
    }
}

void GuldenGUI::acceptNewAccountMobile()
{ 
    restoreCachedWidgetIfNeeded();
}

void GuldenGUI::showAccountSettings()
{
    LogPrintf("GuldenGUI::showAccountSettings\n");
    
    if ( m_pImpl->walletFrame )
    {
        restoreCachedWidgetIfNeeded();
        
        dialogAccountSettings = new AccountSettingsDialog( m_pImpl->platformStyle, m_pImpl->walletFrame->currentWalletView(), m_pImpl->walletFrame->currentWalletView()->walletModel->getActiveAccount(), m_pImpl->walletFrame->currentWalletView()->walletModel);
        connect( dialogAccountSettings, SIGNAL( dismissAccountSettings() ), this, SLOT( dismissAccountSettings() ) );
        connect( m_pImpl->walletFrame->currentWalletView()->walletModel, SIGNAL( activeAccountChanged(CAccount*) ), dialogAccountSettings, SLOT( activeAccountChanged(CAccount*) ) );
        cacheCurrentWidget = m_pImpl->walletFrame->currentWalletView()->currentWidget();
        m_pImpl->walletFrame->currentWalletView()->addWidget( dialogAccountSettings );
        m_pImpl->walletFrame->currentWalletView()->setCurrentWidget( dialogAccountSettings );
    }
}

void GuldenGUI::dismissAccountSettings()
{
    restoreCachedWidgetIfNeeded();
}

void GuldenGUI::showExchangeRateDialog()
{
    if (!dialogExchangeRate)
    {
        CurrencyTableModel* currencyTabelmodel = ticker->GetCurrencyTableModel();
        currencyTabelmodel->setBalance( m_pImpl->walletFrame->currentWalletView()->walletModel->getBalance() );
        connect( m_pImpl->walletFrame->currentWalletView()->walletModel, SIGNAL( balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount) ), currencyTabelmodel , SLOT( balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount) ) );
        dialogExchangeRate = new ExchangeRateDialog( m_pImpl->platformStyle, m_pImpl, currencyTabelmodel );
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
        {"BTC", "\uF15A"},
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
        {"NLG", "Ġ"}
    };

    if (currencyCodeSymbolMap.find(currencyCode) != currencyCodeSymbolMap.end())
    {
        return currencyCodeSymbolMap[currencyCode];
    }
    return "";
}

