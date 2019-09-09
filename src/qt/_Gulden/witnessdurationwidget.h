// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef WITNESSDURATIONWIDGET_H
#define WITNESSDURATIONWIDGET_H

#include <QWidget>
#include "amount.h"

namespace Ui {
class WitnessDurationWidget;
}

class WitnessDurationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WitnessDurationWidget(QWidget *parent = nullptr);
    ~WitnessDurationWidget();

    void setAmount(CAmount lockingAmount);

    /** Configuration.
     * @param minDurationInBlocks, extra minimum lock duration to use when extending a witness
     */
    void configure(CAmount lockingAmount, int minDurationInBlocks, int64_t minimumWeight);

    int duration();

private:
    Ui::WitnessDurationWidget *ui;
    CAmount nAmount;
    int nMinDurationInBlocks;
    int nDuration;
    int64_t nRequiredWeight;
    void setDuration(int newDuration);
    uint64_t GetNetworkWeight();

private Q_SLOTS:
    void durationValueChanged(int newValue);
    void update();
};

#endif // WITNESSDURATIONWIDGET_H
