// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "importprivkeydialog.h"
#include <qt/_Gulden/forms/ui_importprivkeydialog.h>

#include "guiutil.h"
#include "base58.h"

#include <QMessageBox>
#include <QPushButton>
#include "_Gulden/GuldenGUI.h"

ImportPrivKeyDialog::ImportPrivKeyDialog(QWidget *parent)
: QDialog(parent)
, ui(new Ui::ImportPrivKeyDialog)
{
    ui->setupUi(this);

    GUIUtil::setupPrivKeyWidget(ui->privKeyEdit, this);

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

ImportPrivKeyDialog::~ImportPrivKeyDialog()
{
    delete ui;
}



void ImportPrivKeyDialog::accept()
{
    CGuldenSecret vchSecret;
    bool fGood = vchSecret.SetString(getPrivKey().c_str());

    if (!fGood)
        return;

    QDialog::accept();
}

SecureString ImportPrivKeyDialog::getPrivKey() const
{
    return ui->privKeyEdit->text().toStdString().c_str();
}


