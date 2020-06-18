// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "rotatewitnessdialog.h"
#include <qt/forms/ui_rotatewitnessdialog.h>

#include <stdexcept>
#include "wallet/wallet.h"
#include "gui.h"
#include "wallet/witness_operations.h"
#include "units.h"
#include "witnessutil.h"
#include "walletmodel.h"
#include "optionsmodel.h"

RotateWitnessDialog::RotateWitnessDialog(WalletModel* walletModel_, const QStyle *_platformStyle, QWidget *parent)
: QFrame( parent )
, ui( new Ui::RotateWitnessDialog )
, platformStyle( _platformStyle )
, walletModel(walletModel_)
{
    ui->setupUi(this);

    // minumium required is tx fee, 1 should do it
    ui->fundingSelection->setWalletModel(walletModel, 1 * COIN);

    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    connect(ui->confirmButton, SIGNAL(clicked()), this, SLOT(confirmClicked()));
}

RotateWitnessDialog::~RotateWitnessDialog()
{
    delete ui;
}

void RotateWitnessDialog::cancelClicked()
{
    LOG_QT_METHOD;

    Q_EMIT dismiss(this);
}

void RotateWitnessDialog::confirmClicked()
{
    LOG_QT_METHOD;

    // Format confirmation message
    QString questionString = tr("Are you sure you want to rotate the witness key?");

    CAccount* fundingAccount = ui->fundingSelection->selectedAccount();
    if (!fundingAccount) {
        GUI::createDialog(this, tr("No funding account selected"), tr("Okay"), QString(""), 400, 180)->exec();
        return;
    }

    pactiveWallet->BeginUnlocked(_("Wallet unlock required to rotate witness key"), [=](){

        try {
            LOCK2(cs_main, pactiveWallet->cs_wallet);
            CAccount* witnessAccount = pactiveWallet->activeAccount;
            rotatewitnessaccount(pactiveWallet,
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
