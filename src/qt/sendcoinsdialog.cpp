// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "sendcoinsdialog.h"
#include "ui_sendcoinsdialog.h"

#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "clientmodel.h"
#include "coincontroldialog.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "_Gulden/guldensendcoinsentry.h"
#include "_Gulden/nocksrequest.h"
#include "_Gulden/GuldenGUI.h"
#include "walletmodel.h"

#include "base58.h"
#include "coincontrol.h"
#include "main.h" // mempool and minRelayTxFee
#include "ui_interface.h"
#include "txmempool.h"
#include "wallet/wallet.h"

#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QTextDocument>
#include <QTimer>

#define SEND_CONFIRM_DELAY 3

SendCoinsDialog::SendCoinsDialog(const PlatformStyle* platformStyle, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::SendCoinsDialog)
    , clientModel(0)
    , model(0)
    , fNewRecipientAllowed(true)
    , fFeeMinimized(true)
    , platformStyle(platformStyle)
{
    ui->setupUi(this);

    if (!platformStyle->getImagesOnButtons()) {
        ui->addButton->setIcon(QIcon());
        ui->clearButton->setIcon(QIcon());
        ui->sendButton->setIcon(QIcon());
    } else {
        ui->addButton->setIcon(platformStyle->SingleColorIcon(":/icons/add"));
        ui->clearButton->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
        ui->sendButton->setIcon(platformStyle->SingleColorIcon(":/icons/send"));
    }

    GUIUtil::setupAddressWidget(ui->lineEditCoinControlChange, this);

    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(addEntry()));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));

    connect(ui->pushButtonCoinControl, SIGNAL(clicked()), this, SLOT(coinControlButtonClicked()));
    connect(ui->checkBoxCoinControlChange, SIGNAL(stateChanged(int)), this, SLOT(coinControlChangeChecked(int)));
    connect(ui->lineEditCoinControlChange, SIGNAL(textEdited(const QString&)), this, SLOT(coinControlChangeEdited(const QString&)));

    QAction* clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction* clipboardAmountAction = new QAction(tr("Copy amount"), this);
    QAction* clipboardFeeAction = new QAction(tr("Copy fee"), this);
    QAction* clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
    QAction* clipboardBytesAction = new QAction(tr("Copy bytes"), this);
    QAction* clipboardPriorityAction = new QAction(tr("Copy priority"), this);
    QAction* clipboardLowOutputAction = new QAction(tr("Copy dust"), this);
    QAction* clipboardChangeAction = new QAction(tr("Copy change"), this);
    connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardQuantity()));
    connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAmount()));
    connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardFee()));
    connect(clipboardAfterFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAfterFee()));
    connect(clipboardBytesAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardBytes()));
    connect(clipboardPriorityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardPriority()));
    connect(clipboardLowOutputAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardLowOutput()));
    connect(clipboardChangeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardChange()));
    ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
    ui->labelCoinControlAmount->addAction(clipboardAmountAction);
    ui->labelCoinControlFee->addAction(clipboardFeeAction);
    ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
    ui->labelCoinControlBytes->addAction(clipboardBytesAction);
    ui->labelCoinControlPriority->addAction(clipboardPriorityAction);
    ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
    ui->labelCoinControlChange->addAction(clipboardChangeAction);

    QSettings settings;
    if (!settings.contains("fFeeSectionMinimized"))
        settings.setValue("fFeeSectionMinimized", true);
    if (!settings.contains("nFeeRadio") && settings.contains("nTransactionFee") && settings.value("nTransactionFee").toLongLong() > 0) // compatibility
        settings.setValue("nFeeRadio", 1); // custom
    if (!settings.contains("nFeeRadio"))
        settings.setValue("nFeeRadio", 0); // recommended
    if (!settings.contains("nCustomFeeRadio") && settings.contains("nTransactionFee") && settings.value("nTransactionFee").toLongLong() > 0) // compatibility
        settings.setValue("nCustomFeeRadio", 1); // total at least
    if (!settings.contains("nCustomFeeRadio"))
        settings.setValue("nCustomFeeRadio", 0); // per kilobyte
    if (!settings.contains("nSmartFeeSliderPosition"))
        settings.setValue("nSmartFeeSliderPosition", 0);
    if (!settings.contains("nTransactionFee"))
        settings.setValue("nTransactionFee", (qint64)DEFAULT_TRANSACTION_FEE);
    if (!settings.contains("fPayOnlyMinFee"))
        settings.setValue("fPayOnlyMinFee", false);
    ui->groupFee->setId(ui->radioSmartFee, 0);
    ui->groupFee->setId(ui->radioCustomFee, 1);
    ui->groupFee->button((int)std::max(0, std::min(1, settings.value("nFeeRadio").toInt())))->setChecked(true);
    ui->groupCustomFee->setId(ui->radioCustomPerKilobyte, 0);
    ui->groupCustomFee->setId(ui->radioCustomAtLeast, 1);
    ui->groupCustomFee->button((int)std::max(0, std::min(1, settings.value("nCustomFeeRadio").toInt())))->setChecked(true);
    ui->sliderSmartFee->setValue(settings.value("nSmartFeeSliderPosition").toInt());
    ui->customFee->setValue(settings.value("nTransactionFee").toLongLong());
    ui->checkBoxMinimumFee->setChecked(settings.value("fPayOnlyMinFee").toBool());
    minimizeFeeSection(settings.value("fFeeSectionMinimized").toBool());

    ui->customFee->setVisible(false);
    ui->labelFeeHeadline->setVisible(false);
    ui->label->setVisible(false);
    ui->labelBalance->setVisible(false);
    ui->addButton->setVisible(false);
    ui->frameFee->setVisible(false);

    ui->horizontalLayout->removeWidget(ui->sendButton);
    QPushButton* editButton = new QPushButton();
    editButton->setText("&Edit");
    editButton->setObjectName("editButton");
    ui->horizontalLayout->addWidget(editButton);
    ui->horizontalLayout->addWidget(ui->sendButton);

    ui->sendButton->setIcon(QIcon());
    ui->clearButton->setIcon(QIcon());
    ui->clearButton->setText("&Clear");
    ui->clearButton->setMinimumSize(QSize(0, 0));
    ui->sendButton->setMinimumSize(QSize(0, 0));

    ui->verticalLayout->setContentsMargins(QMargins(0, 0, 0, 0));
    QFrame* horizontalLine = new QFrame(this);
    horizontalLine->setObjectName("horizontalLine");
    horizontalLine->setFrameStyle(QFrame::HLine);
    horizontalLine->setFixedHeight(1);
    horizontalLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    ui->verticalLayout->insertWidget(2, horizontalLine);

    QPushButton* deleteButton = new QPushButton();
    deleteButton->setText("&Delete");
    deleteButton->setObjectName("deleteButton");
    ui->horizontalLayout->insertWidget(0, deleteButton);

    ui->clearButton->setCursor(Qt::PointingHandCursor);
    ui->sendButton->setCursor(Qt::PointingHandCursor);
    editButton->setCursor(Qt::PointingHandCursor);
    deleteButton->setCursor(Qt::PointingHandCursor);

    connect(deleteButton, SIGNAL(clicked()), this, SLOT(deleteAddressBookEntry()));
    connect(editButton, SIGNAL(clicked()), this, SLOT(editAddressBookEntry()));

    addEntry();

    nocksRequest = NULL;
}

