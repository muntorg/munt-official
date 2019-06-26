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
#include "Gulden/util.h"
//#include "coincontrol.h"
#include <qt/_Gulden/forms/ui_witnessdurationwidget.h>

WitnessDurationWidget::WitnessDurationWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WitnessDurationWidget),
    nAmount(0),
    nMinDurationInBlocks(0)
{
    ui->setupUi(this);

    ui->pow2LockFundsSlider->setMinimum(gMinimumWitnessLockDays);
    ui->pow2LockFundsSlider->setMaximum(gMaximumWitnessLockDays);
    ui->pow2LockFundsSlider->setCursor(Qt::PointingHandCursor);
    setDuration(gMinimumWitnessLockDays * DailyBlocksTarget());

    connect(ui->pow2LockFundsSlider, SIGNAL(valueChanged(int)), this, SLOT(durationValueChanged(int)));
}

WitnessDurationWidget::~WitnessDurationWidget()
{
    delete ui;
}

void WitnessDurationWidget::configure(CAmount lockingAmount, int minDurationInBlocks)
{
    nAmount = lockingAmount;
    nMinDurationInBlocks = minDurationInBlocks;
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
    int effectiveMinimumDuration = std::max(nMinDurationInBlocks, gMinimumWitnessLockDays * DailyBlocksTarget());
    int effectiveMaximumDuration = gMaximumWitnessLockDays * DailyBlocksTarget();
    nDuration = std::clamp(newDuration, effectiveMinimumDuration, effectiveMaximumDuration);
    ui->pow2LockFundsSlider->setValue(nDuration / DailyBlocksTarget());
    update();
}

void WitnessDurationWidget::durationValueChanged(int newValueDays)
{
    int newValue = newValueDays * DailyBlocksTarget();
    int effectiveMinimumDuration = std::max(nMinDurationInBlocks, gMinimumWitnessLockDays * DailyBlocksTarget());
    int effectiveMaximumDuration = gMaximumWitnessLockDays * DailyBlocksTarget();
    if (std::clamp(newValue, effectiveMinimumDuration, effectiveMaximumDuration) != newValue)
        setDuration(std::clamp(newValue, effectiveMinimumDuration, effectiveMaximumDuration));
    else {
        nDuration = newValue;
        update();
    }
}

#define WITNESS_SUBSIDY 20

void WitnessDurationWidget::update()
{
    setValid(ui->pow2LockFundsInfoLabel, true);

    ui->pow2WeightExceedsMaxPercentWarning->setVisible(false);

    if (nAmount < CAmount(gMinimumWitnessAmount*COIN))
    {
        ui->pow2LockFundsInfoLabel->setText(tr("A minimum amount of %1 is required.").arg(gMinimumWitnessAmount));
        return;
    }

    int newValue = ui->pow2LockFundsSlider->value();

    int nDays = newValue;
    float fMonths = newValue/30.0;
    float fYears = newValue/365.0;
    int nEarnings = 0;

    int64_t nOurWeight = GetPoW2RawWeightForAmount(nAmount, nDays * DailyBlocksTarget());

    static int64_t nNetworkWeight = gStartingWitnessNetworkWeightEstimate;
    if (chainActive.Tip())
    {
        static uint64_t lastUpdate = GetTimeMillis();
        // Only check this once a minute, no need to be constantly updating.
        if (GetTimeMillis() - lastUpdate > 60000)
        {
            LOCK(cs_main);

            lastUpdate = GetTimeMillis();
            if (IsPow2WitnessingActive(chainActive.TipPrev(), Params(), chainActive))
            {
                CGetWitnessInfo witnessInfo;
                CBlock block;
                if (!ReadBlockFromDisk(block, chainActive.Tip(), Params()))
                {
                    std::string strErrorMessage = "GuldenSendCoinsEntry::witnessSliderValueChanged Failed to read block from disk";
                    LogPrintf(strErrorMessage.c_str());
                    CAlert::Notify(strErrorMessage, true, true);
                    return;
                }
                if (!GetWitnessInfo(chainActive, Params(), nullptr, chainActive.Tip()->pprev, block, witnessInfo, chainActive.Tip()->nHeight))
                {
                    std::string strErrorMessage = "GuldenSendCoinsEntry::witnessSliderValueChanged Failed to read block from disk";
                    LogPrintf(strErrorMessage.c_str());
                    CAlert::Notify(strErrorMessage, true, true);
                    return;
                }
                nNetworkWeight = witnessInfo.nTotalWeightRaw;
            }
        }
    }


    if (nOurWeight < gMinimumWitnessWeight)
    {
        ui->pow2LockFundsInfoLabel->setText(tr("A minimum weight of %1 is required, but selected weight is only %2. Please increase the amount or lock time for a larger weight.").arg(gMinimumWitnessWeight).arg(nOurWeight));
        return;
    }

    double fWeightPercent = nOurWeight/(double)nNetworkWeight;
    if (fWeightPercent > 0.01)
    {
        ui->pow2WeightExceedsMaxPercentWarning->setVisible(true);
        fWeightPercent = 0.01;
    }

    double fBlocksPerDay = DailyBlocksTarget() * fWeightPercent;
    if (fBlocksPerDay > 0.01 * DailyBlocksTarget())
        fBlocksPerDay = 0.01 * DailyBlocksTarget();

    nEarnings = fBlocksPerDay * nDays * WITNESS_SUBSIDY;

    float fPercent = (fBlocksPerDay * 30 * WITNESS_SUBSIDY)/((nAmount/100000000))*100;


    QString sSecondTimeUnit = "";
    if (fYears > 1)
    {
        sSecondTimeUnit = tr("1 year");
        if (fYears > 1.0)
            sSecondTimeUnit = tr("%1 years").arg(QString::number(fYears, 'f', 2).replace(".00",""));
    }
    else
    {
        sSecondTimeUnit = tr("1 month");
        if (fMonths > 1.0)
            sSecondTimeUnit = tr("%1 months").arg(QString::number(fMonths, 'f', 2).replace(".00",""));
    }

    ui->pow2LockFundsInfoLabel->setText(tr("Funds will be locked for %1 days (%2). It will not be possible under any circumstances to spend or move these funds for the duration of the lock period.\n\nEstimated earnings: %3 (%4% per month)\n\nWitness weight: %5")
    .arg(nDays)
    .arg(sSecondTimeUnit)
    .arg(nEarnings)
    .arg(QString::number(fPercent, 'f', 2).replace(".00",""))
    .arg(nOurWeight)
    );

    QWidget::update();
}
