// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "upgradewitnessdialog.h"
#include <qt/_Gulden/forms/ui_upgradewitnessdialog.h>

#include <stdexcept>
#include "wallet/wallet.h"
#include "gui.h"
#include "wallet/witness_operations.h"
#include "units.h"
#include "Gulden/util.h"
#include "walletmodel.h"
#include "optionsmodel.h"

UpgradeWitnessDialog::UpgradeWitnessDialog(WalletModel* walletModel_, const QStyle *_platformStyle, QWidget *parent)
: QFrame( parent )
, ui( new Ui::UpgradeWitnessDialog )
, platformStyle( _platformStyle )
, walletModel(walletModel_)
{
    ui->setupUi(this);

    // minumium required is tx fee, 1 should do it
    ui->fundingSelection->setWalletModel(walletModel, 1 * COIN);

    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    connect(ui->confirmButton, SIGNAL(clicked()), this, SLOT(confirmClicked()));
}

UpgradeWitnessDialog::~UpgradeWitnessDialog()
{
    delete ui;
}

void UpgradeWitnessDialog::cancelClicked()
{
    LOG_QT_METHOD;

    Q_EMIT dismiss(this);
}

void UpgradeWitnessDialog::confirmClicked()
{
    LOG_QT_METHOD;

    // Format confirmation message
    QString questionString = tr("Are you sure you want to upgrade the witness?");

    CAccount* fundingAccount = ui->fundingSelection->selectedAccount();
    if (!fundingAccount) {
        GUI::createDialog(this, tr("No funding account selected"), tr("Okay"), QString(""), 400, 180)->exec();
        return;
    }

    pactiveWallet->BeginUnlocked(_("Wallet unlock required to upgrade witness"), [=](){

        try {
            LOCK2(cs_main, pactiveWallet->cs_wallet);
            CAccount* witnessAccount = pactiveWallet->activeAccount;
            upgradewitnessaccount(pactiveWallet,
                                  fundingAccount,
                                  witnessAccount,
                                  nullptr, nullptr); // ignore result params

            // request dismissal only when succesful
            Q_EMIT dismiss(this);

        } catch (std::runtime_error& e) {
            GUI::createDialog(this, e.what(), tr("Okay"), QString(""), 400, 180)->exec();
        }

        pactiveWallet->EndUnlocked();
    });
}
