// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include "optionsdialog.h"
#include "ui_optionsdialog.h"

#include "units.h"
#include "guiutil.h"
#include "optionsmodel.h"

#include "validation/validation.h" // for DEFAULT_SCRIPTCHECK_THREADS and MAX_SCRIPTCHECK_THREADS
#include "netbase.h"
#include "txdb.h" // for -dbcache defaults

#ifdef ENABLE_WALLET
#include "wallet/wallet.h" // for CWallet::GetRequiredFee()
#endif

#include <QDataWidgetMapper>
#include <QDir>
#include <QIntValidator>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>

#include "_Gulden/GuldenGUI.h"
#include <unity/appmanager.h>

//Very primitive function to capatalise first letter of string (only for latin alphabet).
//Only used to "beautify" language selection list so doesn't need to be fast or perfect.
void CapitaliseStringLatinOnly(QString& str)
{
    if (str.isEmpty())
        return;
    if (str[0] == 'a') {str[0]='A';}
    else if (str[0] == 'b') {str[0]='B';}
    else if (str[0] == 'c') {str[0]='C';}
    else if (str[0] == 'd') {str[0]='D';}
    else if (str[0] == 'e') {str[0]='E';}
    else if (str[0] == 'f') {str[0]='F';}
    else if (str[0] == 'g') {str[0]='G';}
    else if (str[0] == 'h') {str[0]='H';}
    else if (str[0] == 'i') {str[0]='I';}
    else if (str[0] == 'j') {str[0]='J';}
    else if (str[0] == 'k') {str[0]='K';}
    else if (str[0] == 'l') {str[0]='L';}
    else if (str[0] == 'm') {str[0]='M';}
    else if (str[0] == 'n') {str[0]='N';}
    else if (str[0] == 'o') {str[0]='O';}
    else if (str[0] == 'p') {str[0]='P';}
    else if (str[0] == 'q') {str[0]='Q';}
    else if (str[0] == 'r') {str[0]='R';}
    else if (str[0] == 's') {str[0]='S';}
    else if (str[0] == 't') {str[0]='T';}
    else if (str[0] == 'u') {str[0]='U';}
    else if (str[0] == 'x') {str[0]='X';}
    else if (str[0] == 'y') {str[0]='Y';}
    else if (str[0] == 'z') {str[0]='Z';}
}

