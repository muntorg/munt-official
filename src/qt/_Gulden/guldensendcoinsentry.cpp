// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/gulden-config.h"
#endif

#include "guldensendcoinsentry.h"
#include "_Gulden/forms/ui_guldensendcoinsentry.h"

#include "accounttablemodel.h"
#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "alert.h"

#include "guiconstants.h"
#include "guiutil.h"
#include "nocksrequest.h"
#include "optionsmodel.h"
#include "units.h"
#include "walletmodel.h"
#include "wallet/wallet.h"

#include <QApplication>
#include <QClipboard>
#include <QSortFilterProxyModel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTimer>

#include "gui.h"
#include "validation/validation.h"//chainActive
#include "validation/witnessvalidation.h"
#include <consensus/validation.h>
#include "Gulden/util.h"

GuldenSendCoinsEntry::GuldenSendCoinsEntry(const QStyle *_platformStyle, QWidget *parent) :
    QFrame(parent),
    ui(new Ui::GuldenSendCoinsEntry),
    model(0),
    platformStyle(_platformStyle),
    nocksQuote(nullptr),
    nocksTimer(nullptr)
{
    ui->setupUi(this);

    QList<QTabBar *> tabBar = this->ui->sendCoinsRecipientBook->findChildren<QTabBar *>();
    tabBar.at(0)->setCursor(Qt::PointingHandCursor);	

    ui->searchLabel1->setText( GUIUtil::fontAwesomeSolid("\uf002") );
    ui->searchLabel1->setTextFormat( Qt::RichText );
    ui->searchLabel2->setText( GUIUtil::fontAwesomeSolid("\uf002") );
    ui->searchLabel2->setTextFormat( Qt::RichText );

    update();

    ui->addressBookTabTable->horizontalHeader()->setStretchLastSection(true);
    ui->addressBookTabTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->addressBookTabTable->horizontalHeader()->hide();


    ui->myAccountsTabTable->horizontalHeader()->setStretchLastSection(true);
    ui->myAccountsTabTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->myAccountsTabTable->horizontalHeader()->hide();

    ui->sendCoinsRecipientStack->setContentsMargins(0, 0, 0, 0);
    ui->sendCoinsRecipientPage->setContentsMargins(0, 0, 0, 0);
    ui->sendCoinsWitnessPage->setContentsMargins(0, 0, 0, 0);

    ui->sendCoinsRecipientBook->setContentsMargins(0, 0, 0, 0);
    ui->searchLabel1->setContentsMargins(0, 0, 0, 0);
    ui->searchLabel2->setContentsMargins(0, 0, 0, 0);
    ui->addressBookTabTable->setContentsMargins(0, 0, 0, 0);
    ui->myAccountsTabTable->setContentsMargins(0, 0, 0, 0);

    ui->sendAll->setContentsMargins(0, 0, 0, 0);
    ui->sendAll->setIndent(0);
    ui->sendAll->setCursor(Qt::PointingHandCursor);

    ui->pow2LockFundsSlider->setMinimum(30);
    ui->pow2LockFundsSlider->setMaximum(365*3);
    ui->pow2LockFundsSlider->setValue(30);
    ui->pow2LockFundsSlider->setCursor(Qt::PointingHandCursor);


    connect(ui->searchBox1, SIGNAL(textEdited(QString)), this, SLOT(searchChangedAddressBook(QString)));
    connect(ui->searchBox2, SIGNAL(textEdited(QString)), this, SLOT(searchChangedMyAccounts(QString)));

    connect(ui->sendCoinsRecipientBook, SIGNAL(currentChanged(int)), this, SIGNAL(sendTabChanged()));
    connect(ui->sendCoinsRecipientBook, SIGNAL(currentChanged(int)), this, SLOT(tabChanged()));
    connect(ui->receivingAddress, SIGNAL(textEdited(QString)), this, SLOT(addressChanged()));

    connect(ui->pow2LockFundsSlider, SIGNAL(valueChanged(int)), this, SLOT(witnessSliderValueChanged(int)));

    connect(ui->payAmount, SIGNAL(amountChanged()), this, SLOT(payAmountChanged()));
    connect(ui->payAmount, SIGNAL(amountChanged()), this, SIGNAL(valueChanged()));

    connect(ui->sendAll, SIGNAL(clicked()), this, SLOT(sendAllClicked()));

    ui->receivingAddress->setProperty("valid", true);
    //ui->addAsLabel->setPlaceholderText(tr("Enter a label for this address to add it to your address book"));
}

GuldenSendCoinsEntry::~GuldenSendCoinsEntry()
{
    delete nocksTimer;
    cancelNocksQuote();
    delete ui;
}

bool GuldenSendCoinsEntry::isPoW2WitnessCreation()
{
    return ui->sendCoinsRecipientStack->currentIndex() == 1;
}

