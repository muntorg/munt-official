// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include "receivecoinsdialog.h"
#include <qt/_Gulden/forms/ui_receivecoinsdialog.h>

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "units.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "receiverequestdialog.h"
#include "recentrequeststablemodel.h"
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

bool ReceiveCoinsDialog::showCopyQRAsImagebutton = true;

ReceiveCoinsDialog::ReceiveCoinsDialog(const QStyle *_platformStyle, QWidget *parent)
: QDialog( parent )
, ui( new Ui::ReceiveCoinsDialog )
, model( 0 )
, platformStyle( _platformStyle )
, buyReceiveAddress( NULL )
, currentAccount( NULL )
{
    ui->setupUi(this);

    ui->accountRequestPaymentButton->setCursor(Qt::PointingHandCursor);
    ui->accountBuyGuldenButton->setCursor(Qt::PointingHandCursor);
    ui->accountSaveQRButton->setCursor(Qt::PointingHandCursor);
    ui->accountCopyToClipboardButton->setCursor(Qt::PointingHandCursor);
    ui->accountCopyToClipboardButton->setTextFormat( Qt::RichText );
    ui->accountCopyToClipboardButton->setText( GUIUtil::fontAwesomeRegular("\uf0c5") );
    ui->accountCopyToClipboardButton->setContentsMargins(0, 0, 0, 0);
    ui->requestCopyToClipboardButton->setCursor(Qt::PointingHandCursor);
    ui->requestCopyToClipboardButton->setTextFormat( Qt::RichText );
    ui->requestCopyToClipboardButton->setText( GUIUtil::fontAwesomeRegular("\uf0c5") );
    ui->requestCopyToClipboardButton->setContentsMargins(0, 0, 0, 0);
    ui->cancelButton->setCursor(Qt::PointingHandCursor);
    ui->closeButton->setCursor(Qt::PointingHandCursor);
    ui->generateRequestButton->setCursor(Qt::PointingHandCursor);
    ui->generateAnotherRequestButton->setCursor(Qt::PointingHandCursor);


    connect(ui->accountCopyToClipboardButton, SIGNAL(clicked()), this, SLOT(copyAddressToClipboard()));
    connect(ui->requestCopyToClipboardButton, SIGNAL(clicked()), this, SLOT(copyAddressToClipboard()));
    connect(ui->accountBuyGuldenButton, SIGNAL(clicked()), this, SLOT(showBuyGuldenDialog()));
    connect(ui->accountSaveQRButton, SIGNAL(clicked()), this, SLOT(saveQRAsImage()));
    connect(ui->accountRequestPaymentButton, SIGNAL(clicked()), this, SLOT(gotoRequestPaymentPage()));
    connect(ui->generateAnotherRequestButton, SIGNAL(clicked()), this, SLOT(gotoRequestPaymentPage()));
    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(cancelRequestPayment()));
    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(cancelRequestPayment()));

    connect( ui->generateRequestButton, SIGNAL(clicked()), this, SLOT(generateRequest()) );

    updateAddress("");

    gotoReceievePage();
}

void ReceiveCoinsDialog::updateAddress(const QString& address)
{
    accountAddress = address;
    ui->accountAddress->setText(accountAddress);
    updateQRCode(accountAddress);
}

void ReceiveCoinsDialog::setActiveAccount(CAccount* account)
{
    if (account != currentAccount)
    {
        gotoReceievePage();
    }
    currentAccount = account;
}

void ReceiveCoinsDialog::setShowCopyQRAsImageButton(bool showCopyQRAsImagebutton_)
{
    showCopyQRAsImagebutton = showCopyQRAsImagebutton_;
    ui->accountSaveQRButton->setVisible(showCopyQRAsImagebutton_);
}

void ReceiveCoinsDialog::activeAccountChanged(CAccount* activeAccount)
{
    LogPrintf("ReceiveCoinsDialog::activeAccountChanged\n");

    setActiveAccount(activeAccount);
}