OptionsDialog::OptionsDialog(QWidget *parent, bool enableWallet) :
    QDialog(parent),
    ui(new Ui::OptionsDialog),
    model(0),
    mapper(0)
{
    ui->setupUi(this);

    /* Main elements init */
    ui->databaseCache->setMinimum(nMinDbCache);
    ui->databaseCache->setMaximum(nMaxDbCache);
    ui->threadsScriptVerif->setMinimum(-GetNumCores());
    ui->threadsScriptVerif->setMaximum(MAX_SCRIPTCHECK_THREADS);

    QFrame* horizontalLine = new QFrame(this);
    horizontalLine->setFrameStyle(QFrame::HLine);
    horizontalLine->setFixedHeight(1);
    horizontalLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    horizontalLine->setStyleSheet(GULDEN_DIALOG_HLINE_STYLE_NOMARGIN);
    ui->verticalLayout->insertWidget(2, horizontalLine);

    setWindowFlags(windowFlags() ^ Qt::WindowContextHelpButtonHint);

    ui->okButton->setCursor(Qt::PointingHandCursor);
    ui->cancelButton->setCursor(Qt::PointingHandCursor);
    ui->resetButton->setCursor(Qt::PointingHandCursor);

    /* Network elements init */
#ifndef USE_UPNP
    ui->mapPortUpnp->setEnabled(false);
#endif

    ui->proxyIp->setEnabled(false);
    ui->proxyPort->setEnabled(false);
    ui->proxyPort->setValidator(new QIntValidator(1, 65535, this));

    ui->proxyIpTor->setEnabled(false);
    ui->proxyPortTor->setEnabled(false);
    ui->proxyPortTor->setValidator(new QIntValidator(1, 65535, this));

    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyIp, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyPort, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), this, SLOT(updateProxyValidationState()));

    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), ui->proxyIpTor, SLOT(setEnabled(bool)));
    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), ui->proxyPortTor, SLOT(setEnabled(bool)));
    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), this, SLOT(updateProxyValidationState()));

    /* Window elements init */
    #ifdef Q_OS_MAC
    // Hide these options on osx as they don't make any sense for the os.
    ui->hideTrayIcon->setVisible(false);
    ui->minimizeToTray->setVisible(false);
    #endif

    /* remove Wallet tab in case of -disablewallet */
    if (!enableWallet) {
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWallet));
    }

    /* Display elements init */
    QDir translations(":translations");

    ui->guldenAtStartup->setToolTip(ui->guldenAtStartup->toolTip().arg(tr(PACKAGE_NAME)));
    ui->guldenAtStartup->setText(ui->guldenAtStartup->text().arg(tr(PACKAGE_NAME)));

    ui->lang->setToolTip(ui->lang->toolTip().arg(tr(PACKAGE_NAME)));
    ui->lang->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    for(const QString &langStr : translations.entryList())
    {
        QLocale locale(langStr);

        QString nativeLanguageName = locale.nativeLanguageName();

        // Neaten up list by ensuring all language names are at least capatalised (was a messy mix/match otherwise)
        // Only works for latin languages but this is fine, no need to get fancier than that for now.
        CapitaliseStringLatinOnly(nativeLanguageName);

        // check if the locale name consists of 2 parts (language_country)
        if(langStr.contains("_"))
        {
            // display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" 
            ui->lang->addItem(nativeLanguageName + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
        else
        {
            //fixme: (FUT)
            // For some reason QLocale("en").nativeLanguageName() returns "American English".
            // This is likely/potentially wrong for other languages as well but English was the most obvious/important to have right.
            // No time to dig into it now so for now we just hardcode a fix here for now, we should look into this in future.
            if (langStr == "en") { nativeLanguageName = "English"; }
            else if (langStr == "eo") { nativeLanguageName = "Esperanto"; }
            else if (langStr == "ku") { nativeLanguageName = "Kurdî"; }
            else if (langStr == "kr") { nativeLanguageName = "Kanuri"; }
            else if (langStr == "la-VA") { nativeLanguageName = "Latin"; }
            else if (langStr == "cs") { nativeLanguageName = "Čeština"; }

            // display language strings as "native language (locale name)", e.g. "Deutsch (de)"
            ui->lang->addItem(nativeLanguageName + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
    }
#if QT_VERSION >= 0x040700
    ui->thirdPartyTxUrls->setPlaceholderText("https://example.com/tx/%s");
#endif

    ui->unit->setModel(new GuldenUnits(this));

    /* Widget-to-option mapper */
    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);

    #ifdef Q_OS_MAC
    ui->minimizeOnClose->setText(tr("H&ide to dock on close"));
    ui->minimizeOnClose->setToolTip(tr("Hide the application to the dock when the window is closed. When this option is enabled, the application will be closed only after selecting Exit in the menu."));
    #endif

    /* setup/change UI elements when proxy IPs are invalid/valid */
    ui->proxyIp->setCheckValidator(new ProxyAddressValidator(parent));
    ui->proxyIpTor->setCheckValidator(new ProxyAddressValidator(parent));
    connect(ui->proxyIp, SIGNAL(validationDidChange(QValidatedLineEdit *)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyIpTor, SIGNAL(validationDidChange(QValidatedLineEdit *)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyPort, SIGNAL(textChanged(const QString&)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyPortTor, SIGNAL(textChanged(const QString&)), this, SLOT(updateProxyValidationState()));
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

void OptionsDialog::setModel(OptionsModel *_model)
{
    this->model = _model;

    if(_model)
    {
        /* check if client restart is needed and show persistent message */
        if (_model->isRestartRequired())
            showRestartWarning(true);

        QString strLabel = _model->getOverriddenByCommandLine();
        if (strLabel.isEmpty())
        {
            ui->overriddenByCommandLineInfoLabel->setVisible(false);
            ui->overriddenByCommandLineLabel->setVisible(false);
        }
        else
        {
            ui->overriddenByCommandLineLabel->setVisible(true);
            ui->overriddenByCommandLineInfoLabel->setVisible(true);
            ui->overriddenByCommandLineLabel->setText(strLabel);
        }

        mapper->setModel(_model);
        setMapper();
        mapper->toFirst();

        updateDefaultProxyNets();
    }

    /* warn when one of the following settings changes by user action (placed here so init via mapper doesn't trigger them) */

    /* Main */
    connect(ui->databaseCache, SIGNAL(valueChanged(int)), this, SLOT(showRestartWarning()));
    connect(ui->threadsScriptVerif, SIGNAL(valueChanged(int)), this, SLOT(showRestartWarning()));
    /* Wallet */
    connect(ui->spendZeroConfChange, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    /* Network */
    connect(ui->allowIncoming, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->connectSocks, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->connectSocksTor, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    /* Display */
    connect(ui->lang, SIGNAL(valueChanged()), this, SLOT(showRestartWarning()));
    connect(ui->thirdPartyTxUrls, SIGNAL(textChanged(const QString &)), this, SLOT(showRestartWarning()));
}

void OptionsDialog::setMapper()
{
    /* Main */
    mapper->addMapping(ui->guldenAtStartup, OptionsModel::StartAtStartup);
    mapper->addMapping(ui->threadsScriptVerif, OptionsModel::ThreadsScriptVerif);
    mapper->addMapping(ui->databaseCache, OptionsModel::DatabaseCache);
    mapper->addMapping(ui->autoUpdateCheck, OptionsModel::AutoUpdateCheck);

    /* Wallet */
    mapper->addMapping(ui->spendZeroConfChange, OptionsModel::SpendZeroConfChange);
    mapper->addMapping(ui->coinControlFeatures, OptionsModel::CoinControlFeatures);

    /* Network */
    mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);
    mapper->addMapping(ui->allowIncoming, OptionsModel::Listen);

    mapper->addMapping(ui->connectSocks, OptionsModel::ProxyUse);
    mapper->addMapping(ui->proxyIp, OptionsModel::ProxyIP);
    mapper->addMapping(ui->proxyPort, OptionsModel::ProxyPort);

    mapper->addMapping(ui->connectSocksTor, OptionsModel::ProxyUseTor);
    mapper->addMapping(ui->proxyIpTor, OptionsModel::ProxyIPTor);
    mapper->addMapping(ui->proxyPortTor, OptionsModel::ProxyPortTor);

    /* Window */
#ifndef Q_OS_MAC
    mapper->addMapping(ui->hideTrayIcon, OptionsModel::HideTrayIcon);
    mapper->addMapping(ui->minimizeToTray, OptionsModel::MinimizeToTray);
    mapper->addMapping(ui->minimizeOnClose, OptionsModel::MinimizeOnClose);
#else
    mapper->addMapping(ui->minimizeOnClose, OptionsModel::DockOnClose);
#endif

    /* Display */
    mapper->addMapping(ui->lang, OptionsModel::Language);
    mapper->addMapping(ui->unit, OptionsModel::DisplayUnit);
    mapper->addMapping(ui->thirdPartyTxUrls, OptionsModel::ThirdPartyTxUrls);

    //fixme: (Post-2.1) - look at adding these back
    ui->coinControlFeatures->setVisible(false);
    ui->unit->setVisible(false);
    ui->thirdPartyTxUrls->setVisible(false);
    ui->unitLabel->setVisible(false);
    ui->thirdPartyTxUrlsLabel->setVisible(false);
}

void OptionsDialog::setOkButtonState(bool fState)
{
    ui->okButton->setEnabled(fState);
}

void OptionsDialog::on_resetButton_clicked()
{
    if(model)
    {
        // confirmation dialog
        QMessageBox::StandardButton btnRetVal = QMessageBox::question(this, tr("Confirm options reset"),
            tr("Client restart required to activate changes.") + "<br><br>" + tr("Client will be shut down. Do you want to proceed?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

        if(btnRetVal == QMessageBox::Cancel)
            return;

        /* reset all options and close GUI */
        model->Reset();
        GuldenAppManager::gApp->shutdown();
    }
}

void OptionsDialog::on_openGuldenConfButton_clicked()
{
    /* explain the purpose of the config file */
    QMessageBox::information(this, tr("Configuration options"),
        tr("The configuration file is used to specify advanced user options which override GUI settings. "
           "Additionally, any command-line options will override this configuration file."));

    /* show an error if there was some problem opening the file */
    if (!GUIUtil::openGuldenConf())
        QMessageBox::critical(this, tr("Error"), tr("The configuration file could not be opened."));
}

void OptionsDialog::on_okButton_clicked()
{
    mapper->submit();
    accept();
    updateDefaultProxyNets();
}

void OptionsDialog::on_cancelButton_clicked()
{
    reject();
}

void OptionsDialog::on_hideTrayIcon_stateChanged(int fState)
{
    if(fState)
    {
        ui->minimizeToTray->setChecked(false);
        ui->minimizeToTray->setEnabled(false);
    }
    else
    {
        ui->minimizeToTray->setEnabled(true);
    }
}

void OptionsDialog::showRestartWarning(bool fPersistent)
{
    ui->statusLabel->setStyleSheet("QLabel { color: red; }");

    if(fPersistent)
    {
        ui->statusLabel->setText(tr("Client restart required to activate changes."));
    }
    else
    {
        ui->statusLabel->setText(tr("This change would require a client restart."));
        // clear non-persistent status label after 10 seconds
        // Todo: should perhaps be a class attribute, if we extend the use of statusLabel
        QTimer::singleShot(10000, this, SLOT(clearStatusLabel()));
    }
}

void OptionsDialog::clearStatusLabel()
{
    ui->statusLabel->clear();
}

void OptionsDialog::updateProxyValidationState()
{
    QValidatedLineEdit *pUiProxyIp = ui->proxyIp;
    QValidatedLineEdit *otherProxyWidget = (pUiProxyIp == ui->proxyIpTor) ? ui->proxyIp : ui->proxyIpTor;
    if (pUiProxyIp->isValid() && (!ui->proxyPort->isEnabled() || ui->proxyPort->text().toInt() > 0) && (!ui->proxyPortTor->isEnabled() || ui->proxyPortTor->text().toInt() > 0))
    {
        setOkButtonState(otherProxyWidget->isValid()); //only enable ok button if both proxys are valid
        ui->statusLabel->clear();
    }
    else
    {
        setOkButtonState(false);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied proxy address is invalid."));
    }
}

void OptionsDialog::updateDefaultProxyNets()
{
    proxyType proxy;
    std::string strProxy;
    QString strDefaultProxyGUI;

    GetProxy(NET_IPV4, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachIPv4->setChecked(true) : ui->proxyReachIPv4->setChecked(false);

    GetProxy(NET_IPV6, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachIPv6->setChecked(true) : ui->proxyReachIPv6->setChecked(false);

    GetProxy(NET_TOR, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachTor->setChecked(true) : ui->proxyReachTor->setChecked(false);
}

ProxyAddressValidator::ProxyAddressValidator(QObject *parent) :
QValidator(parent)
{
}

QValidator::State ProxyAddressValidator::validate(QString &input, int &pos) const
{
    (unused)pos;
    // Validate the proxy
    CService serv(LookupNumeric(input.toStdString().c_str(), 9050));
    proxyType addrProxy = proxyType(serv, true);
    if (addrProxy.IsValid())
        return QValidator::Acceptable;

    return QValidator::Invalid;
}
