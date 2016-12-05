// Copyright (c) 2015-2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_QT_GULDENGUI_H
#define GULDEN_QT_GULDENGUI_H

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include <QObject>
#include <QProxyStyle>
#include <map>
#include <string>
#include "amount.h"

class BitcoinGUI;
class QPushButton;
class ClickableLabel;
class QToolBar;
class QLabel;
class QAction;
class AccountSummaryWidget;
class CAccount;
class OptionsModel;
class CReserveKey;
class CurrencyTicker;
class NocksSettings;
class QDialog;
class NewAccountDialog;
class AccountSettingsDialog;
class BackupDialog;
class PasswordModifyDialog;
class ExchangeRateDialog;
class QFrame;
class QStyle;
class QColorGroup;
class WelcomeDialog;
class CWallet;
class QLineEdit;
class QTextEdit;

#define GULDEN_DIALOG_CANCEL_BUTTON_STYLE "QPushButton{color: #e02121; margin-left: 40px; padding-left: 0px}"
#define GULDEN_DIALOG_CONFIRM_BUTTON_STYLE "QPushButton{color: #fff; background-color: #007aff; margin-right: 40px}"
#define GULDEN_DIALOG_HLINE_STYLE "QFrame{max-height: 1px; margin: 0px; padding: 0px; background-color: #ebebeb; margin-left: 40px; margin-right: 40px; margin-bottom: 18px;}"
#define GULDEN_DIALOG_CANCEL_BUTTON_STYLE_NOMARGIN "QPushButton{color: #e02121; padding-left: 0px}"
#define GULDEN_DIALOG_CONFIRM_BUTTON_STYLE_NOMARGIN "QPushButton{color: #fff; background-color: #007aff;}"
#define GULDEN_DIALOG_HLINE_STYLE_NOMARGIN "QFrame{max-height: 1px; margin: 0px; padding: 0px; background-color: #ebebeb; margin-bottom: 18px;}"
extern std::string CurrencySymbolForCurrencyCode(const std::string& currencyCode);

void setValid(QWidget* control, bool validity);
void burnLineEditMemory(QLineEdit* edit);
void burnTextEditMemory(QTextEdit* edit);
QString limitString(const QString& string, int maxLength);

class GuldenProxyStyle : public QProxyStyle {
    Q_OBJECT
public:
    GuldenProxyStyle();
    void drawItemText(QPainter* painter, const QRect& rectangle, int alignment, const QPalette& palette, bool enabled, const QString& text, QPalette::ColorRole textRole = QPalette::NoRole) const;
    bool altDown;
};

class GuldenEventFilter : public QObject {
    Q_OBJECT

public:
    explicit GuldenEventFilter(QStyle* style, QWidget* parent, GuldenProxyStyle* guldenStyle, QObject* parentObject = 0)
        : QObject(parentObject)
        , parentStyle(style)
        , parentObject(parent)
        , guldenProxyStyle(guldenStyle)
    {
    }

protected:
    bool eventFilter(QObject* obj, QEvent* evt);

private:
    QStyle* parentStyle;
    QWidget* parentObject;
    GuldenProxyStyle* guldenProxyStyle;
};

class GuldenGUI : public QObject {
    Q_OBJECT

public:
    explicit GuldenGUI(BitcoinGUI* pImpl);
    virtual ~GuldenGUI();

    void doPostInit();
    void doApplyStyleSheet();
    void resizeToolBarsGulden();
    void refreshAccountControls();
    bool setCurrentWallet(const QString& name);

    void setOptionsModel(OptionsModel* optionsModel);

    void setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance);

    void createToolBarsGulden();
    void hideToolBars();
    void showToolBars();

    void hideProgressBarLabel();
    void showProgressBarLabel();

    bool welcomeScreenIsVisible();

    static QDialog* createDialog(QWidget* parent, QString message, QString confirmLabel, QString cancelLabel, int minWidth, int minHeight);

protected:
private:
    BitcoinGUI* m_pImpl;
    QToolBar* accountBar;
    QToolBar* guldenBar;
    QToolBar* spacerBarL;
    QToolBar* spacerBarR;
    QToolBar* tabsBar;
    QToolBar* accountInfoBar;
    QToolBar* statusBar;
    QFrame* menuBarSpaceFiller;
    QFrame* balanceContainer;
    WelcomeDialog* welcomeScreen;

    QFrame* accountScrollArea;

    AccountSummaryWidget* accountSummaryWidget;
    NewAccountDialog* dialogNewAccount;
    AccountSettingsDialog* dialogAccountSettings;
    BackupDialog* dialogBackup;
    PasswordModifyDialog* dialogPasswordModify;
    ExchangeRateDialog* dialogExchangeRate;
    QWidget* cacheCurrentWidget;

    std::map<ClickableLabel*, CAccount*> m_accountMap;
    CurrencyTicker* ticker;
    NocksSettings* nocksSettings;

    QLabel* labelBalance;
    QLabel* labelBalanceForex;

    QAction* accountSpacerAction;
    QAction* passwordAction;
    QAction* backupAction;

    OptionsModel* optionsModel;

    ClickableLabel* createAccountButton(const QString& accountName);
    void setActiveAccountButton(ClickableLabel* button);
    void restoreCachedWidgetIfNeeded();
    void updateAccount(CAccount* account);

    CReserveKey* receiveAddress;

    CAmount balanceCached;
    CAmount unconfirmedBalanceCached;
    CAmount immatureBalanceCached;
    CAmount watchOnlyBalanceCached;
    CAmount watchUnconfBalanceCached;
    CAmount watchImmatureBalanceCached;

Q_SIGNALS:

public Q_SLOTS:
    void NotifyRequestUnlock(void* wallet, QString reason);

private Q_SLOTS:
    void activeAccountChanged(CAccount* account);
    void accountListChanged();
    void balanceChanged();
    void accountAdded(CAccount* account);
    void accountDeleted(CAccount* account);
    void accountButtonPressed();
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
};

#endif // GULDEN_QT_GULDENGUI_H