void ReceiveCoinsDialog::updateQRCode(const QString& sAddress)
{
    QString uri = QString("Gulden:") + sAddress;
    if(!uri.isEmpty())
    {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH)
        {
            ui->requestQRImage->setFixedWidth(900);
            ui->addressQRImage->setText(tr("Resulting URI too long, try to reduce the text for label / message."));
        }
        else
        {
            ui->requestQRImage->setText("");
            ui->requestQRImage->setFixedWidth(ui->requestQRImage->height());
            ui->addressQRImage->setCode(uri);
        }
    }
}

void ReceiveCoinsDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    connect(model, SIGNAL(activeAccountChanged(CAccount*)), this, SLOT(activeAccountChanged(CAccount*)));
    activeAccountChanged(model->getActiveAccount());

    if(model && model->getOptionsModel())
    {
    }
}

ReceiveCoinsDialog::~ReceiveCoinsDialog()
{
    delete ui;
}

void ReceiveCoinsDialog::copyAddressToClipboard()
{
    if (ui->receiveCoinsStackedWidget->currentIndex() == 0)
    {
        GUIUtil::setClipboard(accountAddress);
        QToolTip::showText(ui->accountCopyToClipboardButton->mapToGlobal(QPoint(0,0)), tr("Address copied to clipboard"),ui->accountCopyToClipboardButton);
    }
    else if (ui->receiveCoinsStackedWidget->currentIndex() == 3)
    {
        GUIUtil::setClipboard(ui->labelPaymentRequest->text());
        QToolTip::showText(ui->requestCopyToClipboardButton->mapToGlobal(QPoint(0,0)), tr("Request copied to clipboard"),ui->requestCopyToClipboardButton);
    }
}


void ReceiveCoinsDialog::saveQRAsImage()
{
    if (ui->receiveCoinsStackedWidget->currentIndex() == 0)
    {
        ui->addressQRImage->saveImage();
    }
    else if (ui->receiveCoinsStackedWidget->currentIndex() == 3)
    {
        ui->requestQRImage->saveImage();
    }
}

void ReceiveCoinsDialog::gotoReceievePage()
{
    ui->receiveCoinsStackedWidget->setCurrentIndex(0);
    ui->requestLabel->setText("");
    ui->requestAmount->clear();
    ui->requestAmount->setDisplayMaxButton(false);

    ui->accountRequestPaymentButtonComposite->setVisible(true);
    ui->accountBuyGuldenButton->setVisible(true);
    ui->accountSaveQRButtonComposite->setVisible(true);
    ui->accountCopyToClipboardButtonComposite->setVisible(true);
    ui->cancelButton->setVisible(false);
    ui->closeButton->setVisible(false);
    ui->cancelButtonGroup->setVisible(false);
    ui->generateRequestButton->setVisible(false);
    ui->generateAnotherRequestButton->setVisible(false);

    ui->accountSaveQRButton->setVisible(showCopyQRAsImagebutton);
}


void ReceiveCoinsDialog::showBuyGuldenDialog()
{
    QString guldenAddress;

    if (buyReceiveAddress)
    {
        delete buyReceiveAddress;
        buyReceiveAddress = NULL;
    }

    buyReceiveAddress = new CReserveKeyOrScript(pactiveWallet, currentAccount, KEYCHAIN_EXTERNAL);
    CPubKey pubKey;
    if (!buyReceiveAddress || !buyReceiveAddress->GetReservedKey(pubKey))
    {
        //fixme: (2.1.x) better error handling
        return;
    }
    else
    {
        CKeyID keyID = pubKey.GetID();
        guldenAddress = QString::fromStdString(CGuldenAddress(keyID).ToString());
    }

    QUrl purchasePage("https://gulden.com/purchase");
    QUrlQuery purchasePageQueryItems;
    purchasePageQueryItems.addQueryItem("receive_address", guldenAddress);
    purchasePage.setQuery(purchasePageQueryItems);
    QDesktopServices::openUrl(purchasePage);
    return;
}

