// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "optimizewitnessdialog.h"
#include <qt/forms/ui_optimizewitnessdialog.h>

#include <stdexcept>
#include "wallet/wallet.h"
#include "gui.h"
#include "wallet/witness_operations.h"
#include "units.h"
#include "witnessutil.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "validation/witnessvalidation.h"

OptimizeWitnessDialog::OptimizeWitnessDialog(WalletModel* walletModel_, const QStyle *_platformStyle, QWidget *parent)
: QFrame( parent )
, ui( new Ui::OptimizeWitnessDialog )
, platformStyle( _platformStyle )
, walletModel(walletModel_)
{
    ui->setupUi(this);

    // minumium required is tx fee, 1 should do it
    ui->fundingSelection->setWalletModel(walletModel, 1 * COIN);

    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    connect(ui->confirmButton, SIGNAL(clicked()), this, SLOT(confirmClicked()));
}

OptimizeWitnessDialog::~OptimizeWitnessDialog()
{
    delete ui;
}

void OptimizeWitnessDialog::cancelClicked()
{
    LOG_QT_METHOD;

    Q_EMIT dismiss(this);
}

void OptimizeWitnessDialog::confirmClicked()
{
    LOG_QT_METHOD;

    QString questionString = tr("Are you sure you want to optimize the witness parts?");

    CAccount* fundingAccount = ui->fundingSelection->selectedAccount();
    if (!fundingAccount)
    {
        GUI::createDialog(this, tr("No funding account selected"), tr("Okay"), QString(""), 400, 180)->exec();
        return;
    }

    auto performOptimise = [=]()
    {
        try
        {
            LOCK2(cs_main, pactiveWallet->cs_wallet);
            CAccount* witnessAccount = pactiveWallet->activeAccount;
            CGetWitnessInfo witnessInfo = GetWitnessInfoWrapper();
            auto [currentDistribution, duration, totalAmount] = witnessDistribution(pactiveWallet, witnessAccount);

            auto optimalDistribution = optimalWitnessDistribution(totalAmount, duration, witnessInfo.nTotalWeightEligibleRaw);
            
            if (currentDistribution == optimalDistribution || (currentDistribution.size() == 1 && optimalDistribution.size() == 1))
            {
                GUI::createDialog(this, tr("Account is already optimal"), tr("Okay"), QString(""), 400, 180)->exec();
                return;
            }
            
            redistributewitnessaccount(pactiveWallet, fundingAccount, witnessAccount, optimalDistribution, nullptr, nullptr); // ignore result params           

            // request dismissal only when succesful
            Q_EMIT dismiss(this);

        }
        catch (std::runtime_error& e)
        {
            GUI::createDialog(this, e.what(), tr("Okay"), QString(""), 400, 180)->exec();
        }
        pactiveWallet->EndUnlocked();
    };
    pactiveWallet->BeginUnlocked(_("Wallet unlock required to optimize witness parts"), performOptimise);
}
