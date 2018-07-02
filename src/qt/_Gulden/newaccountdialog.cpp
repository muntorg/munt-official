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

    QString accountTemplate = "<table><tr><td style='padding: 10px'><span style='font-size: 12pt; color: #007aff;'>%1</span>  %2</td><tr><td style='padding: 10px'><ul style='type: circle; text-indent: 0px'>%3</ul></td><table>";
    QString textTransactionalAccount = accountTemplate.arg(GUIUtil::fontAwesomeLight("\uf09d")).arg(tr("Transactional account")).arg(tr("<li>Day to day fund management</li><li>Send and receive Gulden</li><li>Send funds to any elligible IBAN account</li><li>Easily transfer funds between mobile and desktop wallet</li>"));
    QString textMobileAccount = accountTemplate.arg(GUIUtil::fontAwesomeLight("\uf10b")).arg(tr("Linked mobile account")).arg(tr("<li>Top up, manage and control your mobile funds from the desktop</li><li>Empty your mobile funds with ease if phone is broken or stolen</li>"));
    QString textWitnessAccount = accountTemplate.arg(GUIUtil::fontAwesomeLight("\uf19c")).arg(tr("Witness account")).arg(tr("<li>Flexible period of 1 month to 3 years</li><li>Help secure the network with minimal hardware equirements</li><li>Grow your money by earning your share in the block rewards</li>"));
    QString textImportWitnessAccount = accountTemplate.arg(GUIUtil::fontAwesomeLight("\uf06e")).arg(tr("Import witness account")).arg(tr("<li>Import a witness account from another device</li><li>Let this device act as a backup witness device so that you are always available to witness</li>"));
    QString textImportPrivKey = accountTemplate.arg(GUIUtil::fontAwesomeLight("\uf084")).arg(tr("Import private key")).arg(tr("<li>Import a private key from cold storage</li><li>Not backed up as part of your recovery phrase</li>"));
    ui->labelTransactionAccount->setText(textTransactionalAccount);
    ui->labelMobileAccount->setText(textMobileAccount);
    ui->labelWitnessAccount->setText(textWitnessAccount);
    ui->labelImportWitnessOnlyAccount->setText(textImportWitnessAccount);
    ui->labelImportPrivateKey->setText(textImportPrivKey);

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
    connect(ui->labelImportWitnessOnlyAccount, SIGNAL(clicked()), this, SIGNAL(importWitnessOnly()));
    connect(ui->labelImportPrivateKey, SIGNAL(clicked()), this, SIGNAL(importPrivateKey()));
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
        LOCK(pactiveWallet->cs_wallet);
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

void NewAccountDialog::importMobile()
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

