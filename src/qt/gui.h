// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_GUI_H
#define GULDEN_QT_GUI_H

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include "amount.h"

#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QPoint>
#include <QSystemTrayIcon>

#include "_Gulden/GuldenGUI.h"
#include "validation.h"

#include <boost/uuid/uuid.hpp>

class ClientModel;
class NetworkStyle;
class Notificator;
class OptionsModel;
class QStyle;
class RPCConsole;
class SendCoinsRecipient;
class WalletFrame;
class WalletModel;
class HelpMessageDialog;
class ModalOverlay;
class AccountSummaryWidget;
class QLabel;
class QMainWindow;
class QMenu;
class QPoint;
class QSystemTrayIcon;

class CWallet;

QT_BEGIN_NAMESPACE
class QAction;
class QProgressBar;
class QProgressDialog;
QT_END_NAMESPACE

//Needed for QVariant.
Q_DECLARE_METATYPE(boost::uuids::uuid);

/**
  Gulden GUI main class. This class represents the main window of the Gulden UI. It communicates with both the client and
  wallet models to give the user an up-to-date view of the current core state.
*/
class GUI : public QMainWindow
{
    Q_OBJECT

public:
    static const QString DEFAULT_WALLET;
    static const std::string DEFAULT_UIPLATFORM;

    explicit GUI(const QStyle *platformStyle, const NetworkStyle *networkStyle, QWidget *parent = 0);
    ~GUI();

    /** Set the client model.
        The client model represents the part of the core that communicates with the P2P network, and is wallet-agnostic.
    */
    void setClientModel(ClientModel *clientModel);
#ifdef ENABLE_WALLET
    /** Set the wallet model.
        The wallet model represents a Gulden wallet, and offers access to the list of transactions, address book and sending
        functionality.
    */
    bool addWallet(const QString& name, WalletModel *walletModel);
    bool setCurrentWallet(const QString& name);
    void removeAllWallets();
    void updateUIForBlockTipChange();
#endif // ENABLE_WALLET
    bool enableWallet = false;
    // If this is false then we show the RPC console instead of the full UI. (like -disablewallet except with a wallet)
    bool enableFullUI = true;

    //fixme: (2.1) - The below are all ex GuldenGUI members that should be refactored back into the codebase to clean things up.
    void hideToolBars();
    void showToolBars();
    void hideBalances();
    void showBalances();
    void hideProgressBarLabel();
    void showProgressBarLabel();
    void doPostInit();
    void doApplyStyleSheet();
    void resizeToolBarsGulden();
    void refreshTabVisibilities();
    void refreshAccountControls();
    void setOptionsModel(OptionsModel* optionsModel);
    bool welcomeScreenIsVisible();
    static QDialog* createDialog(QWidget* parent, QString message, QString confirmLabel, QString cancelLabel, int minWidth, int minHeight);
    ClickableLabel* createAccountButton(const QString& accountName);
    void setActiveAccountButton(ClickableLabel* button);
    void restoreCachedWidgetIfNeeded();
    void updateAccount(CAccount* account);
    ClickableLabel* accountAddedHelper(CAccount* addedAccount);

