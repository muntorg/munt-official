// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include "viewaddressdialog.h"
#include <qt/_Gulden/forms/ui_viewaddressdialog.h>

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "units.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "walletmodel.h"
#include "wallet/wallet.h"

#include <QAction>
#include <QCursor>
#include <QItemSelection>
#include <QMessageBox>
#include <QScrollBar>
#include <QTextDocument>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QToolTip>

#include "GuldenGUI.h"

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

ViewAddressDialog::ViewAddressDialog(const QStyle* _platformStyle, QWidget* parent)
: QDialog( parent )
, ui( new Ui::ViewAddressDialog )
, platformStyle( _platformStyle )
{
    ui->setupUi(this);

    ui->accountCopyToClipboardButton->setCursor(Qt::PointingHandCursor);
    ui->accountCopyToClipboardButton->setTextFormat( Qt::RichText );
    ui->accountCopyToClipboardButton->setText( GUIUtil::fontAwesomeRegular("\uf0c5") );
    ui->accountCopyToClipboardButton->setContentsMargins(0, 0, 0, 0);

    connect(ui->accountCopyToClipboardButton, SIGNAL(clicked()), this, SLOT(copyAddressToClipboard()));
    //connect(ui->accountSaveQRButton, SIGNAL(clicked()), this, SLOT(saveQRAsImage()));

    updateAddress("");
}

void ViewAddressDialog::updateAddress(const QString& address)
{
    accountAddress = address;
    bool showAddress = true;
    if (accountAddress.isEmpty())
    {
        accountAddress = tr("No valid witness address has been generated for this account");
        showAddress = false;
    }
    ui->addressQRImage->setVisible(showAddress);
    ui->accountCopyToClipboardButton->setVisible(showAddress);

    ui->accountAddress->setText(accountAddress);
    updateQRCode(accountAddress);
}

void ViewAddressDialog::setActiveAccount(CAccount* account)
{
    currentAccount = account;
}

void ViewAddressDialog::activeAccountChanged(CAccount* activeAccount)
{
    LogPrintf("ViewAddressDialog::activeAccountChanged\n");

    setActiveAccount(activeAccount);
}

void ViewAddressDialog::updateQRCode(const QString& sAddress)
{
    QString uri = QString("Gulden:") + sAddress;
    if(!uri.isEmpty())
    {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH)
        {
            ui->addressQRImage->setText(tr("Resulting URI too long, try to reduce the text for label / message."));
        }
        else
        {
            ui->addressQRImage->setCode(uri);
        }
    }
}

void ViewAddressDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    connect(model, SIGNAL(activeAccountChanged(CAccount*)), this, SLOT(activeAccountChanged(CAccount*)));
    activeAccountChanged(model->getActiveAccount());

    if(model && model->getOptionsModel())
    {
    }
}

ViewAddressDialog::~ViewAddressDialog()
{
    delete ui;
}

void ViewAddressDialog::copyAddressToClipboard()
{
    GUIUtil::setClipboard(accountAddress);
    QToolTip::showText(ui->accountCopyToClipboardButton->mapToGlobal(QPoint(0,0)), tr("Address copied to clipboard"),ui->accountCopyToClipboardButton);
}


/*void ViewAddressDialog::saveQRAsImage()
{
    if (ui->receiveCoinsStackedWidget->currentIndex() == 0)
    {
        ui->addressQRImage->saveImage();
    }
    else if (ui->receiveCoinsStackedWidget->currentIndex() == 3)
    {
        ui->requestQRImage->saveImage();
    }
}*/
