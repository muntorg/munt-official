// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "extendwitnessdialog.h"
#include <qt/_Gulden/forms/ui_extendwitnessdialog.h>

#include <stdexcept>
#include "wallet/wallet.h"
#include "gui.h"
#include "wallet/witness_operations.h"

#define LOG_QT_METHOD LogPrint(BCLog::QT, "%s\n", __PRETTY_FUNCTION__)

ExtendWitnessDialog::ExtendWitnessDialog(CAmount lockedAmount_, int durationRemaining, int64_t minimumWeight, WalletModel* walletModel, const QStyle *_platformStyle, QWidget *parent)
: QFrame( parent )
, ui( new Ui::ExtendWitnessDialog )
, platformStyle( _platformStyle )
, lockedAmount(lockedAmount_)
{
    ui->setupUi(this);

    // minumium required is tx fee, 1 should do it
    ui->fundingSelection->setWalletModel(walletModel, 1 * COIN);

    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    connect(ui->extendButton, SIGNAL(clicked()), this, SLOT(extendClicked()));

    ui->lockDuration->configure(lockedAmount, durationRemaining, minimumWeight);
}

ExtendWitnessDialog::~ExtendWitnessDialog()
{
    delete ui;
}

void ExtendWitnessDialog::cancelClicked()
{
    LOG_QT_METHOD;

    Q_EMIT dismiss(this);
}

void ExtendWitnessDialog::extendClicked()
{
    LOG_QT_METHOD;

    if(QDialog::Accepted == GUI::createDialog(this, "Confirm extending", tr("Extend"), tr("Cancel"), 600, 360, "ExtendWitnessConfirmationDialog")->exec())
    {
        // selected fundingAccount
        CAccount* fundingAccount = ui->fundingSelection->selectedAccount();
        if (!fundingAccount)
            GUI::createDialog(this, tr("No funding account selected"), tr("Okay"), QString(""), 400, 180)->exec();

        pactiveWallet->BeginUnlocked(_("Wallet unlock required to extend witness"), [=](){

            try {
                LOCK2(cs_main, pactiveWallet->cs_wallet);
                CAccount* witnessAccount = pactiveWallet->activeAccount;
                // TODO: fill actual parameters
                extendwitnessaccount(pactiveWallet,
                                     fundingAccount,
                                     witnessAccount,
                                     lockedAmount, // TODO: add in additional locking amount
                                     ui->lockDuration->duration(),
                                     nullptr, nullptr); // ignore result params

                // request dismissal only when succesful
                Q_EMIT dismiss(this);

            } catch (std::runtime_error& e) {
                GUI::createDialog(this, e.what(), tr("Okay"), QString(""), 400, 180)->exec();
            }

            pactiveWallet->EndUnlocked();
        });
    }
}
