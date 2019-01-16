// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "accountsettingsdialog.h"
#include <qt/_Gulden/forms/ui_accountsettingsdialog.h>

#include "account.h"
#include "wallet/wallet.h"
#include <validation/witnessvalidation.h>

#include <QDialogButtonBox>
#include <QDialog>
#include <QPushButton>
#include "gui.h"
#include "walletmodel.h"

#include <Gulden/util.h>

AccountSettingsDialog::AccountSettingsDialog(const QStyle *_platformStyle, QWidget *parent, CAccount* _activeAccount, WalletModel* model)
: QFrame(parent)
, walletModel(model)
, ui(new Ui::AccountSettingsDialog)
, platformStyle(_platformStyle)
, activeAccount( NULL )
{
    ui->setupUi(this);

    // Setup object names for styling
    ui->buttonDone->setObjectName("doneButton");
    ui->buttonCopy->setObjectName("copyButton");
    ui->buttonDeleteAccount->setObjectName("deleteButton");
    ui->frameAccountSettings->setObjectName("frameAccountSettings");
    ui->addressQRContents->setObjectName("addressQRContents");
    setObjectName("dialogAccountSettings");

    // Zero out all margins so that we can handle whitespace in stylesheet instead.
    ui->labelChangeAccountName->setContentsMargins( 0, 0, 0, 0 );
    ui->lineEditChangeAccountName->setContentsMargins( 0, 0, 0, 0 );
    ui->labelChangeAccountName->setContentsMargins( 0, 0, 0, 0 );
    ui->addressQRImage->setContentsMargins( 0, 0, 0, 0 );


    // Hand cursor for clickable elements.
    ui->buttonDeleteAccount->setCursor(Qt::PointingHandCursor);
    ui->buttonDone->setCursor(Qt::PointingHandCursor);
    ui->buttonCopy->setCursor(Qt::PointingHandCursor);

    // Hide sync-with-mobile, we only show it for mobile accounts.
    ui->frameSyncWithMobile->setVisible(false);

    ui->buttonCopy->setVisible(false);

    // Connect signals
    connect(ui->buttonDeleteAccount, SIGNAL(clicked()), this, SLOT(deleteAccount()));
    connect(ui->buttonDone, SIGNAL(clicked()), this, SLOT(applyChanges()));
    connect(ui->buttonCopy, SIGNAL(clicked()), this, SLOT(copyQr()));

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
    }
    else
    {
        ui->frameSyncWithMobile->setVisible(false);
    }

    if (!ui->lineEditChangeAccountName->text().isEmpty())
    {
        //fixme: (2.1) - prompt user to save changes?
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
            payoutAddress = CGuldenAddress(vchPubKey.GetID()).ToString();

            qrString = QString::fromStdString("guldensync:" + CGuldenSecretExt<CExtKey>(*(static_cast<CAccountHD*>(activeAccount)->GetAccountMasterPrivKey())).ToString( QString::number(currentTime).toStdString(), payoutAddress ) );
            ui->addressQRContents->setVisible(false);
        }
        else if(activeAccount->IsPoW2Witness() && !activeAccount->IsFixedKeyPool())
        {
            //fixme: (2.1) HIGH - seperate this and "getwitnessaccountkeys" (RPC) into a seperate helper function instead of duplicating code.
            if (chainActive.Tip())
            {
                std::map<COutPoint, Coin> allWitnessCoins;
                if (getAllUnspentWitnessCoins(chainActive, Params(), chainActive.Tip(), allWitnessCoins))
                {
                    std::string witnessAccountKeys = "";
                    for (const auto& [witnessOutPoint, witnessCoin] : allWitnessCoins)
                    {
                        (unused)witnessOutPoint;
                        CTxOutPoW2Witness witnessDetails;
                        GetPow2WitnessOutput(witnessCoin.out, witnessDetails);
                        if (activeAccount->HaveKey(witnessDetails.witnessKeyID))
                        {
                            CKey witnessPrivKey;
                            if (activeAccount->GetKey(witnessDetails.witnessKeyID, witnessPrivKey))
                            {
                                //fixme: (2.1) - to be 100% correct we should export the creation time of the actual key (where available) and not getEarliestPossibleCreationTime - however getEarliestPossibleCreationTime will do for now.
                                witnessAccountKeys += CGuldenSecret(witnessPrivKey).ToString() + strprintf("#%s", activeAccount->getEarliestPossibleCreationTime());
                                witnessAccountKeys += ":";
                            }
                        }
                    }
                    if (!witnessAccountKeys.empty())
                    {
                        witnessAccountKeys.pop_back();
                        witnessAccountKeys = "gulden://witnesskeys?keys=" + witnessAccountKeys;
                        qrString = QString::fromStdString(witnessAccountKeys);
                    }
                    else
                    {
                        ui->addressQRContents->setText(tr("Please fund the witness account first."));
                        ui->addressQRContents->setVisible(true);
                        ui->addressQRImage->setVisible(false);
                        disconnect(this, SLOT(showSyncQr()));
                        return;
                    }
                }
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

//fixme: (Post-2.1) - Make this configurable or more intelligent in some way?
#define MINIMUM_VALUABLE_AMOUNT 1000000000 
void AccountSettingsDialog::deleteAccount()
{
    if (activeAccount)
    {
        boost::uuids::uuid accountUUID = activeAccount->getUUID();
        CAmount balance = pactiveWallet->GetLegacyBalance(ISMINE_SPENDABLE, 0, &accountUUID);
        if (activeAccount->IsPoW2Witness() && activeAccount->IsFixedKeyPool())
        {
            balance = pactiveWallet->GetBalance(activeAccount, true, false, true); 
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
                pactiveWallet->deleteAccount(activeAccount, shouldPurge);
            }
        }
    }
    Q_EMIT dismissAccountSettings();
}

AccountSettingsDialog::~AccountSettingsDialog()
{
    delete ui;
}
