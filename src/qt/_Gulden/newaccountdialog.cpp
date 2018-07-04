// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "newaccountdialog.h"
#include <qt/_Gulden/forms/ui_newaccountdialog.h>
#include "wallet/wallet.h"
#include "GuldenGUI.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "units.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "receiverequestdialog.h"
#include "recentrequeststablemodel.h"
#include "walletmodel.h"

#include <QAction>
#include <QCursor>
#include <QItemSelection>
#include <QMessageBox>
#include <QScrollBar>
#include <QTextDocument>
#include <QStringListModel>

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

NewAccountDialog::NewAccountDialog(const QStyle *_platformStyle, QWidget *parent, WalletModel* model)
: QFrame(parent)
, ui(new Ui::NewAccountDialog)
, platformStyle(_platformStyle)
, newAccount( NULL )
, walletModel( model )
{
    ui->setupUi(this);

    // Set object names for styling
    setObjectName("newAccountDialog");
    ui->scanToConnectPage->setObjectName("scanToConnectPage");
    ui->newAccountPage->setObjectName("newAccountPage");

    // Setup cursors for all clickable elements
    ui->cancelButton->setCursor(Qt::PointingHandCursor);
    ui->doneButton2->setCursor(Qt::PointingHandCursor);
    ui->cancelButton2->setCursor(Qt::PointingHandCursor);

    ui->labelMobileAccount->setCursor(Qt::PointingHandCursor);
    ui->labelTransactionAccount->setCursor(Qt::PointingHandCursor);
    ui->labelWitnessAccount->setCursor(Qt::PointingHandCursor);
    ui->labelImportWitnessOnlyAccount->setCursor(Qt::PointingHandCursor);
    ui->labelImportPrivateKey->setCursor(Qt::PointingHandCursor);

    ui->labelMobileAccount->setObjectName("add_account_type_label_mobile");
    ui->labelTransactionAccount->setObjectName("add_account_type_label_transactional");
    ui->labelWitnessAccount->setObjectName("add_account_type_label_witness");
    ui->labelImportWitnessOnlyAccount->setObjectName("add_account_type_label_witnessonly");
    ui->labelImportPrivateKey->setObjectName("add_account_type_label_privkey");

    toolTipTransactionalAccount = tr("<li>Day to day fund management</li><li>Send and receive Gulden</li><li>Send funds to any elligible IBAN account</li>");
    toolTipMobileAccount = tr("<li>Top up, manage and control your mobile funds from the desktop</li><li>Empty your mobile funds with ease if phone is broken or stolen</li>");
    toolTipWitnessAccount = tr("<li>Grow your money</li><li>Flexible period of 1 month to 3 years</li><li>Help secure the network with minimal hardware equirements</li>");
    toolTipImportWitnessAccount = tr("<li>Import a witness account from another device</li><li>Let this device act as a backup witness device so that you are always available to witness</li>");
    toolTipImportPrivKey = tr("<li>Import a private key from cold storage</li><li>Not backed up as part of your recovery phrase</li>");

    ui->labelTransactionAccount->setToolTip(toolTipTransactionalAccount);
    ui->labelMobileAccount->setToolTip(toolTipMobileAccount);
    ui->labelWitnessAccount->setToolTip(toolTipWitnessAccount);
    ui->labelImportWitnessOnlyAccount->setToolTip(toolTipImportWitnessAccount);
    ui->labelImportPrivateKey->setToolTip(toolTipImportPrivKey);

    updateTextForSize(size());

    // Set default display state
    ui->stackedWidget->setCurrentIndex(0);
    setValid(ui->newAccountName, true);
    ui->cancelButton2->setVisible(false);

    // Set default keyboard focus
    ui->newAccountName->setFocus();

    // Connect signals.
    connect(ui->labelTransactionAccount, SIGNAL(clicked()), this, SLOT(addAccount()));
    connect(ui->labelWitnessAccount, SIGNAL(clicked()), this, SLOT(addWitnessAccount()));
    connect(ui->doneButton2, SIGNAL(clicked()), this, SIGNAL(addAccountMobile()));
    connect(ui->labelImportWitnessOnlyAccount, SIGNAL(clicked()), this, SLOT(importWitnessOnly()));
    connect(ui->labelImportPrivateKey, SIGNAL(clicked()), this, SLOT(importPrivateKey()));
    connect(ui->doneButton2, SIGNAL(clicked()), this, SIGNAL(addAccountMobile()));
    connect(ui->cancelButton, SIGNAL(clicked()), this, SIGNAL(cancel()));
    connect(ui->cancelButton2, SIGNAL(clicked()), this, SLOT(cancelMobile()));
    connect(ui->labelMobileAccount, SIGNAL(clicked()), this, SLOT(connectToMobile()));
    connect(ui->newAccountName, SIGNAL(textEdited(QString)), this, SLOT(valueChanged()));

    ui->labelMobileAccount->forceStyleRefresh();
    ui->labelTransactionAccount->forceStyleRefresh();
    ui->labelWitnessAccount->forceStyleRefresh();
    ui->labelImportWitnessOnlyAccount->forceStyleRefresh();
    ui->labelImportPrivateKey->forceStyleRefresh();
}


