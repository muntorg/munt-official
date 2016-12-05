// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "editaddressdialog.h"
#include "ui_editaddressdialog.h"

#include "addresstablemodel.h"
#include "guiutil.h"

#include <QDataWidgetMapper>
#include <QMessageBox>
#include <QPushButton>
#include "_Gulden/GuldenGUI.h"

EditAddressDialog::EditAddressDialog(Mode mode, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::EditAddressDialog)
    , mapper(0)
    , mode(mode)
    , model(0)
{
    ui->setupUi(this);

    GUIUtil::setupAddressWidget(ui->addressEdit, this);

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

    setMinimumSize(400, 200);

    switch (mode) {
    case NewReceivingAddress:
        setWindowTitle(tr("New receiving address"));
        ui->addressEdit->setEnabled(false);
        break;
    case NewSendingAddress:
        setWindowTitle(tr("New sending address"));
        break;
    case EditReceivingAddress:
        setWindowTitle(tr("Edit receiving address"));
        ui->addressEdit->setEnabled(false);
        break;
    case EditSendingAddress:
        setWindowTitle(tr("Edit sending address"));
        break;
    }

    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
}

EditAddressDialog::~EditAddressDialog()
{
    delete ui;
}

void EditAddressDialog::setModel(AddressTableModel* model)
{
    this->model = model;
    if (!model)
        return;

    mapper->setModel(model);
    mapper->addMapping(ui->labelEdit, AddressTableModel::Label);
    mapper->addMapping(ui->addressEdit, AddressTableModel::Address);
}

void EditAddressDialog::loadRow(int row)
{
    mapper->setCurrentIndex(row);
}

bool EditAddressDialog::saveCurrentRow()
{
    if (!model)
        return false;

    switch (mode) {
    case NewReceivingAddress:
    case NewSendingAddress:
        address = model->addRow(
            mode == NewSendingAddress ? AddressTableModel::Send : AddressTableModel::Receive,
            ui->labelEdit->text(),
            ui->addressEdit->text());
        break;
    case EditReceivingAddress:
    case EditSendingAddress:
        if (mapper->submit()) {
            address = ui->addressEdit->text();
        }
        break;
    }
    return !address.isEmpty();
}

void EditAddressDialog::accept()
{
    if (!model)
        return;

    if (!saveCurrentRow()) {
        switch (model->getEditStatus()) {
        case AddressTableModel::OK:

            break;
        case AddressTableModel::NO_CHANGES:

            break;
        case AddressTableModel::INVALID_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                                 tr("The entered address \"%1\" is not a valid Bitcoin address.").arg(ui->addressEdit->text()),
                                 QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::DUPLICATE_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                                 tr("The entered address \"%1\" is already in the address book.").arg(ui->addressEdit->text()),
                                 QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::WALLET_UNLOCK_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                                  tr("Could not unlock wallet."),
                                  QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::KEY_GENERATION_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                                  tr("New key generation failed."),
                                  QMessageBox::Ok, QMessageBox::Ok);
            break;
        }
        return;
    }
    QDialog::accept();
}

QString EditAddressDialog::getAddress() const
{
    return address;
}

void EditAddressDialog::setAddress(const QString& address)
{
    this->address = address;
    ui->addressEdit->setText(address);
}
