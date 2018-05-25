// Copyright (c) 2015-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_QT_GULDENGUI_H
#define GULDEN_QT_GULDENGUI_H

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include <QObject>
#include <QProxyStyle>
#include <map>
#include <functional>
#include <string>
#include "amount.h"


class GUI;
class QPushButton;
class ClickableLabel;
class QToolBar;
class QLabel;
class QAction;
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
class QMenu;


#define GULDEN_DIALOG_CANCEL_BUTTON_STYLE "QPushButton{color: #e02121; margin-left: 40px; padding-left: 0px}"
#define GULDEN_DIALOG_CONFIRM_BUTTON_STYLE "QPushButton{color: #fff; background-color: #007aff; margin-right: 40px}"
#define GULDEN_DIALOG_HLINE_STYLE "QFrame{max-height: 1px; margin: 0px; padding: 0px; background-color: #ebebeb; margin-left: 40px; margin-right: 40px; margin-bottom: 18px;}"
#define GULDEN_DIALOG_CANCEL_BUTTON_STYLE_NOMARGIN "QPushButton{color: #e02121; padding-left: 0px}"
#define GULDEN_DIALOG_CONFIRM_BUTTON_STYLE_NOMARGIN "QPushButton{color: #fff; background-color: #007aff;}"
#define GULDEN_DIALOG_HLINE_STYLE_NOMARGIN "QFrame{max-height: 1px; margin: 0px; padding: 0px; background-color: #ebebeb; margin-bottom: 18px;}"
extern std::string CurrencySymbolForCurrencyCode(const std::string& currencyCode);

//Colors
extern const char* ACCENT_COLOR_1;
extern const char* ACCENT_COLOR_2;
extern const char* TEXT_COLOR_1;
extern const char* COLOR_VALIDATION_FAILED;

//Helpers
void setValid(QWidget* control, bool validity);
void burnLineEditMemory(QLineEdit* edit);
void burnTextEditMemory(QTextEdit* edit);
QString limitString(const QString& string, int maxLength);

#endif // GULDEN_QT_GULDENGUI_H
