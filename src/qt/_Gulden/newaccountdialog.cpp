// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "newaccountdialog.h"
#include <qt/_Gulden/forms/ui_newaccountdialog.h>
#include "wallet/wallet.h"
#include "GuldenGUI.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "receiverequestdialog.h"
#include "recentrequeststablemodel.h"
#include "walletmodel.h"

#include <QAction>
#include <QCursor>
#include <QItemSelection>
#include <QMessageBox>
#include <QScrollBar>
#include <QTextDocument>

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

NewAccountDialog::NewAccountDialog(const PlatformStyle *platformStyle, QWidget *parent, WalletModel* model)
: QFrame(parent)
, ui(new Ui::NewAccountDialog)
, platformStyle(platformStyle)
, newAccount( NULL )
, walletModel( model )
{
    ui->setupUi(this);
    
    // Set object names for styling
    setObjectName("newAccountDialog");
    ui->scanToConnectPage->setObjectName("scanToConnectPage");
    ui->newAccountPage->setObjectName("newAccountPage");
    
    // Setup cursors for all clickable elements
    ui->doneButton->setCursor(Qt::PointingHandCursor);
    ui->cancelButton->setCursor(Qt::PointingHandCursor);
    ui->doneButton2->setCursor(Qt::PointingHandCursor);
    ui->cancelButton2->setCursor(Qt::PointingHandCursor);
    ui->syncWithMobileButton->setCursor(Qt::PointingHandCursor);
    
    // Set default display state
    ui->stackedWidget->setCurrentIndex(0);    
    setValid(ui->newAccountName, true);
    ui->cancelButton2->setVisible(false);
    
    // Set default keyboard focus
    ui->newAccountName->setFocus();
    
    // Connect signals.
    connect(ui->doneButton, SIGNAL(clicked()), this, SLOT(addAccount()));
    connect(ui->doneButton2, SIGNAL(clicked()), this, SIGNAL(addAccountMobile()));
    connect(ui->cancelButton, SIGNAL(clicked()), this, SIGNAL(cancel()));
    connect(ui->cancelButton2, SIGNAL(clicked()), this, SLOT(cancelMobile()));
    connect(ui->syncWithMobileButton, SIGNAL(clicked()), this, SLOT(connectToMobile()));
    connect(ui->newAccountName, SIGNAL(textEdited(QString)), this, SLOT(valueChanged()));
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
        newAccount = pwalletMain->GenerateNewAccount(ui->newAccountName->text().toStdString(), AccountType::Normal, AccountSubType::Mobi);
        LOCK(pwalletMain->cs_wallet);
        {
            int64_t currentTime = newAccount->getEarliestPossibleCreationTime();
        
            std::string payoutAddress;
            CReserveKey reservekey(pwalletMain, newAccount, KEYCHAIN_CHANGE);
            CPubKey vchPubKey;
            if (!reservekey.GetReservedKey(vchPubKey))
                return;
            payoutAddress = CBitcoinAddress(vchPubKey.GetID()).ToString();
                
            QString qrString = QString::fromStdString("guldensync:" + CBitcoinSecretExt(*newAccount->GetAccountMasterPrivKey()).ToString( QString::number(currentTime).toStdString(), payoutAddress ) );
            ui->scanQRCode->setCode(qrString);
            
            disconnect(this, SLOT( showSyncQr() ));
        }
    }
}

void NewAccountDialog::addAccount()
{
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
