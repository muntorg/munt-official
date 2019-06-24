// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "witnessdurationwidget.h"
#include <qt/_Gulden/forms/ui_witnessdurationwidget.h>

WitnessDurationWidget::WitnessDurationWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WitnessDurationWidget)
{
    ui->setupUi(this);
}

WitnessDurationWidget::~WitnessDurationWidget()
{
    delete ui;
}