void SendCoinsDialog::setClientModel(ClientModel* clientModel)
{
    this->clientModel = clientModel;

    if (clientModel) {
        connect(clientModel, SIGNAL(numBlocksChanged(int, QDateTime, double, bool)), this, SLOT(updateSmartFeeLabel()));
    }
}

void SendCoinsDialog::setModel(WalletModel* model)
{
    this->model = model;

    if (model && model->getOptionsModel()) {
        for (int i = 0; i < ui->entries->count(); ++i) {
            GuldenSendCoinsEntry* entry = qobject_cast<GuldenSendCoinsEntry*>(ui->entries->itemAt(i)->widget());
            if (entry) {
                entry->setModel(model);
            }
        }

        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(),
                   model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance());
        connect(model, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this, SLOT(setBalance(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        updateDisplayUnit();

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(coinControlUpdateLabels()));
        connect(model->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)), this, SLOT(coinControlFeatureChanged(bool)));
        ui->frameCoinControl->setVisible(model->getOptionsModel()->getCoinControlFeatures());
        coinControlUpdateLabels();

        connect(ui->sliderSmartFee, SIGNAL(valueChanged(int)), this, SLOT(updateSmartFeeLabel()));
        connect(ui->sliderSmartFee, SIGNAL(valueChanged(int)), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->sliderSmartFee, SIGNAL(valueChanged(int)), this, SLOT(coinControlUpdateLabels()));
        connect(ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(updateFeeSectionControls()));
        connect(ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(coinControlUpdateLabels()));
        connect(ui->groupCustomFee, SIGNAL(buttonClicked(int)), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->groupCustomFee, SIGNAL(buttonClicked(int)), this, SLOT(coinControlUpdateLabels()));
        connect(ui->customFee, SIGNAL(valueChanged()), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->customFee, SIGNAL(valueChanged()), this, SLOT(coinControlUpdateLabels()));
        connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(setMinimumFee()));
        connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(updateFeeSectionControls()));
        connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(coinControlUpdateLabels()));
        ui->customFee->setSingleStep(CWallet::GetRequiredFee(1000));
        updateFeeSectionControls();
        updateMinFeeLabel();
        updateSmartFeeLabel();
        updateGlobalFeeVariables();
    }
}

