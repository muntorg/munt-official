// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "guldensendcoinsentry.h"
#include "_Gulden/forms/ui_guldensendcoinsentry.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "accounttablemodel.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "walletmodel.h"
#include "wallet/wallet.h"

#include <QApplication>
#include <QClipboard>
#include <QSortFilterProxyModel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>

#include "gui.h"
#include "validation.h"//chainActive
#include "Gulden/util.h"

GuldenSendCoinsEntry::GuldenSendCoinsEntry(const QStyle *_platformStyle, QWidget *parent) :
    QFrame(parent),
    ui(new Ui::GuldenSendCoinsEntry),
    model(0),
    platformStyle(_platformStyle)
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

    connect(ui->payAmount, SIGNAL(valueChanged()), this, SLOT(payAmountChanged()));
    connect(ui->payAmount, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));

    ui->receivingAddress->setProperty("valid", true);
    //ui->addAsLabel->setPlaceholderText(tr("Enter a label for this address to add it to your address book"));
}

GuldenSendCoinsEntry::~GuldenSendCoinsEntry()
{
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

void GuldenSendCoinsEntry::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    //ui->payTo->setText(QApplication::clipboard()->text());
}

void GuldenSendCoinsEntry::on_addressBookButton_clicked()
{
    /*if(!model)
        return;
    AddressBookPage dlg(platformStyle, AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
    dlg.setModel(model->getAddressTableModel());
    if(dlg.exec())
    {
        ui->payTo->setText(dlg.getReturnValue());
        ui->payAmount->setFocus();
    }*/
}

void GuldenSendCoinsEntry::on_payTo_textChanged(const QString &address)
{
    updateLabel(address);
}

void GuldenSendCoinsEntry::setModel(WalletModel *_model)
{
    this->model = _model;

    if (model && model->getOptionsModel())
    {
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
        ui->payAmount->setCurrency(model->getOptionsModel(), model->getOptionsModel()->getTicker(), GuldenAmountField::AmountFieldCurrency::CurrencyGulden);
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
            ui->payAmount->setCurrency(NULL, NULL, GuldenAmountField::AmountFieldCurrency::CurrencyBitcoin);
        }
        else if (val.paymentType == SendCoinsRecipient::PaymentType::IBANPayment)
        {
            ui->payAmount->setCurrency(NULL, NULL, GuldenAmountField::AmountFieldCurrency::CurrencyEuro);
        }
        else
        {
            ui->payAmount->setCurrency(NULL, NULL, GuldenAmountField::AmountFieldCurrency::CurrencyGulden);
        }
    }
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
            ui->payAmount->setCurrency(NULL, NULL, GuldenAmountField::AmountFieldCurrency::CurrencyGulden);
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
            ui->sendCoinsRecipientStack->setCurrentIndex(1);
            witnessSliderValueChanged(ui->pow2LockFundsSlider->value());
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