NewAccountDialog::~NewAccountDialog()
{
    delete ui;
}


void NewAccountDialog::connectToMobile()
{
    if (!ui->newAccountName->text().simplified().isEmpty())
    {
        ui->stackedWidget->setCurrentIndex(1);

        ui->scanQRCode->setText(tr("Click here to make QR code visible.\nWARNING: please ensure that you are the only person who can see this QR code as otherwise it could be used to access your funds."));
        connect(ui->scanQRCode, SIGNAL( clicked() ), this, SLOT( showSyncQr() ));
        ui->scanQRSyncHeader->setVisible(true);
    }
    else
    {
        ui->newAccountName->setFocus();
        setValid(ui->newAccountName, false);
    }
}

void NewAccountDialog::showSyncQr()
{
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (ctx.isValid())
    {
        newAccount = pactiveWallet->GenerateNewAccount(ui->newAccountName->text().toStdString(), AccountState::Normal, AccountType::Mobi);
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        {
            int64_t currentTime = newAccount->getEarliestPossibleCreationTime();

            std::string payoutAddress;
            CReserveKeyOrScript reservekey(pactiveWallet, newAccount, KEYCHAIN_CHANGE);
            CPubKey vchPubKey;
            if (!reservekey.GetReservedKey(vchPubKey))
                return;
            payoutAddress = CGuldenAddress(vchPubKey.GetID()).ToString();

            QString qrString = QString::fromStdString("guldensync:" + CGuldenSecretExt<CExtKey>(*newAccount->GetAccountMasterPrivKey()).ToString( QString::number(currentTime).toStdString(), payoutAddress ) );
            ui->scanQRCode->setCode(qrString);

            disconnect(this, SLOT( showSyncQr() ));
        }
    }
}

void NewAccountDialog::addAccount()
{
    m_Type = Transactional;
    if (!ui->newAccountName->text().simplified().isEmpty())
    {
        Q_EMIT accountAdded();
    }
    else
    {
        ui->newAccountName->setFocus();
        setValid(ui->newAccountName, false);
    }
}

void NewAccountDialog::addWitnessAccount()
{
    m_Type = FixedDeposit;
    if (!ui->newAccountName->text().simplified().isEmpty())
    {
        Q_EMIT accountAdded();
    }
    else
    {
        ui->newAccountName->setFocus();
        setValid(ui->newAccountName, false);
    }
}

void NewAccountDialog::importWitnessOnly()
{
    m_Type = WitnessOnly;
    if (!ui->newAccountName->text().simplified().isEmpty())
    {
        Q_EMIT accountAdded();
    }
    else
    {
        ui->newAccountName->setFocus();
        setValid(ui->newAccountName, false);
    }
}

void NewAccountDialog::importPrivateKey()
{
    m_Type = ImportKey;
    if (!ui->newAccountName->text().simplified().isEmpty())
    {
        Q_EMIT accountAdded();
    }
    else
    {
        ui->newAccountName->setFocus();
        setValid(ui->newAccountName, false);
    }
}

void NewAccountDialog::cancelMobile()
{
      ui->stackedWidget->setCurrentIndex(0);
}

void NewAccountDialog::valueChanged()
{
    setValid(ui->newAccountName, true);
}


QString NewAccountDialog::getAccountName()
{
    return ui->newAccountName->text();
}

void NewAccountDialog::resizeEvent(QResizeEvent* event)
{
    updateTextForSize(event->size());
}

