// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "accountsettingsdialog.h"
#include <qt/_Gulden/forms/ui_accountsettingsdialog.h>

#include "account.h"
#include "wallet/wallet.h"

#include <QDialogButtonBox>
#include <QDialog>
#include <QPushButton>
#include "GuldenGUI.h"
#include "walletmodel.h"

AccountSettingsDialog::AccountSettingsDialog(const PlatformStyle *platformStyle, QWidget *parent, CAccount* activeAccount, WalletModel* model)
: QFrame(parent)
, walletModel(model)
, ui(new Ui::AccountSettingsDialog)
, platformStyle(platformStyle)
, activeAccount( NULL )
{
    ui->setupUi(this);
    
    // Setup object names for styling
    ui->buttonDone->setObjectName("doneButton");
    ui->buttonDeleteAccount->setObjectName("deleteButton");
    ui->frameAccountSettings->setObjectName("frameAccountSettings");
    setObjectName("dialogAccountSettings");
    
    // Zero out all margins so that we can handle whitespace in stylesheet instead.
    ui->labelChangeAccountName->setContentsMargins( 0, 0, 0, 0 );
    ui->lineEditChangeAccountName->setContentsMargins( 0, 0, 0, 0 );
    ui->labelChangeAccountName->setContentsMargins( 0, 0, 0, 0 );
    ui->addressQRImage->setContentsMargins( 0, 0, 0, 0 );

       
    // Hand cursor for clickable elements.
    ui->buttonDeleteAccount->setCursor( Qt::PointingHandCursor );
    ui->buttonDone->setCursor( Qt::PointingHandCursor );
    
    // Hide sync-with-mobile, we only show it for mobile accounts.
    ui->frameSyncWithMobile->setVisible(false);
    
    
    // Connect signals
    connect(ui->buttonDeleteAccount, SIGNAL( clicked() ), this, SLOT( deleteAccount() ));
    connect(ui->buttonDone, SIGNAL( clicked() ), this, SLOT( applyChanges() ));
    
    // Set initial state.
    activeAccountChanged(activeAccount);
    
    LogPrintf("AccountSettingsDialog::AccountSettingsDialog\n");
}

void AccountSettingsDialog::activeAccountChanged(CAccount* account)
{
    LogPrintf("AccountSettingsDialog::activeAccountChanged\n");
    disconnect(this, SLOT( showSyncQr() ));
    
    activeAccount = account;
    
    //Should never happen - but just in case
    if (!account)
    {
        ui->lineEditChangeAccountName->setVisible(false);
        ui->frameSyncWithMobile->setVisible(false);
        return;
    }
    else
    {
        ui->lineEditChangeAccountName->setVisible(true);
    }
    
    if (account->m_SubType == AccountSubType::Mobi)
    {
        ui->frameSyncWithMobile->setVisible(true);
        
        ui->addressQRImage->setText(tr("Click here to make QR code visible.\nWARNING: please ensure that you are the only person who can see this QR code as otherwise it could be used to access your funds."));
        connect(ui->addressQRImage, SIGNAL( clicked() ), this, SLOT( showSyncQr() ));
    }
    else
    {
        ui->frameSyncWithMobile->setVisible(false);
    }
    
    if (!ui->lineEditChangeAccountName->text().isEmpty())
    {
        //fixme: GULDEN - prompt user to save changes?
    }
    
    ui->lineEditChangeAccountName->setText("");
    ui->lineEditChangeAccountName->setPlaceholderText( QString::fromStdString(account->getLabel()) );
}


void AccountSettingsDialog::showSyncQr()
{
    int64_t currentTime = activeAccount->getEarliestPossibleCreationTime();
    
    LOCK(pwalletMain->cs_wallet);
    std::string payoutAddress;
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (ctx.isValid())
    {
        CReserveKey reservekey(pwalletMain, activeAccount, KEYCHAIN_CHANGE);
        CPubKey vchPubKey;
        if (!reservekey.GetReservedKey(vchPubKey))
            return;
        payoutAddress = CBitcoinAddress(vchPubKey.GetID()).ToString();
    
        QString qrString = QString::fromStdString("guldensync:" + CBitcoinSecretExt(*(static_cast<CAccountHD*>(activeAccount)->GetAccountMasterPrivKey())).ToString( QString::number(currentTime).toStdString(), payoutAddress ) );
        ui->addressQRImage->setCode(qrString);
    
        disconnect(this, SLOT( showSyncQr() ));
    }
}

void AccountSettingsDialog::applyChanges()
{
    if (activeAccount)
    {
        if (!ui->lineEditChangeAccountName->text().isEmpty())
        {
            //fixme: GULDEN - multiwallet.
            pwalletMain->changeAccountName(activeAccount, ui->lineEditChangeAccountName->text().toStdString());
            ui->lineEditChangeAccountName->setText(QString(""));
        }
    }
    Q_EMIT dismissAccountSettings();
}

//fixme: GULDEN - Make this configurable or more intelligent in some way?
#define MINIMUM_VALUABLE_AMOUNT 1000000000 
void AccountSettingsDialog::deleteAccount()
{
    if (activeAccount)
    {
        CAmount balance = pwalletMain->GetAccountBalance(activeAccount->getUUID(), 0, ISMINE_SPENDABLE, true );
        if (balance > MINIMUM_VALUABLE_AMOUNT)
        {
            QString message = tr("Account not empty, please first empty your account before trying to delete it.");
            QDialog* d = GuldenGUI::createDialog(this, message, tr("Okay"), QString(""), 400, 180);
            d->exec();
        }
        else
        {
            QString message = tr("Are you sure you want to delete %1 from your account list?\nThe account will continue to be monitored and will be restored should it receive new funds in future.").arg( QString::fromStdString(activeAccount->getLabel()) );
            QDialog* d = GuldenGUI::createDialog(this, message, tr("Delete account"), tr("Cancel"), 400, 180);
            int result = d->exec();
            if(result == QDialog::Accepted)
            {
                pwalletMain->deleteAccount(activeAccount);
            }
        }
    }
    Q_EMIT dismissAccountSettings();
}

AccountSettingsDialog::~AccountSettingsDialog()
{
    delete ui;
}