//fixme: (2.0) - enforce minimum weight for pow2.
bool GuldenSendCoinsEntry::validate()
{
    if (!model)
        return false;

    // Check input validity
    bool retval = true;

    // Skip checks for payment request
    if (recipient.paymentRequest.IsInitialized())
        return retval;

    SendCoinsRecipient val = getValue(false);
    if (val.paymentType == SendCoinsRecipient::PaymentType::InvalidPayment)
    {
        ui->receivingAddress->setProperty("valid", false);
        retval = false;
    }
    else
    {
        ui->receivingAddress->setProperty("valid", true);

        if (val.paymentType == SendCoinsRecipient::PaymentType::BitcoinPayment)
        {
            //ui->payAmount->setCurrency(NULL, NULL, GuldenAmountField::AmountFieldCurrency::CurrencyBitcoin);

            CAmount currencyMax = model->getOptionsModel()->getNocksSettings()->getMaximumForCurrency("NLG-BTC");
            CAmount currencyMin = model->getOptionsModel()->getNocksSettings()->getMinimumForCurrency("NLG-BTC");
            if ( ui->payAmount->valueForCurrency(0) > currencyMax || ui->payAmount->valueForCurrency(0) < currencyMin )
            {
                ui->payAmount->setValid(false);
                return false;
            }
        }
        else if (val.paymentType == SendCoinsRecipient::PaymentType::IBANPayment)
        {
            //ui->payAmount->setCurrency(NULL, NULL, GuldenAmountField::AmountFieldCurrency::CurrencyEuro);

            CAmount currencyMax = model->getOptionsModel()->getNocksSettings()->getMaximumForCurrency("NLG-EUR");
            CAmount currencyMin = model->getOptionsModel()->getNocksSettings()->getMinimumForCurrency("NLG-EUR");
            if ( ui->payAmount->valueForCurrency(0) > currencyMax || ui->payAmount->valueForCurrency(0) < currencyMin )
            {
                ui->payAmount->setValid(false);
                return false;
            }
        }
        else
        {
            //ui->payAmount->setCurrency(NULL, NULL, GuldenAmountField::AmountFieldCurrency::CurrencyGulden);
        }
    }

    if (!ui->payAmount->validate())
    {
        retval = false;
    }

    // Sending a zero amount is invalid
    if (ui->payAmount->valueForCurrency(0) <= 0)
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


    recipient.addToAddressBook = false;
    recipient.fSubtractFeeFromAmount = false;
    recipient.amount = ui->payAmount->valueForCurrency();

    recipient.destinationPoW2Witness.lockFromBlock = 0;
    recipient.destinationPoW2Witness.lockUntilBlock = 0;
    if (isPoW2WitnessCreation())
    {
        uint32_t nLockPeriodInBlocks = ui->pow2LockFundsSlider->value()*576;
        // Add a small buffer to give us time to enter the blockchain
        if (nLockPeriodInBlocks == 30*576)
            nLockPeriodInBlocks += 50;
        recipient.destinationPoW2Witness.lockUntilBlock = chainActive.Tip()->nHeight + nLockPeriodInBlocks;
    }

    //fixme: (Post-2.1) - give user a choice here.
    //fixme: (Post-2.1) Check if 'spend unconfirmed' is checked or not.
    if (recipient.amount >= ( pactiveWallet->GetBalance(model->getActiveAccount(), false, true) + pactiveWallet->GetUnconfirmedBalance(model->getActiveAccount(), true) ))
    {
        if (showWarningDialogs)
        {
            QString message = tr("The amount you want to send exceeds your balance, amount has been automatically adjusted downwards to match your balance. Please ensure this is what you want before proceeding to avoid short payment of your recipient.");
            QDialog* d = GUI::createDialog(this, message, tr("Okay"), "", 400, 180);
            d->exec();
        }

        recipient.amount = pactiveWallet->GetBalance(model->getActiveAccount(), false, true) + pactiveWallet->GetUnconfirmedBalance(model->getActiveAccount(), true);
        recipient.fSubtractFeeFromAmount = true;
    }


    //fixme: (Post-2.1) - Handle 'messages'
    //recipient.message = ui->messageTextLabel->text();

    if (isPoW2WitnessCreation())
    {
        CReserveKey keySpending(pactiveWallet, targetWitnessAccount, KEYCHAIN_SPENDING);
        CPubKey pubSpendingKey;
        if (!keySpending.GetReservedKey(pubSpendingKey))
        {
            //fixme: (2.0) Better error handling
            recipient.paymentType = SendCoinsRecipient::PaymentType::InvalidPayment;
            recipient.address = QString("error");
            return recipient;
        }
        //We delibritely return the key here, so that if we fail we won't leak the key.
        //The key will be marked as used when the transaction is accepted anyway.
        keySpending.ReturnKey();
        CReserveKey keyWitness(pactiveWallet, targetWitnessAccount, KEYCHAIN_WITNESS);
        CPubKey pubWitnessKey;
        if (!keySpending.GetReservedKey(pubWitnessKey))
        {
            //fixme: (2.0) Better error handling
            recipient.paymentType = SendCoinsRecipient::PaymentType::InvalidPayment;
            recipient.address = QString("error");
            return recipient;
        }
        //We delibritely return the key here, so that if we fail we won't leak the key.
        //The key will be marked as used when the transaction is accepted anyway.
        keyWitness.ReturnKey();
        recipient.destinationPoW2Witness.spendingKey = pubSpendingKey.GetID();
        recipient.destinationPoW2Witness.witnessKey = pubWitnessKey.GetID();
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

                        LOCK(pactiveWallet->cs_wallet);

                        CReserveKey keySpending(pactiveWallet, pactiveWallet->mapAccounts[accountUUID], KEYCHAIN_EXTERNAL);
                        CPubKey pubSpendingKey;
                        if (!keySpending.GetReservedKey(pubSpendingKey))
                        {
                            //fixme: (2.0) Better error handling
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
    ui->payAmount->setValue(amount);
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
    //fixme: (2.0) (POW2) (CLEANUP)
    CAmount nAmount = ui->payAmount->valueForCurrency();
    ui->pow2WeightExceedsMaxPercentWarning->setVisible(false);

    if (nAmount < CAmount(500000000000))
    {
        ui->pow2LockFundsInfoLabel->setText(tr("A minimum amount of 5000 is required."));
        return;
    }

    //fixme: (2.0) (HIGH) - warn if weight exceeds 1%.
    int nDays = newValue;
    float fMonths = newValue/30.0;
    float fYears = newValue/365.0;
    int nEarnings = 0;


    int64_t nOurWeight = GetPoW2RawWeightForAmount(nAmount, nDays*576);

    //fixme: (2.0) (HIGH)
    int64_t nNetworkWeight = 239990000;


    if (nOurWeight < 10000)
    {
        ui->pow2LockFundsInfoLabel->setText(tr("A minimum weight of 10000 is required, but selected weight is only %1 please increase the amount or lock time for a larger weight.").arg(nOurWeight));
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