    /** Early disconnect timer signals/slots and similar for clean shutdown */
    void disconnectNonEssentialSignals();

protected:
    void resizeEvent(QResizeEvent* event);
    void changeEvent(QEvent *e);
    void closeEvent(QCloseEvent *event);
    void showEvent(QShowEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    bool eventFilter(QObject *object, QEvent *event);

public:
    bool coreAppIsReadyForUIToQuit=false;
    WalletFrame* walletFrame = nullptr;
private:
    ClientModel* clientModel = nullptr;

    QLabel* labelWalletEncryptionIcon = nullptr;
    QLabel* labelWalletHDStatusIcon = nullptr;
    QLabel*connectionsControl = nullptr;
    QLabel* labelBlocksIcon = nullptr;
    QLabel* progressBarLabel = nullptr;
    QProgressBar* progressBar = nullptr;
    QProgressDialog* progressDialog = nullptr;

    QMenuBar* appMenuBar = nullptr;
    QAction* witnessDialogAction = nullptr;
    QAction* overviewAction = nullptr;
    QAction* historyAction = nullptr;
    QAction* quitAction = nullptr;
    QAction* sendCoinsAction = nullptr;
    QAction* sendCoinsMenuAction = nullptr;
    QAction* usedSendingAddressesAction = nullptr;
    QAction* usedReceivingAddressesAction = nullptr;
    QAction* aboutAction = nullptr;
    QAction* receiveCoinsAction = nullptr;
    QAction* receiveCoinsMenuAction = nullptr;
    QAction* optionsAction = nullptr;
    QAction* toggleHideAction = nullptr;
    QAction* encryptWalletAction = nullptr;
    QAction* backupWalletAction = nullptr;
    QAction* changePassphraseAction = nullptr;
    QAction* aboutQtAction = nullptr;
    QAction* openRPCConsoleAction = nullptr;
    QAction* openAction = nullptr;
    QAction* showHelpMessageAction = nullptr;
    QAction* importPrivateKeyAction = nullptr;
    QAction* rescanAction = nullptr;
    QAction* currencyAction = nullptr;

    QSystemTrayIcon* trayIcon = nullptr;
    QMenu* trayIconMenu = nullptr;

    Notificator* notificator = nullptr;
    RPCConsole* rpcConsole = nullptr;
    HelpMessageDialog* helpMessageDialog = nullptr;
    ModalOverlay* modalOverlay = nullptr;
    AccountSummaryWidget* accountSummaryWidget = nullptr;

    QToolBar* accountBar = nullptr;
    QToolBar* guldenBar = nullptr;
    QToolBar* spacerBarL = nullptr;
    QToolBar* spacerBarR = nullptr;
    QToolBar* tabsBar = nullptr;
    QToolBar* accountInfoBar = nullptr;
    QToolBar* statusToolBar = nullptr;
    QFrame* menuBarSpaceFiller = nullptr;
    QFrame* balanceContainer = nullptr;
    WelcomeDialog* welcomeScreen = nullptr;

    QFrame* accountScrollArea = nullptr;

    NewAccountDialog* dialogNewAccount = nullptr;
    AccountSettingsDialog* dialogAccountSettings = nullptr;
    BackupDialog* dialogBackup = nullptr;
    PasswordModifyDialog* dialogPasswordModify = nullptr;
    ExchangeRateDialog* dialogExchangeRate = nullptr;
    QWidget* cacheCurrentWidget = nullptr;

    CurrencyTicker* ticker = nullptr;
    NocksSettings* nocksSettings = nullptr;

    QLabel* labelBalance = nullptr;
    QLabel* labelBalanceForex = nullptr;

    QAction* accountSpacerAction = nullptr;
    QAction* passwordAction = nullptr;
    QAction* backupAction = nullptr;

    OptionsModel* optionsModel = nullptr;

    const QStyle* platformStyle = nullptr;

    QFrame* frameBlocks = nullptr;

    std::map<ClickableLabel*, CAccount*> m_accountMap;

    //Cache the balances so that we can easily re-use them when the currency ticker changes.
    CAmount balanceCached = 0;
    CAmount unconfirmedBalanceCached = 0;
    CAmount immatureBalanceCached = 0;
    CAmount watchOnlyBalanceCached = 0;
    CAmount watchUnconfBalanceCached = 0;
    CAmount watchImmatureBalanceCached = 0;

    /** Keep track of previous number of blocks, to detect progress */
    int prevBlocks = 0;
    int spinnerFrame = 0;

    /** Create the main UI actions. */
    void createActions();
    /** Create the menu bar and sub-menus. */
    void createMenuBar();
    /** Create the toolbars */
    void createToolBars();
    /** Create system tray icon and notification */
    void createTrayIcon(const NetworkStyle *networkStyle);
    /** Create system tray menu (or setup the dock menu) */
    void createTrayIconMenu();

    /** Enable or disable all wallet-related actions */
    void setWalletActionsEnabled(bool enabled);

    /** Connect core signals to GUI client */
    void subscribeToCoreSignals();
    /** Disconnect core signals from GUI client */
    void unsubscribeFromCoreSignals();

    /** Update UI with latest network info from model. */
    void updateNetworkState();

    void updateHeadersSyncProgressLabel(int current, int total);

Q_SIGNALS:
    /** Signal raised when a URI was entered or dragged to the GUI */
    void receivedURI(const QString &uri);

public Q_SLOTS:
    /** Set number of connections shown in the UI */
    void setNumConnections(int count);
    /** Set network state shown in the UI */
    void setNetworkActive(bool networkActive);
    /** Set number of blocks and last block date shown in the UI */
    void setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool headers);
    /** Set number of headers and headers sync progress in the UI */
    void setNumHeaders(int current, int total);

