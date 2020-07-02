// Copyright (c) 2016-2020 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "accountsettingsdialog.h"
#include <qt/forms/ui_accountsettingsdialog.h>

#include "wallet/account.h"
#include "wallet/wallet.h"
#include <validation/witnessvalidation.h>

#include <QDialogButtonBox>
#include <QDialog>
#include <QPushButton>
#include "gui.h"
#include "walletmodel.h"
#include "rotatewitnessdialog.h"
#include <witnessutil.h>
#include "wallet/witness_operations.h"

AccountSettingsDialog::AccountSettingsDialog(const QStyle *_platformStyle, QWidget *parent, CAccount* _activeAccount, WalletModel* model)
: QStackedWidget(parent)
, walletModel(model)
, ui(new Ui::AccountSettingsDialog)
, platformStyle(_platformStyle)
, activeAccount( NULL )
{
    ui->setupUi(this);

    // Hide sync-with-mobile, we only show it for mobile accounts.
    ui->frameSyncWithMobile->setVisible(false);

    ui->buttonCopy->setVisible(false);
    ui->rotateButton->setVisible(false);

    // Connect signals
    connect(ui->buttonDeleteAccount, SIGNAL(clicked()), this, SLOT(deleteAccount()));
    connect(ui->buttonDone, SIGNAL(clicked()), this, SLOT(applyChanges()));
    connect(ui->buttonCopy, SIGNAL(clicked()), this, SLOT(copyQr()));
    connect(ui->rotateButton, SIGNAL(clicked()), this, SLOT(rotateClicked()));

    // Set initial state.
    activeAccountChanged(_activeAccount);

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

    if (account->m_Type == AccountType::Mobi)
    {
        ui->frameSyncWithMobile->setVisible(true);

        ui->labelScanAccountQR->setText(tr("Scan QR to connect to your mobile Gulden app"));

        ui->addressQRImage->setText(tr("Click here to make QR code visible.\nWARNING: please ensure that you are the only person who can see this QR code as otherwise it could be used to access your funds."));
        connect(ui->addressQRImage, SIGNAL( clicked() ), this, SLOT( showSyncQr() ), Qt::UniqueConnection);
    }
    else if (account->IsPoW2Witness() && !account->IsFixedKeyPool())
    {
        ui->frameSyncWithMobile->setVisible(true);

        ui->labelScanAccountQR->setText(tr("Scan QR with a witnessing device to link the device to your wallet"));

        ui->addressQRImage->setText(tr("Click here to make QR code visible.\nWARNING: please ensure that you are the only person who can see this QR code as otherwise it could be used to earn on your behalf and steal your witness earnings."));
        connect(ui->addressQRImage, SIGNAL( clicked() ), this, SLOT( showSyncQr() ), Qt::UniqueConnection);

        ui->rotateButton->setVisible(IsSegSigEnabled(chainActive.TipPrev()));
    }
    else
    {
        ui->frameSyncWithMobile->setVisible(false);
    }


    if (!ui->lineEditChangeAccountName->text().isEmpty())
    {
        //fixme: (FUT) - prompt user to save changes?
    }

    ui->lineEditChangeAccountName->setText("");
    ui->lineEditChangeAccountName->setPlaceholderText( QString::fromStdString(account->getLabel()) );
}