SendCoinsDialog::~SendCoinsDialog()
{
    QSettings settings;
    settings.setValue("fFeeSectionMinimized", fFeeMinimized);
    settings.setValue("nFeeRadio", ui->groupFee->checkedId());
    settings.setValue("nCustomFeeRadio", ui->groupCustomFee->checkedId());
    settings.setValue("nSmartFeeSliderPosition", ui->sliderSmartFee->value());
    settings.setValue("nTransactionFee", (qint64)ui->customFee->value());
    settings.setValue("fPayOnlyMinFee", ui->checkBoxMinimumFee->isChecked());

    delete ui;

    if (nocksRequest) {
        nocksRequest->deleteLater();
        nocksRequest = NULL;
    }
}

void SendCoinsDialog::on_sendButton_clicked()
{
    ui->sendButton->setEnabled(true);

    if (!model || !model->getOptionsModel())
        return;

    QList<SendCoinsRecipient> recipients;
    bool valid = true;

    if (!pendingRecipients.isEmpty()) {
        recipients = pendingRecipients;
    } else {
        for (int i = 0; i < ui->entries->count(); ++i) {
            GuldenSendCoinsEntry* entry = qobject_cast<GuldenSendCoinsEntry*>(ui->entries->itemAt(i)->widget());
            if (entry) {
                if (entry->validate()) {
                    recipients.append(entry->getValue());
                } else {
                    valid = false;
                }
            }
        }
    }

    if (!valid || recipients.isEmpty()) {
        return;
    }

    if (nocksRequest) {
        nocksRequest->deleteLater();
        nocksRequest = NULL;
    }

    pendingRecipients = recipients;
    for (int i = 0; i < pendingRecipients.size(); ++i) {
        if (pendingRecipients[i].paymentType != SendCoinsRecipient::PaymentType::NormalPayment) {
            nocksRequest = new NocksRequest(this, &pendingRecipients[i], NocksRequest::RequestType::Order);
            connect(nocksRequest, SIGNAL(requestProcessed()), this, SLOT(on_sendButton_clicked()));
            ui->sendButton->setEnabled(false);
            return;
        } else if (!pendingRecipients[i].forexFailCode.empty()) {
            WalletModel::SendCoinsReturn errorRet;
            errorRet.status = WalletModel::StatusCode::ForexFailed;
            processSendCoinsReturn(errorRet, QString::fromStdString(pendingRecipients[i].forexFailCode));
            ui->sendButton->setEnabled(true);
            pendingRecipients.clear();
            return;
        }
    }
    pendingRecipients.clear();

    fNewRecipientAllowed = false;
    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) {

        fNewRecipientAllowed = true;
        return;
    }

    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;

    CAccount* forAccount = model->getActiveAccount();
    std::vector<CAccount*> accountsToTry;
    {
        LOCK(pwalletMain->cs_wallet);

        for (const auto& accountPair : pwalletMain->mapAccounts) {
            if (accountPair.second->getParentUUID() == forAccount->getUUID()) {
                accountsToTry.push_back(accountPair.second);
            }
        }
        accountsToTry.push_back(forAccount);
    }
    bool allFailed = true;
    bool didShowError = false;
    for (auto& account : accountsToTry) {
        if ((model->getBalance(account) + model->getUnconfirmedBalance(account)) > currentTransaction.getTotalTransactionAmount()) {
            if (model->getOptionsModel()->getCoinControlFeatures()) // coin control enabled
                prepareStatus = model->prepareTransaction(account, currentTransaction, CoinControlDialog::coinControl);
            else
                prepareStatus = model->prepareTransaction(account, currentTransaction);
            if (prepareStatus.status == WalletModel::OK) {
                allFailed = false;
                didShowError = false;
                forAccount = account;
                break;
            }
            if (prepareStatus.status != WalletModel::AmountExceedsBalance) {
                didShowError = true;
            }
        }
    }
    if (allFailed) {

        if (didShowError) {
            return;
        }
        prepareStatus.status = WalletModel::AmountExceedsBalance;
    }

    processSendCoinsReturn(prepareStatus,
                           BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), currentTransaction.getTransactionFee()));

    if (prepareStatus.status != WalletModel::OK) {
        fNewRecipientAllowed = true;
        return;
    }

    CAmount txFee = currentTransaction.getTransactionFee();

    QStringList formatted;
    Q_FOREACH (const SendCoinsRecipient& rcp, currentTransaction.getRecipients()) {

        QString amount = "<b>" + BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), rcp.amount);
        amount.append("</b>");

        QString address = "<span style='font-family: monospace;'>" + rcp.address;
        address.append("</span>");

        if (!rcp.forexAddress.isEmpty()) {
            amount = "<b>" + BitcoinUnits::format(BitcoinUnits::BTC, rcp.forexAmount);
            if (rcp.forexPaymentType == SendCoinsRecipient::PaymentType::IBANPayment) {
                amount.append(" Euro");
            } else if (rcp.forexPaymentType == SendCoinsRecipient::PaymentType::BCOINPayment) {
                amount.append(" BTC");
            }
            amount.append(" (");
            amount.append(BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), rcp.amount));
            amount.append(")</b>");

            address = "<span style='font-family: monospace;'>" + rcp.forexAddress;
            address.append("</span>");
        }

        QString recipientElement;

        if (!rcp.paymentRequest.IsInitialized()) // normal payment
        {
            if (rcp.label.length() > 0) // label with address
            {
                recipientElement = tr("%1 to %2").arg(amount, GUIUtil::HtmlEscape(rcp.label));
                recipientElement.append(QString(" (%1)").arg(address));
            } else // just address
            {
                recipientElement = tr("%1 to %2").arg(amount, address);
            }
        } else if (!rcp.authenticatedMerchant.isEmpty()) // authenticated payment request
        {
            recipientElement = tr("%1 to %2").arg(amount, GUIUtil::HtmlEscape(rcp.authenticatedMerchant));
        } else // unauthenticated payment request
        {
            recipientElement = tr("%1 to %2").arg(amount, address);
        }

        formatted.append(recipientElement);
    }

    QString questionString = tr("Are you sure you want to send?");
    questionString.append("<br /><br />%1");

    if (txFee > 0) {

        questionString.append("<hr /><span style='color:#aa0000;'>");
        questionString.append(BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), txFee));
        questionString.append("</span> ");
        questionString.append(tr("added as transaction fee"));

        questionString.append(" (" + QString::number((double)currentTransaction.getTransactionSize() / 1000) + " kB)");
    }

    questionString.append("<hr />");
    CAmount totalAmount = currentTransaction.getTotalTransactionAmount() + txFee;
    QStringList alternativeUnits;
    Q_FOREACH (BitcoinUnits::Unit u, BitcoinUnits::availableUnits()) {
        if (u != model->getOptionsModel()->getDisplayUnit())
            alternativeUnits.append(BitcoinUnits::formatHtmlWithUnit(u, totalAmount));
    }
    questionString.append(tr("Total Amount %1")
                              .arg(BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), totalAmount)));
    questionString.append(QString("<span style='font-size:10pt;font-weight:normal;'><br />(=%2)</span>")
                              .arg(alternativeUnits.join(" " + tr("or") + "<br />")));

    QDialog* d = GuldenGUI::createDialog(this, questionString.arg(formatted.join("<br />")), "Send", "Cancel", 600, 360);

    int result = d->exec();
    if (result != QDialog::Accepted) {
        fNewRecipientAllowed = true;
        return;
    }

    Q_FOREACH (const SendCoinsRecipient& rcp, currentTransaction.getRecipients()) {
        if (!rcp.forexAddress.isEmpty() && GetTime() > rcp.expiry) {
            prepareStatus.status = WalletModel::PaymentRequestExpired;
            processSendCoinsReturn(prepareStatus, BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), currentTransaction.getTransactionFee()));
            fNewRecipientAllowed = true;
            return;
        }
    }

    WalletModel::SendCoinsReturn sendStatus = model->sendCoins(currentTransaction);

    processSendCoinsReturn(sendStatus);

    if (sendStatus.status == WalletModel::OK) {
        accept();
        CoinControlDialog::coinControl->UnSelectAll();
        coinControlUpdateLabels();
    }
    fNewRecipientAllowed = true;
}

