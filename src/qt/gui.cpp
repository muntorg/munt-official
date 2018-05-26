// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2015-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include "gui.h"
#include <unity/appmanager.h>

#include "units.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "modaloverlay.h"
#include "networkstyle.h"
#include "notificator.h"
#include "openuridialog.h"
#include "optionsdialog.h"
#include "optionsmodel.h"
#include "rpcconsole.h"
#include "utilitydialog.h"
#include "checkpoints.h"
#include "chainparams.h"
#include "transactiontablemodel.h"
#include "transactionrecord.h"
#include "qt/_Gulden/exchangeratedialog.h"

#ifdef ENABLE_WALLET
#include "walletframe.h"
#include "walletmodel.h"
#include "qt/_Gulden/witnessdialog.h"
#endif // ENABLE_WALLET

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "chainparams.h"
#include "init.h"
#include "ui_interface.h"
#include "util.h"
#include "chain.h"

#include <iostream>

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
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

#include <QFontDatabase>

#include "walletview.h"
#include "sendcoinsdialog.h"

#include <Gulden/util.h>
#include <_Gulden/accountsummarywidget.h>
#include "_Gulden/receivecoinsdialog.h"

#if QT_VERSION < 0x050000
#include <QTextDocument>
#include <QUrl>
#else
#include <QUrlQuery>
#endif

const std::string GUI::DEFAULT_UIPLATFORM =
#if defined(Q_OS_MAC)
        "macosx"
#elif defined(Q_OS_WIN)
        "windows"
#else
        "other"
#endif
        ;

static void NotifyRequestUnlockS(GUI* parent, CWallet* wallet, std::string reason)
{
    QMetaObject::invokeMethod(parent, "NotifyRequestUnlock", Qt::QueuedConnection, Q_ARG(void*, wallet), Q_ARG(QString, QString::fromStdString(reason)));
}

static void NotifyRequestUnlockWithCallbackS(GUI* parent, CWallet* wallet, std::string reason, std::function<void (void)> callback)
{
    QMetaObject::invokeMethod(parent, "NotifyRequestUnlockWithCallback", Qt::QueuedConnection, Q_ARG(void*, wallet), Q_ARG(QString, QString::fromStdString(reason)), Q_ARG(std::function<void (void)>, callback));
}

/** Display name for default wallet name. Uses tilde to avoid name
 * collisions in the future with additional wallets */
const QString GUI::DEFAULT_WALLET = "~Default";

GUI::GUI(const QStyle *_platformStyle, const NetworkStyle *networkStyle, QWidget *parent)
: QMainWindow(parent)
, platformStyle(_platformStyle)
{
    // Delete ourselves on close, application catches this and uses it as a signal to exit.
    setAttribute(Qt::WA_DeleteOnClose, true);

    //fixme: (2.1) ex GuldenGUI code, integrate this better
    {
        ticker = new CurrencyTicker( this );
        nocksSettings = new NocksSettings( this );

        //Start the ticker polling off - after the initial call the ticker will schedule the subsequent ones internally.
        ticker->pollTicker();
        nocksSettings->pollSettings();

        connect( ticker, SIGNAL( exchangeRatesUpdated() ), this, SLOT( updateExchangeRates() ) );

        uiInterface.RequestUnlock.connect(boost::bind(NotifyRequestUnlockS, this, _1, _2));
        uiInterface.RequestUnlockWithCallback.connect(boost::bind(NotifyRequestUnlockWithCallbackS, this, _1, _2, _3));
    }


    #if QT_VERSION > 0x050100
    // Generate high-dpi pixmaps
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    #endif
    #if QT_VERSION >= 0x050600
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    #endif
    #ifdef Q_OS_MAC
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
    #endif

    QFontDatabase::addApplicationFont(":/Gulden/fontawesome_pro_regular");
    QFontDatabase::addApplicationFont(":/Gulden/fontawesome_pro_light");
    QFontDatabase::addApplicationFont(":/Gulden/fontawesome_pro_brands");
    QFontDatabase::addApplicationFont(":/Gulden/fontawesome_pro_solid");
    QFontDatabase::addApplicationFont(":/Gulden/guldensign");
    QFontDatabase::addApplicationFont(":/Gulden/lato_black");
    QFontDatabase::addApplicationFont(":/Gulden/lato_blackitalic");
    QFontDatabase::addApplicationFont(":/Gulden/lato_bold");
    QFontDatabase::addApplicationFont(":/Gulden/lato_bolditalic");
    QFontDatabase::addApplicationFont(":/Gulden/lato_italic");
    QFontDatabase::addApplicationFont(":/Gulden/lato_medium");
    QFontDatabase::addApplicationFont(":/Gulden/lato_mediumitalic");
    QFontDatabase::addApplicationFont(":/Gulden/lato_regular");


    GUIUtil::restoreWindowGeometry("nWindow", QSize(960, 620), this);

    QString windowTitle = tr(PACKAGE_NAME) + " - ";
#ifdef ENABLE_WALLET
    enableWallet = WalletModel::isWalletEnabled();
#endif // ENABLE_WALLET
    if(enableWallet)
    {
        windowTitle += tr("Wallet");
    } else {
        windowTitle += tr("Node");
    }
    enableFullUI = !GetBoolArg("-disableui", false);
    windowTitle += " " + networkStyle->getTitleAddText();
#ifndef Q_OS_MAC
    QApplication::setWindowIcon(networkStyle->getTrayAndWindowIcon());
    setWindowIcon(networkStyle->getTrayAndWindowIcon());
#else
    MacDockIconHandler::instance()->setIcon(networkStyle->getAppIcon());
#endif
    setWindowTitle(IsArgSet("-windowtitle")?QString::fromStdString(GetArg("-windowtitle", "")):windowTitle);

#if defined(Q_OS_MAC) && QT_VERSION < 0x050000
    // This property is not implemented in Qt 5. Setting it has no effect.
    // A replacement API (QtMacUnifiedToolBar) is available in QtMacExtras.
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    rpcConsole = new RPCConsole(_platformStyle, 0);
    helpMessageDialog = new HelpMessageDialog(this, false);
#ifdef ENABLE_WALLET
    if(enableWallet && enableFullUI)
    {
        /** Create wallet frame and make it the central widget */
        walletFrame = new WalletFrame(_platformStyle, this);
        setCentralWidget(walletFrame);
    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
        // Call back to initialize as otherwise we have no f unctionality.
        QMetaObject::invokeMethod(QApplication::instance(), "requestInitialize", Qt::QueuedConnection);
    }

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initialized
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create system tray icon and notification
    createTrayIcon(networkStyle);

    // Create status bar
    statusBar();

    // Disable size grip because it looks ugly and nobody needs it
    statusBar()->setSizeGripEnabled(false);

    // Status bar notification icons
    frameBlocks = new QFrame(this);
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    labelWalletEncryptionIcon = new QLabel(this);
    labelWalletHDStatusIcon = new QLabel(this);
    connectionsControl = new GUIUtil::ClickableLabel();
    labelBlocksIcon = new GUIUtil::ClickableLabel();
    if(enableWallet && enableFullUI)
    {
        //frameBlocksLayout->addStretch();
        //frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelWalletEncryptionIcon);
        frameBlocksLayout->addWidget(labelWalletHDStatusIcon);
    }
    //frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(connectionsControl);
    //frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    //frameBlocksLayout->addStretch();

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    hideProgressBarLabel();
    progressBarLabel->setVisible(false);
    progressBar = new GUIUtil::ProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(true);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = QApplication::style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar, 1);
    statusBar()->addPermanentWidget(frameBlocks);

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

    // Subscribe to notifications from core
    subscribeToCoreSignals();

    connect(connectionsControl, SIGNAL(clicked(QPoint)), this, SLOT(toggleNetworkActive()));

    doPostInit();
    modalOverlay = new ModalOverlay(this->centralWidget());