void AccountSettingsDialog::showSyncQr()
{
    int64_t currentTime = activeAccount->getEarliestPossibleCreationTime();

    ui->buttonCopy->setVisible(true);

    LOCK2(cs_main, pactiveWallet->cs_wallet);
    std::string payoutAddress;
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (ctx.isValid())
    {
        QString qrString = "";
        if (activeAccount->IsMobi())
        {
            CReserveKeyOrScript reservekey(pactiveWallet, activeAccount, KEYCHAIN_CHANGE);
            CPubKey vchPubKey;
            if (!reservekey.GetReservedKey(vchPubKey))
                return;
            payoutAddress = CNativeAddress(vchPubKey.GetID()).ToString();

            qrString = QString::fromStdString("guldensync:" + CEncodedSecretKeyExt<CExtKey>(*(static_cast<CAccountHD*>(activeAccount)->GetAccountMasterPrivKey())).SetCreationTime(QString::number(currentTime).toStdString()).SetPayAccount(payoutAddress).ToURIString() );
            ui->addressQRContents->setVisible(false);
        }
        else if(activeAccount->IsPoW2Witness() && !activeAccount->IsFixedKeyPool())
        {
            std::string linkUrl = witnessKeysLinkUrlForAccount(pactiveWallet, activeAccount);
            if (!linkUrl.empty()) {
                qrString = QString::fromStdString(linkUrl);
            }
            else {
                ui->addressQRContents->setText(tr("Please fund the witness account first."));
                ui->addressQRContents->setVisible(true);
                ui->addressQRImage->setVisible(false);
                disconnect(this, SLOT(showSyncQr()));
                return;
            }
            ui->addressQRContents->setVisible(true);
        }
        ui->addressQRImage->setCode(qrString);
        ui->addressQRContents->setText(qrString);

        disconnect(this, SLOT(showSyncQr()));
    }
}

void AccountSettingsDialog::applyChanges()
{
    if (activeAccount)
    {
        if (!ui->lineEditChangeAccountName->text().isEmpty())
        {
            pactiveWallet->changeAccountName(activeAccount, ui->lineEditChangeAccountName->text().toStdString());
            ui->lineEditChangeAccountName->setText(QString(""));
        }
    }
    Q_EMIT dismissAccountSettings();
}

void AccountSettingsDialog::copyQr()
{
    QString copyText = ui->addressQRContents->text();
    GUIUtil::setClipboard(copyText);
}

void AccountSettingsDialog::rotateClicked()
{
    LOG_QT_METHOD;

    pushDialog(new RotateWitnessDialog(walletModel, platformStyle, this));
}

//fixme: (FUT) - Make this configurable or more intelligent in some way?
#define MINIMUM_VALUABLE_AMOUNT 1000000000 
void AccountSettingsDialog::deleteAccount()
{
    if (activeAccount)
    {
        CAmount balance = pactiveWallet->GetBalanceForDepth(0, activeAccount, true, true);
        if (activeAccount->IsPoW2Witness() && activeAccount->IsFixedKeyPool())
        {
            balance = pactiveWallet->GetBalanceForDepth(0, activeAccount, false, true);
        }
        if (!activeAccount->IsReadOnly() && balance > MINIMUM_VALUABLE_AMOUNT)
        {
            QString message = tr("Account not empty, please first empty your account before trying to delete it.");
            QDialog* d = GUI::createDialog(this, message, tr("Okay"), QString(""), 400, 180);
            d->exec();
        }
        else
        {
            bool shouldPurge = false;
            QString message;
            if (activeAccount->IsPoW2Witness() && activeAccount->IsFixedKeyPool())
            {
                shouldPurge = true;
                message = tr("Are you sure you want to delete %1 from your account list?\n").arg( QString::fromStdString(activeAccount->getLabel()) );
            }
            else
            {
                message = tr("Are you sure you want to delete %1 from your account list?\nThe account will continue to be monitored and will be restored should it receive new funds in future.").arg( QString::fromStdString(activeAccount->getLabel()) );
            }
            QDialog* d = GUI::createDialog(this, message, tr("Delete account"), tr("Cancel"), 400, 180);
            int result = d->exec();
            if(result == QDialog::Accepted)
            {
                CWalletDB walletdb(*pactiveWallet->dbw);
                pactiveWallet->deleteAccount(walletdb, activeAccount, shouldPurge);
            }
        }
    }
    Q_EMIT dismissAccountSettings();
}

AccountSettingsDialog::~AccountSettingsDialog()
{
    delete ui;
}

void AccountSettingsDialog::pushDialog(QWidget *dialog)
{
    addWidget(dialog);
    setCurrentWidget(dialog);
    connect( dialog, SIGNAL( dismiss(QWidget*) ), this, SLOT( popDialog(QWidget*)) );
}

void AccountSettingsDialog::popDialog(QWidget* dialog)
{
    int index = indexOf(dialog);
    if (index < 1)
        return;
    setCurrentIndex(index - 1);
    for (int i = count() - 1; i >= index; i--) {
        QWidget* w = widget(i);
        removeWidget(w);
        w->deleteLater();
    }
}