void SendCoinsDialog::clear()
{

    while (ui->entries->count()) {
        ui->entries->takeAt(0)->widget()->deleteLater();
    }
    addEntry();

    updateTabsAndLabels();
}

void SendCoinsDialog::reject()
{
    clear();
}

void SendCoinsDialog::accept()
{
    clear();
}

void SendCoinsDialog::deleteAddressBookEntry()
{

    for (int i = 0; i < ui->entries->count(); ++i) {
        GuldenSendCoinsEntry* entry = qobject_cast<GuldenSendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry && !entry->isHidden()) {
            entry->deleteAddressBookEntry();
            break;
        }
    }
}

void SendCoinsDialog::editAddressBookEntry()
{

    for (int i = 0; i < ui->entries->count(); ++i) {
        GuldenSendCoinsEntry* entry = qobject_cast<GuldenSendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry && !entry->isHidden()) {
            entry->editAddressBookEntry();
            break;
        }
    }
}

GuldenSendCoinsEntry* SendCoinsDialog::addEntry()
{
    GuldenSendCoinsEntry* entry = new GuldenSendCoinsEntry(platformStyle, this);
    entry->setModel(model);
    ui->entries->addWidget(entry);
    connect(entry, SIGNAL(removeEntry(GuldenSendCoinsEntry*)), this, SLOT(removeEntry(GuldenSendCoinsEntry*)));
    connect(entry, SIGNAL(payAmountChanged()), this, SLOT(coinControlUpdateLabels()));
    connect(entry, SIGNAL(subtractFeeFromAmountChanged()), this, SLOT(coinControlUpdateLabels()));

    connect(entry, SIGNAL(sendTabChanged()), this, SLOT(updateActionButtons()));

    entry->clear();
    entry->setFocus();
    ui->scrollAreaWidgetContents->resize(ui->scrollAreaWidgetContents->sizeHint());
    qApp->processEvents();
    QScrollBar* bar = ui->scrollArea->verticalScrollBar();
    if (bar)
        bar->setSliderPosition(bar->maximum());

    updateTabsAndLabels();
    updateActionButtonsForEntry(entry);
    return entry;
}