#ifdef ENABLE_WALLET
    if(enableWallet && enableFullUI) {
        connect(walletFrame, SIGNAL(requestedSyncWarningInfo()), this, SLOT(showModalOverlay()));
        connect(labelBlocksIcon, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
        connect(progressBar, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
    }
#endif
}

void GUI::disconnectNonEssentialSignals()
{
    disconnect( ticker, SIGNAL( exchangeRatesUpdated() ), this, SLOT( updateExchangeRates() ) );
    if (dialogExchangeRate)
        dialogExchangeRate->disconnectSlots();
    if (accountSummaryWidget)
        accountSummaryWidget->disconnectSlots();
}

GUI::~GUI()
{
    LogPrint(BCLog::QT, "GUI::~GUI\n");

    // Delete items that respond to timers
    delete ticker;
    delete nocksSettings;

    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    if (rpcConsole)
    {
        delete rpcConsole;
        rpcConsole = nullptr;
    }

    // Save window size and position for future loads.
    GUIUtil::saveWindowGeometry("nWindow", this);

    // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
    if(trayIcon) 
        trayIcon->hide();

#ifdef Q_OS_MAC
    delete appMenuBar;
    MacDockIconHandler::cleanup();
#endif
}

void GUI::createActions()
{
    LogPrint(BCLog::QT, "GUI::createActions\n");

    QActionGroup *tabGroup = new QActionGroup(this);
    tabGroup->setObjectName("gui_action_tab_group");

    witnessDialogAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf1fe), tr("&Overview"), this);
    witnessDialogAction->setObjectName("action_witness_dialog");
    witnessDialogAction->setStatusTip(tr("View statistics and information for witness account."));
    witnessDialogAction->setCheckable(true);
    tabGroup->addAction(witnessDialogAction);

    overviewAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf1fe), tr("&Overview"), this);
    overviewAction->setObjectName("action_overview");
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf2f5), tr("&Send"), this);
    sendCoinsAction->setObjectName("action_send_coins");
    sendCoinsAction->setStatusTip(tr("Send coins to a Gulden address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    sendCoinsMenuAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf2f5), tr("&Send"), this);
    sendCoinsMenuAction->setObjectName("action_send_coins_menu");
    sendCoinsMenuAction->setStatusTip(sendCoinsAction->statusTip());
    sendCoinsMenuAction->setToolTip(sendCoinsMenuAction->statusTip());

    receiveCoinsAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf2f6), tr("&Receive"), this);
    receiveCoinsAction->setObjectName("action_receive_coins");
    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and gulden: URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    receiveCoinsMenuAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf2f6), tr("&Receive"), this);
    receiveCoinsMenuAction->setObjectName("action_receive_menu");
    receiveCoinsMenuAction->setStatusTip(receiveCoinsAction->statusTip());
    receiveCoinsMenuAction->setToolTip(receiveCoinsMenuAction->statusTip());

    historyAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf362), tr("&Transactions"), this);
    historyAction->setObjectName("action_history");
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