void NewAccountDialog::updateTextForSize(const QSize& size)
{
    static QString accountTemplateVerticallyCompact = "<table height=\"100%\" width=\"100%\"><tr><td valign=middle align=center><span style='font-size: 12pt;'>%1</span> %2</td></tr><table>";
    static QString accountTemplateExpanded = "<table cellpadding=5 height=\"100%\" width=\"100%\"><tr><td valign=top align=left><span style='font-size: 12pt;'>%1</span> %2</td></tr><tr style=\"color: #999;\"><td valign=top align=left style=\"white-space: wrap; text-align: justify\"><ul style='type: circle; text-indent: 0px'>%3</ul></td></tr><table>";
    QString accountTemplateNewAccounts;
    QString accountTemplateImportAccounts;
    int verticalDisplayType = -1;
    int horizontalDisplayType = -1;
    if (size.height() < 550)
        verticalDisplayType = 0;
    else if (size.height() < 700)
        verticalDisplayType = 1;
    else
        verticalDisplayType = 2;
    if (size.width() < 540)
        horizontalDisplayType = 0;
    else if (size.width() < 680)
        horizontalDisplayType = 1;
    else
        horizontalDisplayType = 2;

    // Prevent multiple expensive updates during resize.
    if (horizontalCachedDisplayType == horizontalDisplayType && verticalCachedDisplayType == verticalDisplayType)
        return;

    verticalCachedDisplayType = verticalDisplayType;
    horizontalCachedDisplayType = horizontalDisplayType;
    uint64_t nWidth = 150;
    uint64_t nHeightAdd = 30;
    uint64_t nHeightImport = 30;

    accountTemplateNewAccounts = accountTemplateVerticallyCompact;
    accountTemplateImportAccounts = accountTemplateVerticallyCompact;
    if (horizontalDisplayType == 2)
    {
        nWidth = 200;
        if (verticalDisplayType >= 1)
        {
            accountTemplateNewAccounts = accountTemplateExpanded;
            nHeightAdd = 150;
        }
        if (verticalDisplayType == 2)
        {
            accountTemplateImportAccounts = accountTemplateExpanded;
            nHeightImport = 150;
        }
    }

    if (horizontalDisplayType == 0)
    {
        nWidth = 130;
        ui->accButtonHSpacer1->changeSize(20, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->accButtonHSpacer2->changeSize(20, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->accButtonHSpacer3->changeSize(20, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    else
    {
        ui->accButtonHSpacer1->changeSize(40, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->accButtonHSpacer2->changeSize(40, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->accButtonHSpacer3->changeSize(40, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    // Padding necessary to force table to full height.
    QString padding = "<br/><br/><br/><br/>";
    QString textTransactionalAccount = accountTemplateNewAccounts.arg(GUIUtil::fontAwesomeLight("\uf09d")).arg(tr("Standard")).arg(toolTipTransactionalAccount+padding);
    QString textMobileAccount = accountTemplateNewAccounts.arg(GUIUtil::fontAwesomeLight("\uf10b")).arg(tr("Linked mobile")).arg(toolTipMobileAccount+padding);
    QString textWitnessAccount = accountTemplateNewAccounts.arg(GUIUtil::fontAwesomeLight("\uf19c")).arg(tr("Witness")).arg(toolTipWitnessAccount+padding);
    QString textImportWitnessAccount = accountTemplateImportAccounts.arg(GUIUtil::fontAwesomeLight("\uf06e")).arg(tr("Witness-only")).arg(toolTipImportWitnessAccount+padding);
    QString textImportPrivKey = accountTemplateImportAccounts.arg(GUIUtil::fontAwesomeLight("\uf084")).arg(tr("Private key")).arg(toolTipImportPrivKey+padding);

    ui->labelTransactionAccount->setText(textTransactionalAccount);
    ui->labelTransactionAccount->setFixedSize(nWidth, nHeightAdd);
    ui->labelMobileAccount->setText(textMobileAccount);
    ui->labelMobileAccount->setFixedSize(nWidth, nHeightAdd);
    ui->labelWitnessAccount->setText(textWitnessAccount);
    ui->labelWitnessAccount->setFixedSize(nWidth, nHeightAdd);
    ui->labelImportWitnessOnlyAccount->setText(textImportWitnessAccount);
    ui->labelImportWitnessOnlyAccount->setFixedSize(nWidth, nHeightImport);
    ui->labelImportPrivateKey->setText(textImportPrivKey);
    ui->labelImportPrivateKey->setFixedSize(nWidth, nHeightImport);

    ui->hLayout1->invalidate();
    ui->hLayout2->invalidate();
    ui->vLayout1->invalidate();
    layout()->invalidate();
}
