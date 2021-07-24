// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "witnessdurationwidget.h"
#include "alert.h"
#include "GuldenGUI.h"
//#include "wallet.h"
#include "validation/witnessvalidation.h"
//#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "witnessutil.h"
#include "wallet/witness_operations.h"
//#include "coincontrol.h"
#include <qt/forms/ui_witnessdurationwidget.h>

WitnessDurationWidget::WitnessDurationWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WitnessDurationWidget),
    nAmount(0),
    nMinDurationInBlocks(0),
    nRequiredWeight(gMinimumWitnessWeight)
{
    ui->setupUi(this);

    ui->pow2LockFundsSlider->setMinimum(gMinimumWitnessLockDays);
    ui->pow2LockFundsSlider->setMaximum(gMaximumWitnessLockDays);
    ui->pow2LockFundsSlider->setCursor(Qt::PointingHandCursor);
    setDuration(MinimumWitnessLockLength());

    connect(ui->pow2LockFundsSlider, SIGNAL(valueChanged(int)), this, SLOT(durationValueChanged(int)));
}

WitnessDurationWidget::~WitnessDurationWidget()
{
    delete ui;
}

void WitnessDurationWidget::configure(CAmount lockingAmount, int minDurationInBlocks, int64_t minimumWeight)
{
    nAmount = lockingAmount;
    nMinDurationInBlocks = minDurationInBlocks;
    nRequiredWeight = std::max(minimumWeight, int64_t(gMinimumWitnessWeight));
    setDuration(nMinDurationInBlocks);
}

void WitnessDurationWidget::setAmount(CAmount lockingAmount)
{
    nAmount = lockingAmount;
    update();
}

int WitnessDurationWidget::duration()
{
    return nDuration;
}

void WitnessDurationWidget::setDuration(int newDuration)
{
    int effectiveMinimumDuration = std::max(nMinDurationInBlocks, int(MinimumWitnessLockLength()));
    int effectiveMaximumDuration = MaximumWitnessLockLength();
    nDuration = std::clamp(newDuration, effectiveMinimumDuration, effectiveMaximumDuration);
    ui->pow2LockFundsSlider->setValue(nDuration / DailyBlocksTarget());
    update();
}

void WitnessDurationWidget::durationValueChanged(int newValueDays)
{
    int newValue = newValueDays * DailyBlocksTarget();
    int effectiveMinimumDuration = std::max(nMinDurationInBlocks, int(MinimumWitnessLockLength()));
    int effectiveMaximumDuration = MaximumWitnessLockLength();
    if (std::clamp(newValue, effectiveMinimumDuration, effectiveMaximumDuration) != newValue)
        setDuration(std::clamp(newValue, effectiveMinimumDuration, effectiveMaximumDuration));
    else {
        nDuration = newValue;
        update();
    }
}

#define WITNESS_SUBSIDY 15

void WitnessDurationWidget::update()
{
    setValid(ui->pow2LockFundsInfoLabel, true);

    if (nAmount < CAmount(gMinimumWitnessAmount*COIN))
    {
        ui->pow2LockFundsInfoLabel->setText(tr("A minimum amount of %1 is required.").arg(gMinimumWitnessAmount));
        return;
    }

    int newValue = ui->pow2LockFundsSlider->value();

    int nDays = newValue;
    int nEarnings = 0;

    uint64_t duration = nDays * DailyBlocksTarget();
    uint64_t networkWeight = GetNetworkWeight();
    const auto optimalAmounts = optimalWitnessDistribution(nAmount, duration, networkWeight);
    int64_t nOurWeight = combinedWeight(optimalAmounts, chainActive.Height(), duration);

    if (nOurWeight < nRequiredWeight)
    {
        ui->pow2LockFundsInfoLabel->setText(tr("A minimum weight of %1 is required, but selected weight is only %2. Please increase the amount or lock time for a larger weight.").arg(nRequiredWeight).arg(nOurWeight));
        return;
    }

    double witnessProbability = witnessFraction(optimalAmounts, chainActive.Height(), duration, networkWeight);
    double fBlocksPerDay = DailyBlocksTarget() * witnessProbability;

    nEarnings = fBlocksPerDay * nDays * WITNESS_SUBSIDY;

    float fPercent = (fBlocksPerDay * 30 * WITNESS_SUBSIDY)/((nAmount/COIN))*100;

    QString sSecondTimeUnit = daysToHuman(nDays);
    
    QString partsText = "";
    if (optimalAmounts.size() > 1)
    {
        partsText.append(tr("Parts"));
        partsText.append((": "));
        partsText.append(QString::number(optimalAmounts.size()));
    }

    ui->pow2LockFundsInfoLabel->setText(tr("Funds will be locked for %1 days (%2). It will not be possible under any circumstances to spend or move these funds for the duration of the lock period.\n\nEstimated earnings: %3 (%4% per month)\nWitness weight: %5\n%6")
    .arg(nDays)
    .arg(sSecondTimeUnit)
    .arg(nEarnings)
    .arg(QString::number(fPercent, 'f', 2).replace(".00",""))
    .arg(nOurWeight)
    .arg(partsText)
    );

    QWidget::update();
}

uint64_t WitnessDurationWidget::GetNetworkWeight()
{
    static int64_t nNetworkWeight = gStartingWitnessNetworkWeightEstimate;
    if (chainActive.Tip())
    {
        static uint64_t lastUpdate = 0;
        // Only check this once a minute, no need to be constantly updating.
        if (GetTimeMillis() - lastUpdate > 60000)
        {
            LOCK(cs_main);

            lastUpdate = GetTimeMillis();
            if (IsPow2WitnessingActive(chainActive.TipPrev()->nHeight))
            {
                CGetWitnessInfo witnessInfo = GetWitnessInfoWrapper();
                nNetworkWeight = witnessInfo.nTotalWeightEligibleRaw;
            }
        }
    }
    return nNetworkWeight;
}