void GuldenSendCoinsEntry::update()
{
    if (isPoW2WitnessCreation())
    {
        ui->sendCoinsRecipientStack->setCurrentIndex(0);
        ui->myAccountsTabTable->clearSelection();
    }
}

void GuldenSendCoinsEntry::setModel(WalletModel *_model)
{
    this->model = _model;

    if (model && model->getOptionsModel())
    {
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
        ui->payAmount->setOptionsModel(model->getOptionsModel());
    }

    if (model)
    {
        {
            QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(this);
            proxyModel->setSourceModel(model->getAddressTableModel());
            proxyModel->setDynamicSortFilter(true);
            proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
            proxyModel->setFilterRole(AddressTableModel::TypeRole);
            proxyModel->setFilterFixedString(AddressTableModel::Send);

            proxyModelRecipients = new QSortFilterProxyModel(this);
            proxyModelRecipients->setSourceModel(proxyModel);
            proxyModelRecipients->setDynamicSortFilter(true);
            proxyModelRecipients->setSortCaseSensitivity(Qt::CaseInsensitive);
            proxyModelRecipients->setFilterFixedString("");
            proxyModelRecipients->setFilterCaseSensitivity(Qt::CaseInsensitive);
            proxyModelRecipients->setFilterKeyColumn(AddressTableModel::ColumnIndex::Label);

            ui->addressBookTabTable->setModel(proxyModelRecipients);
        }
        {
            QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(this);
            proxyModel->setSourceModel(model->getAccountTableModel());
            proxyModel->setDynamicSortFilter(true);
            proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
            proxyModel->setFilterRole(AccountTableModel::StateRole);
            proxyModel->setFilterFixedString(GetAccountStateString(AccountState::Normal).c_str());

            QSortFilterProxyModel *proxyInactive = new QSortFilterProxyModel(this);
            proxyInactive->setSourceModel(proxyModel);
            proxyInactive->setDynamicSortFilter(true);
            proxyInactive->setFilterRole(AccountTableModel::ActiveAccountRole);
            proxyInactive->setFilterFixedString(AccountTableModel::Inactive);

            QSortFilterProxyModel *proxyFilterBySubType = new QSortFilterProxyModel(this);
            proxyFilterBySubType->setSourceModel(proxyInactive);
            proxyFilterBySubType->setDynamicSortFilter(true);
            proxyFilterBySubType->setFilterRole(AccountTableModel::TypeRole);
            proxyFilterBySubType->setFilterRegExp(("^(?!"+GetAccountTypeString(AccountType::PoW2Witness)+").*$").c_str());

            proxyModelAddresses = new QSortFilterProxyModel(this);
            proxyModelAddresses->setSourceModel(proxyFilterBySubType);
            proxyModelAddresses->setDynamicSortFilter(true);
            proxyModelAddresses->setSortCaseSensitivity(Qt::CaseInsensitive);
            proxyModelAddresses->setFilterFixedString("");
            proxyModelAddresses->setFilterCaseSensitivity(Qt::CaseInsensitive);
            proxyModelAddresses->setFilterKeyColumn(AccountTableModel::ColumnIndex::Label);
            proxyModelAddresses->setSortRole(Qt::DisplayRole);
            proxyModelAddresses->sort(0);

            connect(proxyModel, SIGNAL(rowsInserted(QModelIndex,int,int)), proxyModelAddresses, SLOT(invalidate()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
            connect(proxyModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), proxyModelAddresses, SLOT(invalidate()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
            connect(proxyModel, SIGNAL(columnsInserted(QModelIndex,int,int)), proxyModelAddresses, SLOT(invalidate()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
            connect(proxyModel, SIGNAL(columnsRemoved(QModelIndex,int,int)), proxyModelAddresses, SLOT(invalidate()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
            connect(proxyModel, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)), proxyModelAddresses, SLOT(invalidate()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
            connect(proxyModel, SIGNAL(columnsMoved(QModelIndex,int,int,QModelIndex,int)), proxyModelAddresses, SLOT(invalidate()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
            connect(proxyModel, SIGNAL(modelReset()), proxyModelAddresses, SLOT(invalidate()));

            ui->myAccountsTabTable->setModel(proxyModelAddresses);
        }
        connect(ui->addressBookTabTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SIGNAL(sendTabChanged()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection)); 
        connect(ui->myAccountsTabTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SIGNAL(sendTabChanged()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));

        connect(ui->addressBookTabTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(addressBookSelectionChanged()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection)); 
        connect(ui->myAccountsTabTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(myAccountsSelectionChanged()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection)); 

        if (!nocksTimer)
        {
            nocksTimer = new QTimer(this);
            nocksTimer->setInterval(60 * 1000); // if nothing changed update quote every 60s
            connect(nocksTimer, SIGNAL(timeout()), this, SLOT(nocksTimeout()));
            nocksTimer->start();
        }
    }

    clear();
}

void GuldenSendCoinsEntry::addressChanged()
{
    SendCoinsRecipient val = getValue(false);
    if (val.paymentType == SendCoinsRecipient::PaymentType::InvalidPayment)
    {
    }
    else
    {
        ui->receivingAddress->setProperty("valid", true);

        if (val.paymentType == SendCoinsRecipient::PaymentType::BitcoinPayment)
        {
            // ui->payAmount->setDisplayCurrency(GuldenAmountField::Currency::Bitcoin);
        }
        else if (val.paymentType == SendCoinsRecipient::PaymentType::IBANPayment)
        {
            ui->payAmount->setPrimaryDisplayCurrency(GuldenAmountField::Currency::Euro);
        }
        else
        {
            ui->payAmount->setPrimaryDisplayCurrency(GuldenAmountField::Currency::Gulden);
        }
    }

    payInfoUpdateRequired();
}

void GuldenSendCoinsEntry::tabChanged()
{
    switch(ui->sendCoinsRecipientBook->currentIndex())
    {
        case 0:
        {
            addressChanged();
        }
        break;
        case 1:
        {
            addressChanged();
        }
        break;
        case 2:
        {
            ui->payAmount->setPrimaryDisplayCurrency(GuldenAmountField::Currency::Gulden);
        }
        break;
    }
}

void GuldenSendCoinsEntry::addressBookSelectionChanged()
{
    tabChanged();
}

void GuldenSendCoinsEntry::myAccountsSelectionChanged()
{
    QModelIndexList selection = ui->myAccountsTabTable->selectionModel()->selectedRows();
    if (selection.count() > 0)
    {
        QModelIndex index = selection.at(0);
        boost::uuids::uuid accountUUID = getUUIDFromString(index.data(AccountTableModel::AccountTableRoles::SelectedAccountRole).toString().toStdString());

        LOCK(pactiveWallet->cs_wallet);

        CAccount* pAccount = pactiveWallet->mapAccounts[accountUUID];
        if ( pAccount->IsPoW2Witness() )
        {
            gotoWitnessTab(pAccount);
        }
    }

    tabChanged();
}


void GuldenSendCoinsEntry::clear()
{
    ui->receivingAddress->setProperty("valid", true);
    ui->receivingAddress->setText("");
    ui->payAmount->clear();

    //fixme: (2.1) - implement the rest of this.
    // clear UI elements for normal payment
    /*ui->payTo->clear();
    ui->addAsLabel->clear();
    ui->checkboxSubtractFeeFromAmount->setCheckState(Qt::Unchecked);
    ui->messageTextLabel->clear();
    ui->messageTextLabel->hide();
    ui->messageLabel->hide();
    // clear UI elements for unauthenticated payment request
    ui->payTo_is->clear();
    ui->memoTextLabel_is->clear();
    ui->payAmount_is->clear();
    // clear UI elements for authenticated payment request
    ui->payTo_s->clear();
    ui->memoTextLabel_s->clear();
    ui->payAmount_s->clear();*/

    // update the display unit, to not use the default ("NLG")
    updateDisplayUnit();
}

void GuldenSendCoinsEntry::deleteClicked()
{
    Q_EMIT removeEntry(this);
}

bool GuldenSendCoinsEntry::validate()
{
    cancelNocksQuote();

    if (!model)
        return false;

    // Check input validity
    bool retval = true;

    // Skip checks for payment request
    if (recipient.paymentRequest.IsInitialized())
        return retval;

    ui->payAmount->setValid(true);
    clearPayInfo();

    if (isPoW2WitnessCreation())
    {
        int nDays = ui->pow2LockFundsSlider->value();
        int64_t nOurWeight = GetPoW2RawWeightForAmount(ui->payAmount->amount(), nDays*576);
        if (ui->payAmount->amount() < (gMinimumWitnessAmount*COIN) || nOurWeight <= gMinimumWitnessWeight)
        {
            setValid(ui->pow2LockFundsInfoLabel, false);
            return false;
        }
    }

    SendCoinsRecipient val = getValue(false);
    if (val.paymentType == SendCoinsRecipient::PaymentType::InvalidPayment)
    {
        setValid(ui->receivingAddress, false);
        retval = false;
    }
    else
    {
        setValid(ui->receivingAddress, false);

        if (val.paymentType == SendCoinsRecipient::PaymentType::BitcoinPayment)
        {
            //ui->payAmount->setCurrency(NULL, NULL, GuldenAmountField::AmountFieldCurrency::CurrencyBitcoin);

            CAmount currencyMax = model->getOptionsModel()->getNocksSettings()->getMaximumForCurrency("NLG-BTC");
            CAmount currencyMin = model->getOptionsModel()->getNocksSettings()->getMinimumForCurrency("NLG-BTC");
            if ( ui->payAmount->amount() > currencyMax || ui->payAmount->amount() < currencyMin )
            {
                ui->payAmount->setValid(false);
                return false;
            }
        }
        else if (val.paymentType == SendCoinsRecipient::PaymentType::IBANPayment)
        {
            CAmount currencyMax = model->getOptionsModel()->getNocksSettings()->getMaximumForCurrency("NLG-EUR");
            CAmount currencyMin = model->getOptionsModel()->getNocksSettings()->getMinimumForCurrency("NLG-EUR");

            if (val.amount > currencyMax ) {
                ui->payAmount->setValid(false);
                setPayInfo(tr("Amount exceeds maximum for IBAN payment."), true);
                return false;
            }

            if (val.amount < currencyMin)
            {
                ui->payAmount->setValid(false);
                setPayInfo(tr("Amount below minimum for IBAN payment."), true);
                return false;
            }
        }
        else
        {
            //ui->payAmount->setCurrency(NULL, NULL, GuldenAmountField::AmountFieldCurrency::CurrencyGulden);
        }
    }

    // Sending a zero amount is invalid
    if (ui->payAmount->amount() <= 0)
    {
        ui->payAmount->setValid(false);
        retval = false;
    }

    // Reject dust outputs:
    /*if (retval && GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value())) {
        ui->payAmount->setValid(false);
        retval = false;
    }*/

    return retval;
}


SendCoinsRecipient::PaymentType GuldenSendCoinsEntry::getPaymentType(const QString& fromAddress)
{
    SendCoinsRecipient::PaymentType ret = SendCoinsRecipient::PaymentType::InvalidPayment;
    if (model->validateAddress(recipient.address))
    {
        ret = SendCoinsRecipient::PaymentType::NormalPayment;
    }
    else
    {
        QString compareModified = recipient.address;
        #ifdef SUPPORT_BITCOIN_AS_FOREX
        if (model->validateAddressBitcoin(compareModified))
        {
            ret = SendCoinsRecipient::PaymentType::BitcoinPayment;
        }
        else
        #endif
        {
            // IBAN
            if (model->validateAddressIBAN(recipient.address))
            {
                ret = SendCoinsRecipient::PaymentType::IBANPayment;
            }
        }
    }
    return ret;
}

SendCoinsRecipient GuldenSendCoinsEntry::getValue(bool showWarningDialogs)
{
    // Payment request
    if (recipient.paymentRequest.IsInitialized())
        return recipient;

    if (!model)
    {
        recipient.paymentType = SendCoinsRecipient::PaymentType::InvalidPayment;
        recipient.address = QString("error");
        return recipient;
    }

    recipient.addToAddressBook = false;
    recipient.fSubtractFeeFromAmount = false;
    recipient.amount = ui->payAmount->amount();

    //fixme: (Post-2.1) - give user a choice here.
    //fixme: (Post-2.1) Check if 'spend unconfirmed' is checked or not.
    CAmount balanceToCheck = pactiveWallet->GetBalance(model->getActiveAccount(), false, true) + pactiveWallet->GetUnconfirmedBalance(model->getActiveAccount(), false, true);
    if (recipient.amount >= balanceToCheck)
    {
        if (showWarningDialogs)
        {
            QString message = recipient.amount > balanceToCheck ?
                        tr("The amount you want to send exceeds your balance, amount has been automatically adjusted downwards to match your balance. Please ensure this is what you want before proceeding to avoid short payment of your recipient.")
                      : tr("The amount you want to send equals your balance, it will be adjusted for the transaction fee. Please ensure this is what you want before proceeding to avoid short payment of your recipient.");

            QDialog* d = GUI::createDialog(this, message, tr("Okay"), "", 400, 180);
            d->exec();
        }

        recipient.amount = balanceToCheck;
        recipient.fSubtractFeeFromAmount = true;
    }


    //fixme: (Post-2.1) - Handle 'messages'
    //recipient.message = ui->messageTextLabel->text();

    recipient.destinationPoW2Witness.lockFromBlock = 0;
    recipient.destinationPoW2Witness.lockUntilBlock = 0;
    if (isPoW2WitnessCreation())
    {
        uint32_t nLockPeriodInBlocks = ui->pow2LockFundsSlider->value()*576;
        // Add a small buffer to give us time to enter the blockchain
        if (nLockPeriodInBlocks == 30*576)
            nLockPeriodInBlocks += 50;
        recipient.destinationPoW2Witness.lockUntilBlock = chainActive.Tip()->nHeight + nLockPeriodInBlocks;

        CReserveKeyOrScript keySpending(pactiveWallet, targetWitnessAccount, KEYCHAIN_SPENDING);
        CPubKey pubSpendingKey;
        if (!keySpending.GetReservedKey(pubSpendingKey))
        {
            std::string strErrorMessage = "Failed to generate a spending key for witness funding.\nPlease unlock your wallet and try again.\nIf the problem persists please seek technical support.";
            CAlert::Notify(strErrorMessage, true, true);
            LogPrintf("%s", strErrorMessage.c_str());
            recipient.paymentType = SendCoinsRecipient::PaymentType::InvalidPayment;
            recipient.address = QString("error");
            return recipient;
        }
        //We delibritely return the key here, so that if we fail we won't leak the key.
        //The key will be marked as used when the transaction is accepted anyway.
        keySpending.ReturnKey();
        CReserveKeyOrScript keyWitness(pactiveWallet, targetWitnessAccount, KEYCHAIN_WITNESS);
        CPubKey pubWitnessKey;
        if (!keySpending.GetReservedKey(pubWitnessKey))
        {
            std::string strErrorMessage = "Failed to generate a witness key for witness funding.\nPlease unlock your wallet and try again.\nIf the problem persists please seek technical support.";
            CAlert::Notify(strErrorMessage, true, true);
            LogPrintf(strErrorMessage.c_str());
            recipient.paymentType = SendCoinsRecipient::PaymentType::InvalidPayment;
            recipient.address = QString("error");
            return recipient;
        }
        //We delibritely return the key here, so that if we fail we won't leak the key.
        //The key will be marked as used when the transaction is accepted anyway.
        keyWitness.ReturnKey();
        recipient.destinationPoW2Witness.spendingKey = pubSpendingKey.GetID();
        recipient.destinationPoW2Witness.witnessKey = pubWitnessKey.GetID();
        recipient.destinationPoW2Witness.failCount = 0;
        recipient.destinationPoW2Witness.actionNonce = 0;

        //NB! Setting this is -super- important, if we don't then encrypted wallets may fail to witness.
        recipient.witnessForAccount = targetWitnessAccount;
        recipient.address = QString::fromStdString(CGuldenAddress(CPoW2WitnessDestination(recipient.destinationPoW2Witness.spendingKey, recipient.destinationPoW2Witness.witnessKey)).ToString());
        recipient.label = QString::fromStdString(pactiveWallet->mapAccountLabels[targetWitnessAccount->getUUID()]);
    }
    else
    {
        switch(ui->sendCoinsRecipientBook->currentIndex())
        {
            case 0:
            {
                recipient.address = ui->receivingAddress->text();
                recipient.label = ui->receivingAddressLabel->text();
                recipient.addToAddressBook = ui->checkBoxAddToAddressBook->isChecked();
                break;
            }
            case 1:
            {
                if (proxyModelRecipients)
                {
                    QModelIndexList selection = ui->addressBookTabTable->selectionModel()->selectedRows();
                    if (selection.count() > 0)
                    {
                        QModelIndex index = selection.at(0);
                        recipient.address = index.sibling(index.row(), 1).data(Qt::DisplayRole).toString();
                        recipient.label = index.sibling(index.row(), 0).data(Qt::DisplayRole).toString();
                    }
                }
                break;
            }
            case 2:
            {
                if (proxyModelAddresses)
                {
                    QModelIndexList selection = ui->myAccountsTabTable->selectionModel()->selectedRows();
                    if (selection.count() > 0)
                    {
                        QModelIndex index = selection.at(0);
                        boost::uuids::uuid accountUUID = getUUIDFromString(index.data(AccountTableModel::AccountTableRoles::SelectedAccountRole).toString().toStdString());

                        LOCK2(cs_main, pactiveWallet->cs_wallet);

                        CReserveKeyOrScript keySpending(pactiveWallet, pactiveWallet->mapAccounts[accountUUID], KEYCHAIN_EXTERNAL);
                        CPubKey pubSpendingKey;
                        if (!keySpending.GetReservedKey(pubSpendingKey))
                        {
                            std::string strErrorMessage = "Failed to generate a spending key for transaction.\nPlease unlock your wallet and try again.\nIf the problem persists please seek technical support.";
                            CAlert::Notify(strErrorMessage, true, true);
                            LogPrintf(strErrorMessage.c_str());
                            recipient.paymentType = SendCoinsRecipient::PaymentType::InvalidPayment;
                            recipient.address = QString("error");
                            return recipient;
                        }
                        //We delibritely return the key here, so that if we fail we won't leak the key.
                        //The key will be marked as used when the transaction is accepted anyway.
                        keySpending.ReturnKey();
                        CKeyID keyID = pubSpendingKey.GetID();
                        recipient.address = QString::fromStdString(CGuldenAddress(keyID).ToString());
                        recipient.label = QString::fromStdString(pactiveWallet->mapAccountLabels[accountUUID]);
                    }
                }
                break;
            }
        }
    }

    // Strip all whitespace
    recipient.address.replace(QRegularExpression("\\s"), "");
    // Strip all punctuation
    recipient.address.replace(QRegularExpression("\\p{P}"), "");

    // Select payment type
    recipient.paymentType = getPaymentType(recipient.address);

    // For IBAN transactions adjust the final amount to Euro
    if (recipient.paymentType == SendCoinsRecipient::PaymentType::IBANPayment)
        recipient.amount = ui->payAmount->amount(GuldenAmountField::Currency::Euro);

    return recipient;
}

QWidget *GuldenSendCoinsEntry::setupTabChain(QWidget *prev)
{
    //fixme: (2.1) Implement.

    return ui->payAmount;
}

void GuldenSendCoinsEntry::setValue(const SendCoinsRecipient &value)
{
    recipient = value;

    /*if (recipient.paymentRequest.IsInitialized()) // payment request
    {
        if (recipient.authenticatedMerchant.isEmpty()) // unauthenticated
        {
            ui->payTo_is->setText(recipient.address);
            ui->memoTextLabel_is->setText(recipient.message);
            ui->payAmount_is->setValue(recipient.amount);
            ui->payAmount_is->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_UnauthenticatedPaymentRequest);
        }
        else // authenticated
        {
            ui->payTo_s->setText(recipient.authenticatedMerchant);
            ui->memoTextLabel_s->setText(recipient.message);
            ui->payAmount_s->setValue(recipient.amount);
            ui->payAmount_s->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_AuthenticatedPaymentRequest);
        }
    }
    else // normal payment
    {
        // message
        ui->messageTextLabel->setText(recipient.message);
        ui->messageTextLabel->setVisible(!recipient.message.isEmpty());
        ui->messageLabel->setVisible(!recipient.message.isEmpty());

        ui->addAsLabel->clear();
        ui->payTo->setText(recipient.address); // this may set a label from addressbook
        if (!recipient.label.isEmpty()) // if a label had been set from the addressbook, don't overwrite with an empty label
            ui->addAsLabel->setText(recipient.label);
        ui->payAmount->setValue(recipient.amount);
    }*/
}

void GuldenSendCoinsEntry::setAmount(const CAmount amount)
{
    ui->payAmount->setAmount(amount);
}

void GuldenSendCoinsEntry::setAddress(const QString &address)
{
    /*ui->payTo->setText(address);
    ui->payAmount->setFocus();*/
}

bool GuldenSendCoinsEntry::isClear()
{
    //return ui->payTo->text().isEmpty() && ui->payTo_is->text().isEmpty() && ui->payTo_s->text().isEmpty();
  return true;
}

void GuldenSendCoinsEntry::setFocus()
{
    //ui->payTo->setFocus();
}

bool GuldenSendCoinsEntry::ShouldShowEditButton() const
{
    switch(ui->sendCoinsRecipientBook->currentIndex())
    {
        case 0:
        case 2:
            return false;
    }
    if (ui->addressBookTabTable->currentIndex().row() >= 0)
        return true;
    return false;
}

bool GuldenSendCoinsEntry::ShouldShowClearButton() const
{
    switch(ui->sendCoinsRecipientBook->currentIndex())
    {
        case 1:
        case 2:
            return false;
    }
    return true;
}

bool GuldenSendCoinsEntry::ShouldShowDeleteButton() const
{
    switch(ui->sendCoinsRecipientBook->currentIndex())
    {
      case 0:
      case 2:
        return false;
    }
    if (ui->addressBookTabTable->currentIndex().row() >= 0)
      return true;
    return false;
}

void GuldenSendCoinsEntry::deleteAddressBookEntry()
{
    switch(ui->sendCoinsRecipientBook->currentIndex())
    {
      case 0:
      case 2:
        return;
    }
    if (proxyModelRecipients && ui->addressBookTabTable->currentIndex().row() >= 0)
    {
        QModelIndexList indexes = ui->addressBookTabTable->selectionModel()->selectedRows();
        if(!indexes.isEmpty())
        {
            QString message = tr("Are you sure you want to delete %1 from the address book?").arg(indexes.at(0).sibling(indexes.at(0).row(), 0).data(Qt::DisplayRole).toString());
            QDialog* d = GUI::createDialog(this, message, tr("Delete"), tr("Cancel"), 400, 180);

            int result = d->exec();
            if(result == QDialog::Accepted)
            {
                ui->addressBookTabTable->model()->removeRow(indexes.at(0).row());
            }
        }
    }
}

void GuldenSendCoinsEntry::editAddressBookEntry()
{
    switch(ui->sendCoinsRecipientBook->currentIndex())
    {
      case 0:
      case 2:
        return;
    }
    if (proxyModelRecipients && ui->addressBookTabTable->currentIndex().row() >= 0)
    {
        QModelIndexList indexes = ui->addressBookTabTable->selectionModel()->selectedRows();
        if(!indexes.isEmpty())
        {
            QDialog* d = new QDialog(this);
            d->setMinimumSize(QSize(400,200));
            QVBoxLayout* vbox = new QVBoxLayout();
            vbox->setSpacing(0);
            vbox->setContentsMargins( 0, 0, 0, 0 );


            QLineEdit* lineEditAddress = new QLineEdit(d);
            vbox->addWidget(lineEditAddress);
            lineEditAddress->setText(indexes.at(0).sibling(indexes.at(0).row(), 0).data(Qt::DisplayRole).toString());
            lineEditAddress->setObjectName("receivingAddress_dialog");
            lineEditAddress->setContentsMargins( 0, 0, 0, 0 );


            QLineEdit* lineEditLabel = new QLineEdit(d);
            vbox->addWidget(lineEditLabel);
            lineEditLabel->setText(indexes.at(0).sibling(indexes.at(0).row(), 1).data(Qt::DisplayRole).toString());
            lineEditLabel->setObjectName("receivingAddressLabel_dialog");
            lineEditLabel->setContentsMargins( 0, 0, 0, 0 );

            QWidget* spacer = new QWidget(d);
            spacer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
            vbox->addWidget(spacer);

            QFrame* horizontalLine = new QFrame(d);
            horizontalLine->setFrameStyle(QFrame::HLine);
            horizontalLine->setFixedHeight(1);
            horizontalLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            horizontalLine->setStyleSheet(GULDEN_DIALOG_HLINE_STYLE);
            vbox->addWidget(horizontalLine);

            //We use reset button because it shows on the left where we want it.
            QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Reset, d);
            QObject::connect(buttonBox, SIGNAL(accepted()), d, SLOT(accept()));
            QObject::connect(buttonBox->button(QDialogButtonBox::Reset), SIGNAL(clicked()), d, SLOT(reject()));
            vbox->addWidget(buttonBox);
            buttonBox->setContentsMargins( 0, 0, 0, 0 );

            buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Save"));
            buttonBox->button(QDialogButtonBox::Ok)->setCursor(Qt::PointingHandCursor);
            buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(GULDEN_DIALOG_CONFIRM_BUTTON_STYLE);
            //buttonBox->button(QDialogButtonBox::Reset)->setObjectName("cancelButton");
            buttonBox->button(QDialogButtonBox::Reset)->setText(tr("Cancel"));
            buttonBox->button(QDialogButtonBox::Reset)->setCursor(Qt::PointingHandCursor);
            buttonBox->button(QDialogButtonBox::Reset)->setStyleSheet(GULDEN_DIALOG_CANCEL_BUTTON_STYLE);

            d->setLayout(vbox);

            int result = d->exec();
            if(result == QDialog::Accepted)
            {
                ui->addressBookTabTable->model()->setData(indexes.at(0).sibling(indexes.at(0).row(), 0), lineEditAddress->text(), Qt::EditRole);
                ui->addressBookTabTable->model()->setData(indexes.at(0).sibling(indexes.at(0).row(), 1), lineEditLabel->text(), Qt::EditRole);
            }
        }
    }
}

void GuldenSendCoinsEntry::gotoWitnessTab(CAccount* targetAccount)
{
    targetWitnessAccount = targetAccount;
    witnessSliderValueChanged(0);
    ui->sendCoinsRecipientStack->setCurrentIndex(1);
}

void GuldenSendCoinsEntry::updateDisplayUnit()
{
    /*if(model && model->getOptionsModel())
    {
        // Update payAmount with the current unit
        ui->payAmount->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_is->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_s->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    }*/
}

void GuldenSendCoinsEntry::searchChangedAddressBook(const QString& searchString)
{
    proxyModelRecipients->setFilterFixedString(searchString);
    //fixme: (2.1) - Only if currently selected item not still visible
    ui->addressBookTabTable->selectionModel()->clear();
    ui->addressBookTabTable->selectionModel()->setCurrentIndex ( proxyModelRecipients->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

void GuldenSendCoinsEntry::searchChangedMyAccounts(const QString& searchString)
{
    proxyModelAddresses->setFilterFixedString(searchString);
    //fixme: (2.1) - Only if currently selected item not still visible
    ui->myAccountsTabTable->selectionModel()->clear();
    ui->myAccountsTabTable->selectionModel()->setCurrentIndex ( proxyModelAddresses->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

#define WITNESS_SUBSIDY 20
void GuldenSendCoinsEntry::witnessSliderValueChanged(int newValue)
{
    setValid(ui->pow2LockFundsInfoLabel, true);

    //fixme: (2.0.1) Improve this dialog.
    CAmount nAmount = ui->payAmount->amount();
    ui->pow2WeightExceedsMaxPercentWarning->setVisible(false);

    if (nAmount < CAmount(gMinimumWitnessAmount*COIN))
    {
        ui->pow2LockFundsInfoLabel->setText(tr("A minimum amount of %1 is required.").arg(gMinimumWitnessAmount));
        return;
    }

    int nDays = newValue;
    float fMonths = newValue/30.0;
    float fYears = newValue/365.0;
    int nEarnings = 0;

    int64_t nOurWeight = GetPoW2RawWeightForAmount(nAmount, nDays*576);

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

    double fBlocksPerDay = 576 * fWeightPercent;
    if (fBlocksPerDay > 5.76)
        fBlocksPerDay = 5.76;

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
}

void GuldenSendCoinsEntry::payAmountChanged()
{
    witnessSliderValueChanged(ui->pow2LockFundsSlider->value());
    payInfoUpdateRequired();
}

bool GuldenSendCoinsEntry::updateLabel(const QString &address)
{
    if(!model)
        return false;

    // Fill in label from address book, if address has an associated label
    QString associatedLabel = model->getAddressTableModel()->labelForAddress(address);
    if(!associatedLabel.isEmpty())
    {
        //ui->addAsLabel->setText(associatedLabel);
        return true;
    }

    return false;
}

void GuldenSendCoinsEntry::payInfoUpdateRequired()
{
    // any outstanding quote request is now outdated
    cancelNocksQuote();

    if (!model)
        return;

    // for IBAN payment that passes minimum amount request a quote
    SendCoinsRecipient val = getValue(false);
    CAmount currencyMin = model->getOptionsModel()->getNocksSettings()->getMinimumForCurrency("NLG-EUR");
    if (val.paymentType == SendCoinsRecipient::PaymentType::IBANPayment && val.amount > currencyMin) {
        nocksQuote = new NocksRequest(this);
        connect(nocksQuote, SIGNAL(requestProcessed()), this, SLOT(nocksQuoteProcessed()));
        nocksQuote->startRequest(NULL, NocksRequest::RequestType::Quotation, "NLG", "EUR",
                                 GuldenUnits::format(GuldenUnits::NLG, val.amount, false, GuldenUnits::separatorNever, 2));
    }
    else
    {
        clearPayInfo();
    }

}

void GuldenSendCoinsEntry::nocksQuoteProcessed()
{
    if (nocksQuote->nativeAmount > 0) // for very small amounts, like EUR 0.01 Nocks will return a negative amount
    {
        QString msg = QString(tr("Will require approximately %1 Gulden including IBAN service fee")).arg(GuldenUnits::format(
                                                                                           GuldenUnits::NLG,
                                                                                           nocksQuote->nativeAmount,
                                                                                           false, GuldenUnits::separatorAlways, 2));
        setPayInfo(msg);
    }
    else
    {
        clearPayInfo();
    }
    nocksQuote->deleteLater();
    nocksQuote = nullptr;
}

void GuldenSendCoinsEntry::nocksTimeout()
{
    // require an update of the payInfo to keep up with Nocks exchange rate changes
    // if there is already a pending Nocks quote we can skip this timer update
    if (!nocksQuote)
    {
        payInfoUpdateRequired();
    }
}

void GuldenSendCoinsEntry::sendAllClicked()
{
    //fixme: (Post-2.1) Check if 'spend unconfirmed' is checked or not.
    ui->payAmount->setAmount(pactiveWallet->GetBalance(model->getActiveAccount(), false, true) + pactiveWallet->GetUnconfirmedBalance(model->getActiveAccount(), false, true));
    payInfoUpdateRequired();
    //Update witness value for amount.
    witnessSliderValueChanged(ui->pow2LockFundsSlider->value());
}

void GuldenSendCoinsEntry::setPayInfo(const QString &msg, bool attention)
{
    ui->payInfo->setText(msg);
    if (attention)
        ui->payInfo->setStyleSheet(STYLE_INVALID);
    else
        ui->payInfo->setStyleSheet("");
}

void GuldenSendCoinsEntry::clearPayInfo()
{
    ui->payInfo->setText("");
    ui->payInfo->setStyleSheet("");
}

void GuldenSendCoinsEntry::cancelNocksQuote()
{
    if (nocksQuote) {
        nocksQuote->cancel();
        nocksQuote->deleteLater();
        nocksQuote = nullptr;
    }
}