void ReceiveCoinsDialog::gotoRequestPaymentPage()
{
    ui->receiveCoinsStackedWidget->setCurrentIndex(2);

    ui->accountRequestPaymentButtonComposite->setVisible(false);
    ui->accountBuyGuldenButton->setVisible(false);
    ui->accountSaveQRButtonComposite->setVisible(false);
    ui->accountCopyToClipboardButtonComposite->setVisible(false);
    ui->cancelButton->setVisible(true);
    ui->closeButton->setVisible(false);
    ui->cancelButtonGroup->setVisible(true);
    ui->generateRequestButton->setVisible(true);
    ui->generateAnotherRequestButton->setVisible(false);

    ui->accountSaveQRButton->setVisible(false);
}

void ReceiveCoinsDialog::generateRequest()
{
    //fixme: (2.1) (HD) key gaps
    CReserveKeyOrScript reservekey(pactiveWallet, model->getActiveAccount(), KEYCHAIN_EXTERNAL);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey))
    {
        //fixme: (2.1) Better error handling.
        return;
    }
    reservekey.KeepKey();


    ui->receiveCoinsStackedWidget->setCurrentIndex(3);
    ui->accountRequestPaymentButtonComposite->setVisible(false);
    ui->accountBuyGuldenButton->setVisible(false);
    ui->accountSaveQRButtonComposite->setVisible(true);
    ui->accountCopyToClipboardButtonComposite->setVisible(true);
    ui->cancelButton->setVisible(false);
    ui->closeButton->setVisible(true);
    ui->cancelButtonGroup->setVisible(true);
    ui->generateRequestButton->setVisible(false);
    ui->generateAnotherRequestButton->setVisible(true);

    ui->accountSaveQRButton->setVisible(showCopyQRAsImagebutton);

    CAmount amount = ui->requestAmount->amount();
    if (amount > 0)
    {
        ui->labelPaymentRequestHeading->setText(tr("Request %1 Gulden").arg(GuldenUnits::format( GuldenUnits::NLG, amount, false, GuldenUnits::separatorStandard, 2 )));
    }
    else
    {
        ui->labelPaymentRequestHeading->setText(tr("Request Gulden"));
    }

    QString args;
    QString label =  QUrl::toPercentEncoding(ui->requestLabel->text());
    QString strAmount;
    if (!label.isEmpty())
    {
        label = "label=" + label;
    }
    if (amount > 0)
    {
        strAmount = "amount=" +  QUrl::toPercentEncoding(GuldenUnits::format( GuldenUnits::NLG, amount, false, GuldenUnits::separatorNever, -1 ));
        //Trim trailing decimal zeros
        while(strAmount.endsWith("0"))
        {
            strAmount.chop(1);
        }
        if(strAmount.endsWith("."))
        {
            strAmount.chop(1);
        }
    }
    if (!strAmount.isEmpty() && !label.isEmpty())
    {
        args = "?" + label + "&" + strAmount;
    }
    else if(!strAmount.isEmpty())
    {
        args = "?" + strAmount;
    }
    else if(!label.isEmpty())
    {
        args = "?" + label;
    }

    QString uri = QString("Gulden:") + QString::fromStdString(CGuldenAddress(vchPubKey.GetID()).ToString()) + args;
    ui->labelPaymentRequest->setText( uri );
    if(!uri.isEmpty())
    {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH)
        {
            ui->requestQRImage->setFixedWidth(900);
            ui->requestQRImage->setText(tr("Resulting URI too long, try to reduce the text for the label."));
        }
        else
        {
            ui->requestQRImage->setText("");
            ui->requestQRImage->setFixedWidth(ui->requestQRImage->height());
            ui->requestQRImage->setCode(uri);

        }
    }

    pactiveWallet->SetAddressBook(CGuldenAddress(vchPubKey.GetID()).ToString(), ui->requestLabel->text().toStdString(), "receive");
}


void ReceiveCoinsDialog::cancelRequestPayment()
{
    ui->requestLabel->setText("");
    ui->requestAmount->clear();
    gotoReceievePage();
}