void SendCoinsDialog::updateTabsAndLabels()
{
    setupTabChain(0);
    coinControlUpdateLabels();
}

void SendCoinsDialog::removeEntry(GuldenSendCoinsEntry* entry)
{
    entry->hide();

    if (ui->entries->count() == 1)
        addEntry();

    entry->deleteLater();

    updateTabsAndLabels();
}

QWidget* SendCoinsDialog::setupTabChain(QWidget* prev)
{
    for (int i = 0; i < ui->entries->count(); ++i) {
        GuldenSendCoinsEntry* entry = qobject_cast<GuldenSendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry) {
            prev = entry->setupTabChain(prev);
        }
    }
    QWidget::setTabOrder(prev, ui->sendButton);
    QWidget::setTabOrder(ui->sendButton, ui->clearButton);
    QWidget::setTabOrder(ui->clearButton, ui->addButton);
    return ui->addButton;
}

void SendCoinsDialog::setAddress(const QString& address)
{
    GuldenSendCoinsEntry* entry = 0;

    if (ui->entries->count() == 1) {
        GuldenSendCoinsEntry* first = qobject_cast<GuldenSendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if (first->isClear()) {
            entry = first;
        }
    }
    if (!entry) {
        entry = addEntry();
    }

    entry->setAddress(address);
}

void SendCoinsDialog::pasteEntry(const SendCoinsRecipient& rv)
{
    if (!fNewRecipientAllowed)
        return;

    GuldenSendCoinsEntry* entry = 0;

    if (ui->entries->count() == 1) {
        GuldenSendCoinsEntry* first = qobject_cast<GuldenSendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if (first->isClear()) {
            entry = first;
        }
    }
    if (!entry) {
        entry = addEntry();
    }

    entry->setValue(rv);
    updateTabsAndLabels();
}

bool SendCoinsDialog::handlePaymentRequest(const SendCoinsRecipient& rv)
{

    pasteEntry(rv);
    return true;
}

