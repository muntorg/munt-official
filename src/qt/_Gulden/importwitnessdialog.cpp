// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "importwitnessdialog.h"
#include <qt/_Gulden/forms/ui_importwitnessdialog.h>

#include "guiutil.h"
#include "base58.h"

#include <QMessageBox>
#include <QPushButton>
#include "_Gulden/GuldenGUI.h"

ImportWitnessDialog::ImportWitnessDialog(QWidget *parent)
: QDialog(parent)
, ui(new Ui::ImportWitnessDialog)
{
    ui->setupUi(this);

    GUIUtil::setupGuldenURLEntryWidget(ui->privKeyEdit, this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Ok"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setCursor(Qt::PointingHandCursor);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(GULDEN_DIALOG_CONFIRM_BUTTON_STYLE_NOMARGIN);
    ui->buttonBox->button(QDialogButtonBox::Reset)->setText(tr("Cancel"));
    ui->buttonBox->button(QDialogButtonBox::Reset)->setCursor(Qt::PointingHandCursor);
    ui->buttonBox->button(QDialogButtonBox::Reset)->setStyleSheet(GULDEN_DIALOG_CANCEL_BUTTON_STYLE_NOMARGIN);
    QObject::connect(ui->buttonBox->button(QDialogButtonBox::Reset), SIGNAL(clicked()), this, SLOT(reject()));

    QFrame* horizontalLine = new QFrame(this);
    horizontalLine->setFrameStyle(QFrame::HLine);
    horizontalLine->setFixedHeight(1);
    horizontalLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    horizontalLine->setStyleSheet(GULDEN_DIALOG_HLINE_STYLE_NOMARGIN);
    ui->verticalLayout->insertWidget(1, horizontalLine);

    //Need a minimum height otherwise our horizontal line gets hidden.
    setMinimumSize(300,200);
}

ImportWitnessDialog::~ImportWitnessDialog()
{
    delete ui;
}



void ImportWitnessDialog::accept()
{
    //fixme: (2.1) Improve this check.
    bool fGood = ui->privKeyEdit->text().startsWith("gulden://witnesskeys?keys=");

    if (!fGood)
        return;

    QDialog::accept();
}

SecureString ImportWitnessDialog::getWitnessURL() const
{
    return ui->privKeyEdit->text().toStdString().c_str();
}