#ifdef ENABLE_WALLET
    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(witnessDialogAction, SIGNAL(triggered()), this, SLOT(showWitnessDialog()));
    connect(witnessDialogAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(sendCoinsMenuAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsMenuAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(receiveCoinsMenuAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsMenuAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
#endif // ENABLE_WALLET

    toggleHideAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf070), tr("&Show / Hide"), this);
    toggleHideAction->setObjectName("action_toggle_hide");
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    importPrivateKeyAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf019), tr("&Import key"), this);
    importPrivateKeyAction->setObjectName("action_import_privkey");
    importPrivateKeyAction->setStatusTip(tr("Import a private key address"));
    importPrivateKeyAction->setCheckable(false);

    rescanAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf002), tr("&Rescan transactions"), this);
    rescanAction->setObjectName("action_rescan");
    rescanAction->setStatusTip(tr("Rescan the blockchain looking for any missing transactions"));
    rescanAction->setCheckable(false);

    openAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf35d), tr("Open &URI..."), this);
    openAction->setObjectName("action_open_uri");
    openAction->setStatusTip(tr("Open a gulden: URI or payment request"));

    backupWalletAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf0c7), tr("&Backup Wallet..."), this);
    backupWalletAction->setObjectName("action_backup_wallet");
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));

    usedSendingAddressesAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf2f5), tr("&Sending addresses..."), this);
    usedSendingAddressesAction->setObjectName("action_used_sending_addresses");
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));

    usedReceivingAddressesAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf2f6), tr("&Receiving addresses..."), this);
    usedReceivingAddressesAction->setObjectName("action_used_receiving_addresses");
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));

    quitAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf057), tr("E&xit"), this);
    quitAction->setObjectName("action_quit");
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);

    openRPCConsoleAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf390), tr("&Debug window"), this);
    openRPCConsoleAction->setObjectName("action_open_rpc_console");
    openRPCConsoleAction->setStatusTip(tr("Open debugging and diagnostic console"));
    openRPCConsoleAction->setEnabled(false); // initially disable the debug window menu item

    showHelpMessageAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf120), tr("&Command-line options"), this);
    showHelpMessageAction->setObjectName("action_show_help_message");
    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the %1 help message to get a list with possible Gulden command-line options").arg(tr(PACKAGE_NAME)));

    aboutAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf05a), tr("&About %1").arg(tr(PACKAGE_NAME)), this);
    aboutAction->setObjectName("action_about");
    aboutAction->setStatusTip(tr("Show information about %1").arg(tr(PACKAGE_NAME)));
    aboutAction->setMenuRole(QAction::AboutRole);

    aboutAction->setEnabled(false);
    aboutQtAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf05a), tr("About &Qt"), this);
    aboutQtAction->setObjectName("action_about_qt");
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);

    encryptWalletAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf30d), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setObjectName("action_encrypt_wallet");
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);

    changePassphraseAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf084), tr("&Change Passphrase..."), this);
    changePassphraseAction->setObjectName("action_change_passphrase");
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));

    optionsAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf085), tr("&Options..."), this);
    optionsAction->setObjectName("action_options");
    optionsAction->setStatusTip(tr("Modify configuration options for %1").arg(tr(PACKAGE_NAME)));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    optionsAction->setEnabled(false);

    currencyAction = new QAction(GUIUtil::getIconFromFontAwesomeRegularGlyph(0xf3d1), tr("&Select currency"), this);
    currencyAction->setObjectName("action_currency");
    currencyAction->setStatusTip(tr("Change the local currency that is used to display estimates"));
    currencyAction->setCheckable(false);

    //NB! It is important that we use a queued connection here, otherwise on WIN32 we end up (in some cases) with the app closing while menu handling code has not yet exited cleanly.
    connect(quitAction, SIGNAL(triggered()), this, SLOT(userWantsToQuit()), (Qt::ConnectionType)(Qt::QueuedConnection|Qt::UniqueConnection));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(currencyAction, SIGNAL(triggered()), this, SLOT(showExchangeRateDialog()));
    connect(importPrivateKeyAction, SIGNAL(triggered()), this, SLOT(promptImportPrivKey()));
    connect(rescanAction, SIGNAL(triggered()), this, SLOT(promptRescan()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(showHelpMessageAction, SIGNAL(triggered()), this, SLOT(showHelpMessageClicked()));
    connect(openRPCConsoleAction, SIGNAL(triggered()), this, SLOT(showDebugWindow()));

#ifdef ENABLE_WALLET
    if(walletFrame)
    {
        connect(encryptWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(encryptWallet(bool)));
        connect(backupWalletAction, SIGNAL(triggered()), walletFrame, SLOT(backupWallet()));
        connect(changePassphraseAction, SIGNAL(triggered()), walletFrame, SLOT(changePassphrase()));
        connect(usedSendingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedSendingAddresses()));
        connect(usedReceivingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedReceivingAddresses()));
        connect(openAction, SIGNAL(triggered()), this, SLOT(openClicked()));
    }