void SendCoinsDialog::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                                 const CAmount& watchBalance, const CAmount& watchUnconfirmedBalance, const CAmount& watchImmatureBalance)
{
    Q_UNUSED(unconfirmedBalance);
    Q_UNUSED(immatureBalance);
    Q_UNUSED(watchBalance);
    Q_UNUSED(watchUnconfirmedBalance);
    Q_UNUSED(watchImmatureBalance);

    if (model && model->getOptionsModel()) {
        ui->labelBalance->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), balance));
    }
}

void SendCoinsDialog::updateActionButtonsForEntry(GuldenSendCoinsEntry* entry)
{
    findChild<QPushButton*>("editButton")->setVisible(entry->ShouldShowEditButton());
    findChild<QPushButton*>("deleteButton")->setVisible(entry->ShouldShowDeleteButton());
    findChild<QPushButton*>("clearButton")->setVisible(entry->ShouldShowClearButton());
}

void SendCoinsDialog::updateActionButtons()
{
    QObject* sender = this->sender();
    GuldenSendCoinsEntry* entry = qobject_cast<GuldenSendCoinsEntry*>(sender);

    updateActionButtonsForEntry(entry);
}

void SendCoinsDialog::updateDisplayUnit()
{
    setBalance(model->getBalance(), 0, 0, 0, 0, 0);
    ui->customFee->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    updateMinFeeLabel();
    updateSmartFeeLabel();
}

void SendCoinsDialog::processSendCoinsReturn(const WalletModel::SendCoinsReturn& sendCoinsReturn, const QString& msgArg)
{
    QPair<QString, CClientUIInterface::MessageBoxFlags> msgParams;

    msgParams.second = CClientUIInterface::MSG_WARNING;

    switch (sendCoinsReturn.status) {
    case WalletModel::InvalidAddress:
        msgParams.first = tr("The recipient address is not valid. Please recheck.");
        break;
    case WalletModel::InvalidAmount:
        msgParams.first = tr("The amount to pay must be larger than 0.");
        break;
    case WalletModel::AmountExceedsBalance:
        msgParams.first = tr("The amount exceeds your balance.\nIf you  have recently received funds you may need to wait for them to clear before spending them.");
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        msgParams.first = tr("The total exceeds your balance when the %1 transaction fee is included.").arg(msgArg);
        break;
    case WalletModel::DuplicateAddress:
        msgParams.first = tr("Duplicate address found: addresses should only be used once each.");
        break;
    case WalletModel::TransactionCreationFailed:
        msgParams.first = tr("Transaction creation failed!");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::TransactionCommitFailed:
        msgParams.first = tr("The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::AbsurdFee:
        msgParams.first = tr("A fee higher than %1 is considered an absurdly high fee.").arg(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), maxTxFee));
        break;
    case WalletModel::PaymentRequestExpired:
        msgParams.first = tr("Payment request expired.");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::ForexFailed:
        msgParams.first = tr("Nocks request failed [%1]").arg(msgArg);
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;

    case WalletModel::OK:
    default:
        return;
    }

    Q_EMIT message(tr("Send Coins"), msgParams.first, msgParams.second);
}

void SendCoinsDialog::minimizeFeeSection(bool fMinimize)
{
    ui->labelFeeMinimized->setVisible(fMinimize);
    ui->buttonChooseFee->setVisible(fMinimize);
    ui->buttonMinimizeFee->setVisible(!fMinimize);
    ui->frameFeeSelection->setVisible(!fMinimize);
    ui->horizontalLayoutSmartFee->setContentsMargins(0, (fMinimize ? 0 : 6), 0, 0);
    fFeeMinimized = fMinimize;
}

void SendCoinsDialog::on_buttonChooseFee_clicked()
{
    minimizeFeeSection(false);
}

void SendCoinsDialog::on_buttonMinimizeFee_clicked()
{
    updateFeeMinimizedLabel();
    minimizeFeeSection(true);
}

void SendCoinsDialog::setMinimumFee()
{
    ui->radioCustomPerKilobyte->setChecked(true);
    ui->customFee->setValue(CWallet::GetRequiredFee(1000));
}

void SendCoinsDialog::updateFeeSectionControls()
{
    ui->sliderSmartFee->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelSmartFee->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelSmartFee2->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelSmartFee3->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelFeeEstimation->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelSmartFeeNormal->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelSmartFeeFast->setEnabled(ui->radioSmartFee->isChecked());
    ui->checkBoxMinimumFee->setEnabled(ui->radioCustomFee->isChecked());
    ui->labelMinFeeWarning->setEnabled(ui->radioCustomFee->isChecked());
    ui->radioCustomPerKilobyte->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked());
    ui->radioCustomAtLeast->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked() && CoinControlDialog::coinControl->HasSelected());
    ui->customFee->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked());
}

