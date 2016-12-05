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

#include "walletview.h"

#include "addressbookpage.h"
#include "askpassphrasedialog.h"
#include "bitcoingui.h"
#include "clientmodel.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "overviewpage.h"
#include "platformstyle.h"
#include "qt/_Gulden/receivecoinsdialog.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "transactiontablemodel.h"
#include "transactionview.h"
#include "walletmodel.h"

#include "ui_interface.h"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QProgressDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QToolButton>

WalletView::WalletView(const PlatformStyle* platformStyle, QWidget* parent)
    : QStackedWidget(parent)
    , clientModel(0)
    , walletModel(0)
    , platformStyle(platformStyle)
{

    overviewPage = new OverviewPage(platformStyle);

    setContentsMargins(0, 0, 0, 0);

    transactionsPage = new QWidget(this);

    QVBoxLayout* vbox = new QVBoxLayout();
    QHBoxLayout* hbox_buttons = new QHBoxLayout();
    transactionView = new TransactionView(platformStyle, this);
    transactionsPage->setObjectName("transactionView");
    transactionView->setObjectName("transactionViewTable");
    vbox->addWidget(transactionView, 6);
    QPushButton* exportButton = new QPushButton(tr("&Export"), this);
    exportButton->setVisible(false);
    exportButton->setToolTip(tr("Export the data in the current tab to a file"));
    if (platformStyle->getImagesOnButtons()) {
        exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }
    hbox_buttons->addStretch();
    hbox_buttons->addWidget(exportButton);
    vbox->addLayout(hbox_buttons);
    vbox->addStretch(1);

    transactionsPage->setLayout(vbox);
    transactionsPage->setContentsMargins(0, 0, 0, 0);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    receiveCoinsPage = new ReceiveCoinsDialog(platformStyle);
    sendCoinsPage = new SendCoinsDialog(platformStyle);

    usedSendingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::SendingTab, this);
    usedReceivingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::ReceivingTab, this);

    addWidget(overviewPage);
    addWidget(transactionsPage);
    addWidget(receiveCoinsPage);
    addWidget(sendCoinsPage);

    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    connect(exportButton, SIGNAL(clicked()), transactionView, SLOT(exportClicked()));

    connect(sendCoinsPage, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));

    connect(transactionView, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));
}

WalletView::~WalletView()
{
}

void WalletView::setBitcoinGUI(BitcoinGUI* gui)
{
    if (gui) {

        connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), gui, SLOT(gotoHistoryPage()));

        connect(this, SIGNAL(message(QString, QString, unsigned int)), gui, SLOT(message(QString, QString, unsigned int)));

        connect(this, SIGNAL(encryptionStatusChanged(int)), gui, SLOT(setEncryptionStatus(int)));

        connect(this, SIGNAL(incomingTransaction(QString, int, CAmount, CAmount, QString, QString, QString, QString)), gui, SLOT(incomingTransaction(QString, int, CAmount, CAmount, QString, QString, QString, QString)));
    }
}

void WalletView::setClientModel(ClientModel* clientModel)
{
    this->clientModel = clientModel;

    overviewPage->setClientModel(clientModel);
    sendCoinsPage->setClientModel(clientModel);
}

void WalletView::setWalletModel(WalletModel* walletModel)
{
    this->walletModel = walletModel;

    transactionView->setModel(walletModel);
    overviewPage->setWalletModel(walletModel);
    receiveCoinsPage->setModel(walletModel);
    sendCoinsPage->setModel(walletModel);
    usedReceivingAddressesPage->setModel(walletModel->getAddressTableModel());
    usedSendingAddressesPage->setModel(walletModel->getAddressTableModel());

    if (walletModel) {

        connect(walletModel, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));

        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SIGNAL(encryptionStatusChanged(int)));
        updateEncryptionStatus();

        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex, int, int)),
                this, SLOT(processNewTransaction(QModelIndex, int, int)));

        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));

        connect(walletModel, SIGNAL(showProgress(QString, int)), this, SLOT(showProgress(QString, int)));
    }
}