    /** Notify the user of an event from the core network or transaction handling code.
       @param[in] title     the message box / notification title
       @param[in] message   the displayed text
       @param[in] style     modality and style definitions (icon and used buttons - buttons only for message boxes)
                            @see CClientUIInterface::MessageBoxFlags
       @param[in] ret       pointer to a bool that will be modified to whether Ok was clicked (modal only)
    */
    void message(const QString &title, const QString &message, unsigned int style, bool *ret = NULL);

    void setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance);

    /* UI calls this to signal that the user wants to exit - this then causes the core to initiate proper shutdown etc. */
    void userWantsToQuit();

#ifdef ENABLE_WALLET
    /** Set the encryption status as shown in the UI.
       @param[in] status            current encryption status
       @see WalletModel::EncryptionStatus
    */
    void setEncryptionStatus(int status);

    bool handlePaymentRequest(const SendCoinsRecipient& recipient);

    /** Show incoming transaction notification for new transactions. */
    void incomingTransaction(const QString& date, int unit, const CAmount& amountReceived, const CAmount& amountSent, const QString& type, const QString& address, const QString& account, const QString& label);
#endif // ENABLE_WALLET

    //fixme: (2.1) The below are all ex GuldenGUI slots that should be factored back in and cleaned up.
    void NotifyRequestUnlock(void* wallet, QString reason);
    void NotifyRequestUnlockWithCallback(void* wallet, QString reason, std::function<void (void)> successCallback);
    void handlePaymentAccepted();

private Q_SLOTS:
#ifdef ENABLE_WALLET
    /** Switch to witness overview page */
    void showWitnessDialog();
    /** Switch to overview (home) page */
    void gotoOverviewPage();
    /** Switch to history (transactions) page */
    void gotoHistoryPage();
    /** Switch to receive coins page */
    void gotoReceiveCoinsPage();
    /** Switch to send coins page */
    void gotoSendCoinsPage(QString addr = "");

    /** Show open dialog */
    void openClicked();
#endif // ENABLE_WALLET
    /** Show configuration dialog */
    void optionsClicked();
    /** Show about dialog */
    void aboutClicked();
    /** Show debug window */
    void showDebugWindow();
    /** Show debug window and set focus to the console */
    void showDebugWindowActivateConsole();
    /** Show help message dialog */
    void showHelpMessageClicked();
#ifndef Q_OS_MAC
    /** Handle tray icon clicked */
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
#endif

    /** Show window if hidden, unminimize when minimized, rise when obscured or show if hidden and fToggleHidden is true */
    void showNormalIfMinimized(bool fToggleHidden = false);
    /** Simply calls showNormalIfMinimized(true) for use in SLOT() macro */
    void toggleHidden();

    /** Show progress dialog e.g. for verifychain */
    void showProgress(const QString &title, int nProgress);

    /** When hideTrayIcon setting is changed in OptionsModel hide or show the icon accordingly. */
    void setTrayIconVisible(bool);

    /** Toggle networking */
    void toggleNetworkActive();

    void showModalOverlay();

    //fixme: (2.1) The below are all ex GuldenGUI slots that should be factored back in and cleaned up.
    void activeAccountChanged(CAccount* account);
    void accountNameChanged(CAccount* account);
    void accountWarningChanged(CAccount* account);
    void balanceChanged();
    void accountAdded(CAccount* account);
    void accountDeleted(CAccount* account);
    void accountButtonPressed();
    void promptImportPrivKey();
    void promptRescan();
    void gotoWebsite();
    void gotoNewAccountDialog();
    void gotoPasswordDialog();
    void gotoBackupDialog();
    void dismissBackupDialog();
    void dismissPasswordDialog();
    void cancelNewAccountDialog();
    void acceptNewAccount();
    void acceptNewAccountMobile();
    void showAccountSettings();
    void dismissAccountSettings();
    void showExchangeRateDialog();
    void updateExchangeRates();
    void requestRenewWitness(CAccount* funderAccount);
    void requestFundWitness(CAccount* funderAccount);
    void requestEmptyWitness();
};

#endif // GULDEN_QT_GUI_H