void SendCoinsDialog::updateGlobalFeeVariables()
{
    if (ui->radioSmartFee->isChecked()) {
        nTxConfirmTarget = defaultConfirmTarget - ui->sliderSmartFee->value();
        payTxFee = CFeeRate(0);

        CoinControlDialog::coinControl->nMinimumTotalFee = 0;
    } else {
        nTxConfirmTarget = defaultConfirmTarget;
        payTxFee = CFeeRate(ui->customFee->value());

        CoinControlDialog::coinControl->nMinimumTotalFee = ui->radioCustomAtLeast->isChecked() ? ui->customFee->value() : 0;
    }
}

void SendCoinsDialog::updateFeeMinimizedLabel()
{
    if (!model || !model->getOptionsModel())
        return;

    if (ui->radioSmartFee->isChecked())
        ui->labelFeeMinimized->setText(ui->labelSmartFee->text());
    else {
        ui->labelFeeMinimized->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), ui->customFee->value()) + ((ui->radioCustomPerKilobyte->isChecked()) ? "/kB" : ""));
    }
}

void SendCoinsDialog::updateMinFeeLabel()
{
    if (model && model->getOptionsModel())
        ui->checkBoxMinimumFee->setText(tr("Pay only the required fee of %1").arg(
            BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), CWallet::GetRequiredFee(1000)) + "/kB"));
}

void SendCoinsDialog::updateSmartFeeLabel()
{
    if (!model || !model->getOptionsModel())
        return;

    int nBlocksToConfirm = defaultConfirmTarget - ui->sliderSmartFee->value();
    int estimateFoundAtBlocks = nBlocksToConfirm;
    CFeeRate feeRate = mempool.estimateSmartFee(nBlocksToConfirm, &estimateFoundAtBlocks);
    if (feeRate <= CFeeRate(0)) // not enough data => minfee
    {
        ui->labelSmartFee->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(),
                                                                std::max(CWallet::fallbackFee.GetFeePerK(), CWallet::GetRequiredFee(1000))) + "/kB");
        ui->labelSmartFee2->show(); // (Smart fee not initialized yet. This usually takes a few blocks...)
        ui->labelFeeEstimation->setText("");
    } else {
        ui->labelSmartFee->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(),
                                                                std::max(feeRate.GetFeePerK(), CWallet::GetRequiredFee(1000))) + "/kB");
        ui->labelSmartFee2->hide();
        ui->labelFeeEstimation->setText(tr("Estimated to begin confirmation within %n block(s).", "", estimateFoundAtBlocks));
    }

    updateFeeMinimizedLabel();
}

void SendCoinsDialog::coinControlClipboardQuantity()
{
    GUIUtil::setClipboard(ui->labelCoinControlQuantity->text());
}