#endif // ENABLE_WALLET

    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C), this, SLOT(showDebugWindowActivateConsole()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D), this, SLOT(showDebugWindow()));
}

void GUI::createMenuBar()
{
    LogPrint(BCLog::QT, "GUI::createMenuBar\n");

#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif
    appMenuBar->setObjectName("menu_bar_app");

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    file->setObjectName("menu_file");
    if(walletFrame)
    {
        file->addAction(openAction);
        file->addAction(backupWalletAction);
        file->addSeparator();
        file->addAction(usedSendingAddressesAction);
        file->addAction(usedReceivingAddressesAction);
        file->addSeparator();
    }
    file->addAction(quitAction);

    QMenu* settingsMenu = appMenuBar->addMenu(tr("&Settings"));
    settingsMenu->setObjectName("menu_settings");
    if(walletFrame)
    {
        settingsMenu->addAction(encryptWalletAction);
        settingsMenu->addAction(changePassphraseAction);
        settingsMenu->addSeparator();
    }
    settingsMenu->addAction(optionsAction);
    settingsMenu->addAction(currencyAction);

    QMenu* toolsMenu = appMenuBar->addMenu(tr("&Tools"));
    toolsMenu->setObjectName("menu_tools");
    toolsMenu->addAction(importPrivateKeyAction);
    toolsMenu->addAction(rescanAction);

    QMenu* help = appMenuBar->addMenu(tr("&Help"));
    help->setObjectName("menu_help");
    if(walletFrame)
    {
        help->addAction(openRPCConsoleAction);
    }
    help->addAction(showHelpMessageAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

void GUI::setClientModel(ClientModel *_clientModel)
{
    LogPrint(BCLog::QT, "GUI::setClientModel\n");

    this->clientModel = _clientModel;
    if(_clientModel)
    {
        optionsAction->setEnabled(true);

        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        updateNetworkState();
        connect(_clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
        connect(_clientModel, SIGNAL(networkActiveChanged(bool)), this, SLOT(setNetworkActive(bool)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));

        modalOverlay->setKnownBestHeight(_clientModel->getHeaderTipHeight(), QDateTime::fromTime_t(_clientModel->getHeaderTipTime()));
        setNumBlocks(_clientModel->getNumBlocks(), _clientModel->getLastBlockDate(), _clientModel->getVerificationProgress(NULL), false);
        connect(_clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(setNumBlocks(int,QDateTime,double,bool)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
        connect(_clientModel, SIGNAL(headerProgressChanged(int, int)), this, SLOT(setNumHeaders(int,int)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));

        // Receive and report messages from client model
        connect(_clientModel, SIGNAL(message(QString,QString,unsigned int)), this, SLOT(message(QString,QString,unsigned int)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));

        // Show progress dialog
        connect(_clientModel, SIGNAL(showProgress(QString,int)), this, SLOT(showProgress(QString,int)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));

        rpcConsole->setClientModel(_clientModel);
#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->setClientModel(_clientModel);
        }
#endif // ENABLE_WALLET
        OptionsModel* optionsModel = _clientModel->getOptionsModel();
        if(optionsModel)
        {
            // be aware of the tray icon disable state change reported by the OptionsModel object.
            connect(optionsModel,SIGNAL(hideTrayIconChanged(bool)),this,SLOT(setTrayIconVisible(bool)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));

            // initialize the disable state of the tray icon with the current value in the model.
            setTrayIconVisible(optionsModel->getHideTrayIcon());

            setOptionsModel(optionsModel);
        }
    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if(trayIconMenu)
        {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
        // Propagate cleared model to child objects
        rpcConsole->setClientModel(nullptr);
#ifdef ENABLE_WALLET
        if (walletFrame)
        {
            walletFrame->setClientModel(nullptr);
        }
#endif // ENABLE_WALLET
    }
}

#ifdef ENABLE_WALLET
bool GUI::addWallet(const QString& name, WalletModel *walletModel)
{
    LogPrint(BCLog::QT, "GUI::addWallet\n");

    if(!walletFrame)
        return false;
    setWalletActionsEnabled(true);

    connect(walletModel, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
    connect(walletModel, SIGNAL(showProgress(QString,int)), this, SLOT(showProgress(QString,int)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));

    //fixme: (2.0) This can be removed
    // Force this to run once to ensure correct PoW2 phase displays
    clientModel->updatePoW2Display();
    rpcConsole->updatePoW2PhaseState();

    return walletFrame->addWallet(name, walletModel);
}

bool GUI::setCurrentWallet(const QString& name)
{
    LogPrint(BCLog::QT, "GUI::setCurrentWallet\n");

    if(!walletFrame)
        return false;
    bool ret =  walletFrame->setCurrentWallet(name);

    // Now that we have an active wallet it is safe to show the toolbars and menubars again.
    showToolBars();
    appMenuBar->setVisible(true);

    refreshAccountControls();

    connect( walletFrame->currentWalletView()->walletModel, SIGNAL( balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount) ), accountSummaryWidget , SLOT( balanceChanged() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    connect( walletFrame->currentWalletView()->walletModel, SIGNAL( balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount) ), this , SLOT( balanceChanged() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    connect( walletFrame->currentWalletView()->walletModel, SIGNAL( accountNameChanged(CAccount*) ), this , SLOT( accountNameChanged(CAccount*) ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    connect( walletFrame->currentWalletView()->walletModel, SIGNAL( accountWarningChanged(CAccount*) ), this , SLOT( accountWarningChanged(CAccount*) ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    connect( walletFrame->currentWalletView()->walletModel, SIGNAL( activeAccountChanged(CAccount*) ), this , SLOT( activeAccountChanged(CAccount*) ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    connect( walletFrame->currentWalletView()->walletModel, SIGNAL( accountDeleted(CAccount*) ), this , SLOT( accountDeleted(CAccount*) ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    connect( walletFrame->currentWalletView()->walletModel, SIGNAL( accountAdded(CAccount*) ), this , SLOT( accountAdded(CAccount*) ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    connect( walletFrame->currentWalletView()->witnessDialogPage, SIGNAL(requestEmptyWitness()), this, SLOT(requestEmptyWitness()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    connect( walletFrame->currentWalletView()->witnessDialogPage, SIGNAL(requestFundWitness(CAccount*)), this, SLOT(requestFundWitness(CAccount*)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
    connect( walletFrame->currentWalletView()->witnessDialogPage, SIGNAL(requestRenewWitness(CAccount*)), this, SLOT(requestRenewWitness(CAccount*)) );
    connect( walletFrame->currentWalletView()->sendCoinsPage, SIGNAL(notifyPaymentAccepted()), this, SLOT(handlePaymentAccepted()) );

    return ret;
}

void GUI::removeAllWallets()
{
    LogPrint(BCLog::QT, "GUI::removeAllWallets\n");

    if(!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}
#endif // ENABLE_WALLET

void GUI::setWalletActionsEnabled(bool enabled)
{
    LogPrint(BCLog::QT, "GUI::setWalletActionsEnabled\n");

    witnessDialogAction->setEnabled(enabled);
    overviewAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    sendCoinsMenuAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    receiveCoinsMenuAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);
    encryptWalletAction->setEnabled(enabled);
    backupWalletAction->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
    usedSendingAddressesAction->setEnabled(enabled);
    usedReceivingAddressesAction->setEnabled(enabled);
    openAction->setEnabled(enabled);
}

void GUI::createTrayIcon(const NetworkStyle *networkStyle)
{
    LogPrint(BCLog::QT, "GUI::createTrayIcon\n");

#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("%1 client").arg(tr(PACKAGE_NAME)) + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getTrayAndWindowIcon());
    trayIcon->hide();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

void GUI::createTrayIconMenu()
{
    LogPrint(BCLog::QT, "GUI::createTrayIconMenu\n");

#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIconMenu->setObjectName("menu_tray_icon");
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsMenuAction);
    trayIconMenu->addAction(receiveCoinsMenuAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void GUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    LogPrint(BCLog::QT, "GUI::trayIconActivated\n");

    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#endif

void GUI::optionsClicked()
{
    LogPrint(BCLog::QT, "GUI::optionsClicked\n");

    if(!clientModel || !clientModel->getOptionsModel())
        return;

    OptionsDialog dlg(this, enableWallet);
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void GUI::aboutClicked()
{
    LogPrint(BCLog::QT, "GUI::aboutClicked\n");

    if(!clientModel)
        return;

    HelpMessageDialog dlg(this, true);
    dlg.exec();
}

void GUI::showDebugWindow()
{
    LogPrint(BCLog::QT, "GUI::showDebugWindow\n");

    rpcConsole->showNormal();
    rpcConsole->show();
    rpcConsole->raise();
    rpcConsole->activateWindow();
}

void GUI::showDebugWindowActivateConsole()
{
    LogPrint(BCLog::QT, "GUI::showDebugWindowActivateConsole\n");

    rpcConsole->setTabFocus(RPCConsole::TAB_CONSOLE);
    showDebugWindow();
}

void GUI::showHelpMessageClicked()
{
    LogPrint(BCLog::QT, "GUI::showHelpMessageClicked\n");

    helpMessageDialog->show();
}

#ifdef ENABLE_WALLET
void GUI::openClicked()
{
    LogPrint(BCLog::QT, "GUI::openClicked\n");

    OpenURIDialog dlg(this);
    if(dlg.exec())
    {
        Q_EMIT receivedURI(dlg.getURI());
    }
}

void GUI::showWitnessDialog()
{
    LogPrint(BCLog::QT, "GUI::showWitnessDialog\n");

    witnessDialogAction->setChecked(true);
    if (walletFrame)
    {
        walletFrame->currentWalletView()->witnessDialogPage->update();
        walletFrame->currentWalletView()->setCurrentWidget(walletFrame->currentWalletView()->witnessDialogPage);
    }
}

void GUI::gotoOverviewPage()
{
    LogPrint(BCLog::QT, "GUI::gotoOverviewPage\n");

    overviewAction->setChecked(true);
    if (walletFrame) walletFrame->gotoOverviewPage();
}

void GUI::gotoHistoryPage()
{
    LogPrint(BCLog::QT, "GUI::gotoHistoryPage\n");

    historyAction->setChecked(true);
    if (walletFrame) walletFrame->gotoHistoryPage();
    //if (walletFrame) walletFrame->currentWalletView()->historyPage->update();
}

void GUI::gotoReceiveCoinsPage()
{
    LogPrint(BCLog::QT, "GUI::gotoReceiveCoinsPage\n");

    receiveCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
    //if (walletFrame) walletFrame->currentWalletView()->receiveCoinsPage->update();
}

void GUI::gotoSendCoinsPage(QString addr)
{
    LogPrint(BCLog::QT, "GUI::gotoSendCoinsPage\n");

    sendCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
    if (walletFrame) walletFrame->currentWalletView()->sendCoinsPage->update();
}
#endif // ENABLE_WALLET

void GUI::updateNetworkState()
{
    LogPrint(BCLog::QT, "GUI::updateNetworkState\n");

    int count = clientModel->getNumConnections();
    float devicePixelRatio = 1.0;
    #if QT_VERSION > 0x050100
    devicePixelRatio = ((QGuiApplication*)QCoreApplication::instance())->devicePixelRatio();
    #endif

    QString icon;
    switch(count)
    {
    case 0: icon = devicePixelRatio<1.1?":/icons/connect_0":":/icons/connect_0_x2"; break;
    case 1: case 2: case 3: icon = devicePixelRatio<1.1?":/icons/connect_1":":/icons/connect_1_x2"; break;
    case 4: case 5: case 6: icon = devicePixelRatio<1.1?":/icons/connect_2":":/icons/connect_2_x2"; break;
    case 7: case 8: case 9: icon = devicePixelRatio<1.1?":/icons/connect_3":":/icons/connect_3_x2"; break;
    default: icon = devicePixelRatio<1.1?":/icons/connect_4":":/icons/connect_4_x2"; break;
    }

    QString tooltip;

    if (clientModel->getNetworkActive()) {
        tooltip = tr("%n active connection(s) to Gulden network", "", count) + QString(".<br>") + tr("Click to disable network activity.");
    } else {
        tooltip = tr("Network activity disabled.") + QString("<br>") + tr("Click to enable network activity again.");
        icon = ":/icons/network_disabled";
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");
    connectionsControl->setToolTip(tooltip);

    connectionsControl->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
}

void GUI::setNumConnections(int count)
{
    LogPrint(BCLog::QT, "GUI::setNumConnections\n");

    updateNetworkState();
}

void GUI::setNetworkActive(bool networkActive)
{
    LogPrint(BCLog::QT, "GUI::setNetworkActive\n");

    updateNetworkState();
}

void GUI::updateHeadersSyncProgressLabel(int current, int total)
{
    LogPrint(BCLog::QT, "GUI::updateHeadersSyncProgressLabel\n");

    if (total - current > HEADER_HEIGHT_DELTA_SYNC)
        progressBarLabel->setText(tr("Syncing Headers (%1%)...").arg(QString::number(100.0 * current / total, 'f', 1)));
}

void GUI::setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool header)
{
    LogPrint(BCLog::QT, "GUI::setNumBlocks\n");

    if (!clientModel)
        return;

    double nSyncProgress = std::min(1.0, (double)count / clientModel->getProbableHeight());

    if (modalOverlay)
    {
        if (header)
            modalOverlay->setKnownBestHeight(count, blockDate);
        else
            modalOverlay->tipUpdate(count, blockDate, nSyncProgress);
    }

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
        case BLOCK_SOURCE_NETWORK:
            // set label only if header download is (almost) done
            if (clientModel->getProbableHeight() - clientModel->getHeaderTipHeight() < HEADER_HEIGHT_DELTA_SYNC)
                progressBarLabel->setText(tr("Synchronizing with network..."));
            break;
        case BLOCK_SOURCE_DISK:
            if (header) {
                progressBarLabel->setText(tr("Indexing blocks on disk..."));
            } else {
                progressBarLabel->setText(tr("Processing blocks on disk..."));
            }
            break;
        case BLOCK_SOURCE_REINDEX:
            progressBarLabel->setText(tr("Reindexing blocks on disk..."));
            break;
        case BLOCK_SOURCE_NONE:
            if (header) {
                return;
            }
            progressBarLabel->setText(tr("Connecting to peers..."));
            break;
    }

    QString tooltip;

    QDateTime currentDate = QDateTime::currentDateTime();
    qint64 secs = blockDate.secsTo(currentDate);

    tooltip += tr("Processed %n block(s) of transaction history.", "", count);

    if (IsInitialBlockDownload())
    {
        hideBalances();
    }
    else
    {
        showBalances();
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(false);
            modalOverlay->showHide(true, true);
        }
#endif // ENABLE_WALLET

        hideProgressBarLabel();
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);
    }
    else
    {
        QString timeBehindText = GUIUtil::formatNiceTimeOffset(secs);

        showProgressBarLabel();
        progressBarLabel->setVisible(true);
        progressBar->setFormat(tr("%1 behind").arg(timeBehindText));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(nSyncProgress * 1000000000.0 + 0.5);
        progressBar->setVisible(true);

        tooltip = tr("Catching up... %1% complete.<br>").arg( std::min(99, qRound(nSyncProgress * 100)) ) + tooltip;
        if(count != prevBlocks)
        {
            labelBlocksIcon->setPixmap(QIcon(QString(
                ":/movies/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0')))
                .pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
        }
        prevBlocks = count;

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(true);
            modalOverlay->showHide();
        }
#endif // ENABLE_WALLET

        tooltip += QString("<br>");
        if (count>1)
        {
            //tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
            tooltip += QString("<br>");
        }
        tooltip += tr("Transactions and balances will not be accurate or correct until synchronisation is complete.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void GUI::setNumHeaders(int current, int total)
{
    LogPrint(BCLog::QT, "GUI::setNumHeaders\n");

    if (!clientModel)
        return;

    if (modalOverlay)
        modalOverlay->setKnownBestHeight(clientModel->getHeaderTipHeight(), QDateTime::fromTime_t(clientModel->getHeaderTipTime()));

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbled text)
    statusBar()->clearMessage();

    updateHeadersSyncProgressLabel(current, total);
}

void GUI::message(const QString &title, const QString &message, unsigned int style, bool *ret)
{
    LogPrint(BCLog::QT, "GUI::message\n");

    QString strTitle = title; // default title
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    QString msgType;

    // Prefer supplied title over style based title
    if (!title.isEmpty()) {
        msgType = title;
    }
    else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            break;
        default:
            break;
        }
    }

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    }
    else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        showNormalIfMinimized();
        QMessageBox mBox((QMessageBox::Icon)nMBoxIcon, strTitle, message, buttons, this);
        int r = mBox.exec();
        if (ret != NULL)
            *ret = r == QMessageBox::Ok;
    }
    else
        notificator->notify((Notificator::Class)nNotifyIcon, strTitle, message);
}

void GUI::resizeEvent(QResizeEvent* event)
{
    LogPrint(BCLog::QT, "GUI::resizeEvent\n");

    QMainWindow::resizeEvent(event);

    // If we are working with limited horizontal spacing then hide some non-essential UI elements to help things fit more comfortably.
    bool restrictedHorizontalSpace = (event->size().width() < 940) ? true : false;
    if (accountSummaryWidget)
        accountSummaryWidget->showForexBalance(!restrictedHorizontalSpace);
    if (walletFrame && walletFrame->currentWalletView() && walletFrame->currentWalletView()->receiveCoinsPage)
        walletFrame->currentWalletView()->receiveCoinsPage->setShowCopyQRAsImageButton(!restrictedHorizontalSpace);
    else
        ReceiveCoinsDialog::showCopyQRAsImagebutton = !restrictedHorizontalSpace;
}

void GUI::changeEvent(QEvent *e)
{
    LogPrint(BCLog::QT, "GUI::changeEvent\n");

    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void GUI::userWantsToQuit()
{
    // close rpcConsole in case it was open to make some space for the shutdown window
    if (rpcConsole)
        rpcConsole->close();
    GuldenAppManager::gApp->shutdown();
}

//NB! This is a bit subtle/tricky, but we want to ignore this close event even when we are closing.
//The core needs to clean up various things before the UI can safely close, so we signal to the core that we are closing and then let the core signal to us when we should actually do so.
//In the meantime we hide the window for immediate user feedback.
void GUI::closeEvent(QCloseEvent *event)
{
    LogPrint(BCLog::QT, "GUI::closeEvent\n");

    // We are really in the exit phase now, so this time really exit.
    if (coreAppIsReadyForUIToQuit)
    {
        QMainWindow::closeEvent(event);
        return;
    }

#ifdef Q_OS_MAC // Ignored on Mac
    QMainWindow::closeEvent(event);
    return;
#else
    QMainWindow::hide();
    event->ignore();
    if(clientModel && clientModel->getOptionsModel())
    {
        if(clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            QMainWindow::showMinimized();
            return;
        }
    }
    //Initiate the exit process
    userWantsToQuit();
#endif
}

void GUI::showEvent(QShowEvent *event)
{
    LogPrint(BCLog::QT, "GUI::showEvent\n");

    // enable the debug window when the main window shows up
    openRPCConsoleAction->setEnabled(true);
    aboutAction->setEnabled(true);
    //optionsAction->setEnabled(true);
}

#ifdef ENABLE_WALLET
void GUI::incomingTransaction(const QString& date, int unit, const CAmount& amountReceived, const CAmount& amountSent, const QString& type, const QString& address, const QString& account, const QString& label)
{
    LogPrint(BCLog::QT, "GUI::incomingTransaction\n");

    // On new transaction, make an info balloon
    QString msg = tr("Date: %1\n").arg(date) +
                  tr("Received: %1\n").arg(GuldenUnits::formatWithUnit(unit, amountReceived, true)) +
                  tr("Sent: %1\n").arg(GuldenUnits::formatWithUnit(unit, amountSent, true)) +
                  tr("Type: %1\n").arg(type);
    if (!label.isEmpty())
        msg += tr("Label: %1\n").arg(label);
    else if (account.isEmpty() && !address.isEmpty())
        msg += tr("Address: %1\n").arg(address);

    QString accountStr = "";
    if (!account.isEmpty())
        accountStr = " [" + account + "]";

    //fixme: (Post-2.1) - prevent spurious blank messages from showing - track down why these happen though.
    if (amountSent == CAmount(0) && amountReceived == CAmount(0) && type.isEmpty())
    {
        return;
    }

    //testnet prevent spamming of mined messages
    if (IsArgSet("-testnet") && type == "Mined")
    {
        return;
    }

    message(amountSent>amountReceived ? tr("Sent funds%1").arg(accountStr) : tr("Incoming funds%1").arg(accountStr),
             msg, CClientUIInterface::MSG_INFORMATION);
}
#endif // ENABLE_WALLET

void GUI::dragEnterEvent(QDragEnterEvent *event)
{
    LogPrint(BCLog::QT, "GUI::dragEnterEvent\n");

    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void GUI::dropEvent(QDropEvent *event)
{
    LogPrint(BCLog::QT, "GUI::dropEvent\n");

    if(event->mimeData()->hasUrls())
    {
        for(const QUrl &uri : event->mimeData()->urls())
        {
            Q_EMIT receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool GUI::eventFilter(QObject *object, QEvent *event)
{
    // Catch status tip events
    if (event->type() == QEvent::StatusTip)
    {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if (progressBarLabel->isVisible() || progressBar->isVisible())
            return true;
    }
    return QMainWindow::eventFilter(object, event);
}

#ifdef ENABLE_WALLET
bool GUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    LogPrint(BCLog::QT, "GUI::handlePaymentRequest\n");

    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    return false;
}

void GUI::setEncryptionStatus(int status)
{
    LogPrint(BCLog::QT, "GUI::setEncryptionStatus\n");

    float devicePixelRatio = 1.0;
    #if QT_VERSION > 0x050100
    devicePixelRatio = ((QGuiApplication*)QCoreApplication::instance())->devicePixelRatio();
    #endif

    switch(status)
    {
    case WalletModel::Unencrypted:
        labelWalletEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelWalletEncryptionIcon->hide();
        labelWalletEncryptionIcon->setPixmap(QIcon(devicePixelRatio<1.1?":/Gulden/lock":":/Gulden/lock_x2").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelWalletEncryptionIcon->show();
        labelWalletEncryptionIcon->setPixmap(QIcon(devicePixelRatio<1.1?":/Gulden/lock":":/Gulden/lock_x2").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}
#endif // ENABLE_WALLET

void GUI::showNormalIfMinimized(bool fToggleHidden)
{
    LogPrint(BCLog::QT, "GUI::showNormalIfMinimized\n");

    if(!clientModel)
        return;

    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

void GUI::toggleHidden()
{
    LogPrint(BCLog::QT, "GUI::toggleHidden\n");

    showNormalIfMinimized(true);
}

void GUI::showProgress(const QString &title, int nProgress)
{
    LogPrint(BCLog::QT, "GUI::showProgress\n");

    if (nProgress == 100)
    {
        if (progressBarLabel->text() == title)
        {
            progressBarLabel->setVisible(false);
            progressBar->setVisible(false);
            hideProgressBarLabel();
        }
    }
    else
    {
        if (!progressBar->isVisible() || progressBarLabel->text() == title)
        {
            progressBarLabel->setText(title);
            progressBar->setMaximum(1000000000);
            progressBar->setValue(nProgress * 10000000.0 + 0.5);
            progressBarLabel->setVisible(true);
            progressBar->setVisible(true);
            showProgressBarLabel();

            QString tooltip = title + "<br>" + QString("%1% complete.").arg(nProgress);
            progressBarLabel->setToolTip(tooltip);
            progressBar->setToolTip(tooltip);
        }
    }
}

void GUI::setTrayIconVisible(bool fHideTrayIcon)
{
    LogPrint(BCLog::QT, "GUI::setTrayIconVisible\n");

    if (trayIcon)
    {
        trayIcon->setVisible(!fHideTrayIcon);
    }
}

void GUI::showModalOverlay()
{
    LogPrint(BCLog::QT, "GUI::showModalOverlay\n");

    if (modalOverlay && (progressBar->isVisible() || modalOverlay->isLayerVisible()))
        modalOverlay->toggleVisibility();
}

static bool ThreadSafeMessageBox(GUI *gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
                               modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                               Q_ARG(QString, QString::fromStdString(caption)),
                               Q_ARG(QString, QString::fromStdString(message)),
                               Q_ARG(unsigned int, style),
                               Q_ARG(bool*, &ret));
    return ret;
}

void GUI::subscribeToCoreSignals()
{
    LogPrint(BCLog::QT, "GUI::subscribeToCoreSignals\n");

    // Connect signals to client
    uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
    uiInterface.ThreadSafeQuestion.connect(boost::bind(ThreadSafeMessageBox, this, _1, _3, _4));
}

void GUI::unsubscribeFromCoreSignals()
{
    LogPrint(BCLog::QT, "GUI::unsubscribeFromCoreSignals\n");

    // Disconnect signals from client
    uiInterface.ThreadSafeMessageBox.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
    uiInterface.ThreadSafeQuestion.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _3, _4));
}

void GUI::toggleNetworkActive()
{
    LogPrint(BCLog::QT, "GUI::toggleNetworkActive\n");

    if (clientModel) {
        clientModel->setNetworkActive(!clientModel->getNetworkActive());
    }
}
