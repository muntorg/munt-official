// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "fundwitnessdialog.h"
#include <qt/forms/ui_fundwitnessdialog.h>

#include <stdexcept>
#include "wallet/wallet.h"
#include "gui.h"
#include "wallet/witness_operations.h"
#include "units.h"
#include "witnessutil.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "consensus/validation.h"

FundWitnessDialog::FundWitnessDialog(CAmount minimumFunding, CAmount lockedAmount_, int durationRemaining, int64_t minimumWeight, WalletModel* walletModel_, const QStyle *_platformStyle, QWidget *parent)
: QFrame( parent )
, ui( new Ui::FundWitnessDialog )
, platformStyle( _platformStyle )
, walletModel(walletModel_)
, lockedAmount(lockedAmount_)
{
    ui->setupUi(this);

    ui->fundingSelection->setWalletModel(walletModel, minimumFunding);

    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    connect(ui->extendButton, SIGNAL(clicked()), this, SLOT(extendClicked()));
    connect(ui->fundButton, SIGNAL(clicked()), this, SLOT(fundClicked()));
    connect(ui->payAmount, SIGNAL(amountChanged()), this, SLOT(amountFieldChanged()));

    ui->payAmount->setOptionsModel(walletModel->getOptionsModel());
    ui->payAmount->setDisplayMaxButton(false);
    ui->payAmount->setAmount(lockedAmount);
    ui->lockDuration->configure(lockedAmount, durationRemaining, minimumWeight);
}

FundWitnessDialog::FundWitnessDialog(CAmount lockedAmount_, int durationRemaining, int64_t minimumWeight, WalletModel* walletModel_, const QStyle *_platformStyle, QWidget *parent)
    : FundWitnessDialog(1 * COIN, lockedAmount_, durationRemaining, minimumWeight, walletModel_, _platformStyle, parent)
{
    ui->extendButton->setVisible(true);
    ui->fundButton->setVisible(false);
    ui->labelExtendDescription->setText(tr("Extend a witness to increase amount and/or locking duration. A funding account is needed to provide the transaction fee, even if the amount is not increased."));
}

FundWitnessDialog::FundWitnessDialog(WalletModel* walletModel_, const QStyle *platformStyle, QWidget *parent)
    : FundWitnessDialog(gMinimumWitnessAmount * COIN, 0, 0, 0, walletModel_, platformStyle, parent)
{
    ui->extendButton->setVisible(false);
    ui->fundButton->setVisible(true);
    ui->labelExtendDescription->setText(tr("Fund your witness to start witnessing and earn rewards."));

    // Initial funding dialog is shown automatically when an empty witness account is selected and does not need the cancel button
    ui->cancelButton->setVisible(false);
}

FundWitnessDialog::~FundWitnessDialog()
{
    delete ui;
}

void FundWitnessDialog::cancelClicked()
{
    LOG_QT_METHOD;

    Q_EMIT dismiss(this);
}

void FundWitnessDialog::extendClicked()
{
    LOG_QT_METHOD;

    // Format confirmation message
    QString questionString = tr("Are you sure you want to extend the witness?");
    questionString.append("<br /><br />");
    int days = ui->lockDuration->duration() / DailyBlocksTarget();
    questionString.append(tr("%1 will be locked for %2 days (%3).")
                          .arg(GuldenUnits::formatHtmlWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), ui->payAmount->amount()))
                          .arg(days)
                          .arg(daysToHuman(days)));
    questionString.append("<br /><br />");
    questionString.append(tr("It will not be possible under any circumstances to spend or move these funds for the duration of the lock period."));

    if(QDialog::Accepted == GUI::createDialog(this, questionString, tr("Extend witness"), tr("Cancel"), 600, 360, "ExtendWitnessConfirmationDialog")->exec())
    {
        // selected fundingAccount
        CAccount* fundingAccount = ui->fundingSelection->selectedAccount();
        if (!fundingAccount) {
            GUI::createDialog(this, tr("No funding account selected"), tr("Okay"), QString(""), 400, 180)->exec();
            return;
        }

        pactiveWallet->BeginUnlocked(_("Wallet unlock required to extend witness"), [=](){

            try {
                LOCK2(cs_main, pactiveWallet->cs_wallet);
                CAccount* witnessAccount = pactiveWallet->activeAccount;
                extendwitnessaccount(pactiveWallet,
                                     fundingAccount,
                                     witnessAccount,
                                     ui->payAmount->amount(),
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

void FundWitnessDialog::fundClicked()
{
    LOG_QT_METHOD;

    // Format confirmation message
    QString questionString = tr("Are you sure you want to fund the witness?");
    questionString.append("<br /><br />");
    int days = ui->lockDuration->duration() / DailyBlocksTarget();
    questionString.append(tr("%1 will be locked for %2 days (%3).")
                              .arg(GuldenUnits::formatHtmlWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), ui->payAmount->amount()))
                              .arg(days)
                              .arg(daysToHuman(days)));
    questionString.append("<br /><br />");
    questionString.append(tr("It will not be possible under any circumstances to spend or move these funds for the duration of the lock period."));

    if(QDialog::Accepted == GUI::createDialog(this, questionString, tr("Fund witness"), tr("Cancel"), 600, 360, "FundWitnessConfirmationDialog")->exec())
    {
        // selected fundingAccount
        CAccount* fundingAccount = ui->fundingSelection->selectedAccount();
        if (!fundingAccount) {
            GUI::createDialog(this, tr("No funding account selected"), tr("Okay"), QString(""), 400, 180)->exec();
            return;
        }

        pactiveWallet->BeginUnlocked(_("Wallet unlock required to fund witness"), [=](){

            try
            {
                LOCK2(cs_main, pactiveWallet->cs_wallet);
                CAccount* witnessAccount = pactiveWallet->activeAccount;
                fundwitnessaccount(pactiveWallet,
                                     fundingAccount,
                                     witnessAccount,
                                     ui->payAmount->amount(),
                                     ui->lockDuration->duration(),
                                     false,
                                     nullptr, nullptr); // ignore result params

                // request dismissal only when succesful
                Q_EMIT dismiss(this);

            }
            catch (std::runtime_error& e)
            {
                GUI::createDialog(this, e.what(), tr("Okay"), QString(""), 400, 180)->exec();
            }

            pactiveWallet->EndUnlocked();
        });
    }
}

void FundWitnessDialog::amountFieldChanged()
{
    CAmount newAmount = ui->payAmount->amount();
    ui->lockDuration->setAmount(newAmount);
}