void SendCoinsDialog::coinControlClipboardAmount()
{
    GUIUtil::setClipboard(ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
}

void SendCoinsDialog::coinControlClipboardFee()
{
    GUIUtil::setClipboard(ui->labelCoinControlFee->text().left(ui->labelCoinControlFee->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

void SendCoinsDialog::coinControlClipboardAfterFee()
{
    GUIUtil::setClipboard(ui->labelCoinControlAfterFee->text().left(ui->labelCoinControlAfterFee->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

void SendCoinsDialog::coinControlClipboardBytes()
{
    GUIUtil::setClipboard(ui->labelCoinControlBytes->text().replace(ASYMP_UTF8, ""));
}

void SendCoinsDialog::coinControlClipboardPriority()
{
    GUIUtil::setClipboard(ui->labelCoinControlPriority->text());
}

void SendCoinsDialog::coinControlClipboardLowOutput()
{
    GUIUtil::setClipboard(ui->labelCoinControlLowOutput->text());
}

void SendCoinsDialog::coinControlClipboardChange()
{
    GUIUtil::setClipboard(ui->labelCoinControlChange->text().left(ui->labelCoinControlChange->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

void SendCoinsDialog::coinControlFeatureChanged(bool checked)
{
    ui->frameCoinControl->setVisible(checked);

    if (!checked && model) // coin control features disabled
        CoinControlDialog::coinControl->SetNull();

    coinControlUpdateLabels();
}

void SendCoinsDialog::coinControlButtonClicked()
{
    CoinControlDialog dlg(platformStyle);
    dlg.setModel(model);
    dlg.exec();
    coinControlUpdateLabels();
}

void SendCoinsDialog::coinControlChangeChecked(int state)
{
    if (state == Qt::Unchecked) {
        CoinControlDialog::coinControl->destChange = CNoDestination();
        ui->labelCoinControlChangeLabel->clear();
    } else

        coinControlChangeEdited(ui->lineEditCoinControlChange->text());

    ui->lineEditCoinControlChange->setEnabled((state == Qt::Checked));
}

void SendCoinsDialog::coinControlChangeEdited(const QString& text)
{
    if (model && model->getAddressTableModel()) {

        CoinControlDialog::coinControl->destChange = CNoDestination();
        ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:red;}");

        CBitcoinAddress addr = CBitcoinAddress(text.toStdString());

        if (text.isEmpty()) // Nothing entered
        {
            ui->labelCoinControlChangeLabel->setText("");
        } else if (!addr.IsValid()) // Invalid address
        {
            ui->labelCoinControlChangeLabel->setText(tr("Warning: Invalid Bitcoin address"));
        } else // Valid address
        {
            CKeyID keyid;
            addr.GetKeyID(keyid);
            if (!model->havePrivKey(keyid)) // Unknown change address
            {
                ui->labelCoinControlChangeLabel->setText(tr("Warning: Unknown change address"));
            } else // Known change address
            {
                ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:black;}");

                QString associatedLabel = model->getAddressTableModel()->labelForAddress(text);
                if (!associatedLabel.isEmpty())
                    ui->labelCoinControlChangeLabel->setText(associatedLabel);
                else
                    ui->labelCoinControlChangeLabel->setText(tr("(no label)"));

                CoinControlDialog::coinControl->destChange = addr.Get();
            }
        }
    }
}

void SendCoinsDialog::coinControlUpdateLabels()
{
    if (!model || !model->getOptionsModel())
        return;

    if (model->getOptionsModel()->getCoinControlFeatures()) {

        ui->radioCustomAtLeast->setVisible(true);

        ui->radioCustomAtLeast->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked() && CoinControlDialog::coinControl->HasSelected());
    } else {

        ui->radioCustomAtLeast->setVisible(false);
        return;
    }

    CoinControlDialog::payAmounts.clear();
    CoinControlDialog::fSubtractFeeFromAmount = false;
    for (int i = 0; i < ui->entries->count(); ++i) {
        GuldenSendCoinsEntry* entry = qobject_cast<GuldenSendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry && !entry->isHidden()) {
            SendCoinsRecipient rcp = entry->getValue();
            CoinControlDialog::payAmounts.append(rcp.amount);
            if (rcp.fSubtractFeeFromAmount)
                CoinControlDialog::fSubtractFeeFromAmount = true;
        }
    }

    if (CoinControlDialog::coinControl->HasSelected()) {

        CoinControlDialog::updateLabels(model, this);

        ui->labelCoinControlAutomaticallySelected->hide();
        ui->widgetCoinControl->show();
    } else {

        ui->labelCoinControlAutomaticallySelected->show();
        ui->widgetCoinControl->hide();
        ui->labelCoinControlInsuffFunds->hide();
    }
}

SendConfirmationDialog::SendConfirmationDialog(const QString& title, const QString& text, int secDelay,
                                               QWidget* parent)
    : QMessageBox(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::Cancel, parent)
    , secDelay(secDelay)
{
    setDefaultButton(QMessageBox::Cancel);
    yesButton = button(QMessageBox::Yes);
    updateYesButton();
    connect(&countDownTimer, SIGNAL(timeout()), this, SLOT(countDown()));
}

int SendConfirmationDialog::exec()
{
    updateYesButton();
    countDownTimer.start(1000);
    return QMessageBox::exec();
}

void SendConfirmationDialog::countDown()
{
    secDelay--;
    updateYesButton();

    if (secDelay <= 0) {
        countDownTimer.stop();
    }
}

void SendConfirmationDialog::updateYesButton()
{
    if (secDelay > 0) {
        yesButton->setEnabled(false);
        yesButton->setText(tr("Yes") + " (" + QString::number(secDelay) + ")");
    } else {
        yesButton->setEnabled(true);
        yesButton->setText(tr("Yes"));
    }
}