void WalletView::processNewTransaction(const QModelIndex& parent, int start, int end)
{

    if (!walletModel || !clientModel || clientModel->inInitialBlockDownload())
        return;

    TransactionTableModel* ttm = walletModel->getTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions())
        return;

    for (int idx = start; idx <= end; ++idx) {
        QString date = ttm->index(idx, TransactionTableModel::Date, parent).data().toString();
        qint64 amountReceived = ttm->index(idx, TransactionTableModel::AmountReceived, parent).data(Qt::EditRole).toULongLong();
        qint64 amountSent = ttm->index(idx, TransactionTableModel::AmountSent, parent).data(Qt::EditRole).toULongLong();
        QString type = ttm->index(idx, TransactionTableModel::Type, parent).data().toString();
        QModelIndex index = ttm->index(idx, 0, parent);
        QString address = ttm->data(index, TransactionTableModel::AddressRole).toString();
        QString label = ttm->data(index, TransactionTableModel::LabelRole).toString();

        QString accountUUID = ttm->data(index, TransactionTableModel::AccountRole).toString();
        if (fShowChildAccountsSeperately) {
            QString accountParentUUID = ttm->data(index, TransactionTableModel::AccountParentRole).toString();
            if (!accountParentUUID.isEmpty())
                accountUUID = accountParentUUID;
        }

        QString account;
        if (!accountUUID.isEmpty()) {
            account = walletModel->getAccountLabel(accountUUID.toStdString());
        }

        Q_EMIT incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amountReceived, amountSent, type, address, account, label);
    }
}

void WalletView::gotoOverviewPage()
{
    setCurrentWidget(overviewPage);
}

void WalletView::gotoHistoryPage()
{
    setCurrentWidget(transactionsPage);
}

void WalletView::gotoReceiveCoinsPage()
{
    setCurrentWidget(receiveCoinsPage);
    receiveCoinsPage->gotoReceievePage();
}

void WalletView::gotoSendCoinsPage(QString addr)
{
    setCurrentWidget(sendCoinsPage);

    if (!addr.isEmpty())
        sendCoinsPage->setAddress(addr);
}

void WalletView::gotoSignMessageTab(QString addr)
{

    SignVerifyMessageDialog* signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr)
{

    SignVerifyMessageDialog* signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

bool WalletView::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    return sendCoinsPage->handlePaymentRequest(recipient);
}

void WalletView::showOutOfSyncWarning(bool fShow)
{
    overviewPage->showOutOfSyncWarning(fShow);
}

void WalletView::updateEncryptionStatus()
{
    Q_EMIT encryptionStatusChanged(walletModel->getEncryptionStatus());
}

void WalletView::encryptWallet(bool status)
{
    if (!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt : AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    updateEncryptionStatus();
}

void WalletView::backupWallet()
{
    QString filename = GUIUtil::getSaveFileName(this,
                                                tr("Backup Wallet"), QString(),
                                                tr("Wallet Data (*.dat)"), NULL);

    if (filename.isEmpty())
        return;

    if (!walletModel->backupWallet(filename)) {
        Q_EMIT message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to %1.").arg(filename),
                       CClientUIInterface::MSG_ERROR);
    } else {
        Q_EMIT message(tr("Backup Successful"), tr("The wallet data was successfully saved to %1.").arg(filename),
                       CClientUIInterface::MSG_INFORMATION);
    }
}

void WalletView::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void WalletView::unlockWallet()
{
    if (!walletModel)
        return;

    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void WalletView::usedSendingAddresses()
{
    if (!walletModel)
        return;

    usedSendingAddressesPage->show();
    usedSendingAddressesPage->raise();
    usedSendingAddressesPage->activateWindow();
}

void WalletView::usedReceivingAddresses()
{
    if (!walletModel)
        return;

    usedReceivingAddressesPage->show();
    usedReceivingAddressesPage->raise();
    usedReceivingAddressesPage->activateWindow();
}

void WalletView::showProgress(const QString& title, int nProgress)
{
    if (nProgress == 0) {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    } else if (nProgress == 100) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    } else if (progressDialog)
        progressDialog->setValue(nProgress);
}
