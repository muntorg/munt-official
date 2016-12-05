// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coincontroldialog.h"
#include "ui_coincontroldialog.h"

#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "txmempool.h"
#include "walletmodel.h"

#include "coincontrol.h"
#include "init.h"
#include "main.h" // For minRelayTxFee
#include "wallet/wallet.h"

#include <boost/assign/list_of.hpp> // for 'map_list_of()'

#include <QApplication>
#include <QCheckBox>
#include <QCursor>
#include <QDialogButtonBox>
#include <QFlags>
#include <QIcon>
#include <QSettings>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>

QList<CAmount> CoinControlDialog::payAmounts;
CCoinControl* CoinControlDialog::coinControl = new CCoinControl();
bool CoinControlDialog::fSubtractFeeFromAmount = false;

CoinControlDialog::CoinControlDialog(const PlatformStyle* platformStyle, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::CoinControlDialog)
    , model(0)
    , platformStyle(platformStyle)
{
    ui->setupUi(this);

    QAction* copyAddressAction = new QAction(tr("Copy address"), this);
    QAction* copyLabelAction = new QAction(tr("Copy label"), this);
    QAction* copyAmountAction = new QAction(tr("Copy amount"), this);
    copyTransactionHashAction = new QAction(tr("Copy transaction ID"), this); // we need to enable/disable this
    lockAction = new QAction(tr("Lock unspent"), this); // we need to enable/disable this
    unlockAction = new QAction(tr("Unlock unspent"), this); // we need to enable/disable this

    contextMenu = new QMenu();
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyTransactionHashAction);
    contextMenu->addSeparator();
    contextMenu->addAction(lockAction);
    contextMenu->addAction(unlockAction);

    connect(ui->treeWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu(QPoint)));
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(copyAddress()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(copyLabel()));
    connect(copyAmountAction, SIGNAL(triggered()), this, SLOT(copyAmount()));
    connect(copyTransactionHashAction, SIGNAL(triggered()), this, SLOT(copyTransactionHash()));
    connect(lockAction, SIGNAL(triggered()), this, SLOT(lockCoin()));
    connect(unlockAction, SIGNAL(triggered()), this, SLOT(unlockCoin()));

    QAction* clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction* clipboardAmountAction = new QAction(tr("Copy amount"), this);
    QAction* clipboardFeeAction = new QAction(tr("Copy fee"), this);
    QAction* clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
    QAction* clipboardBytesAction = new QAction(tr("Copy bytes"), this);
    QAction* clipboardPriorityAction = new QAction(tr("Copy priority"), this);
    QAction* clipboardLowOutputAction = new QAction(tr("Copy dust"), this);
    QAction* clipboardChangeAction = new QAction(tr("Copy change"), this);

    connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(clipboardQuantity()));
    connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(clipboardAmount()));
    connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(clipboardFee()));
    connect(clipboardAfterFeeAction, SIGNAL(triggered()), this, SLOT(clipboardAfterFee()));
    connect(clipboardBytesAction, SIGNAL(triggered()), this, SLOT(clipboardBytes()));
    connect(clipboardPriorityAction, SIGNAL(triggered()), this, SLOT(clipboardPriority()));
    connect(clipboardLowOutputAction, SIGNAL(triggered()), this, SLOT(clipboardLowOutput()));
    connect(clipboardChangeAction, SIGNAL(triggered()), this, SLOT(clipboardChange()));

    ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
    ui->labelCoinControlAmount->addAction(clipboardAmountAction);
    ui->labelCoinControlFee->addAction(clipboardFeeAction);
    ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
    ui->labelCoinControlBytes->addAction(clipboardBytesAction);
    ui->labelCoinControlPriority->addAction(clipboardPriorityAction);
    ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
    ui->labelCoinControlChange->addAction(clipboardChangeAction);

    connect(ui->radioTreeMode, SIGNAL(toggled(bool)), this, SLOT(radioTreeMode(bool)));
    connect(ui->radioListMode, SIGNAL(toggled(bool)), this, SLOT(radioListMode(bool)));

    connect(ui->treeWidget, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(viewItemChanged(QTreeWidgetItem*, int)));

#if QT_VERSION < 0x050000
    ui->treeWidget->header()->setClickable(true);
#else
    ui->treeWidget->header()->setSectionsClickable(true);
#endif
    connect(ui->treeWidget->header(), SIGNAL(sectionClicked(int)), this, SLOT(headerSectionClicked(int)));

    connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonBoxClicked(QAbstractButton*)));

    connect(ui->pushButtonSelectAll, SIGNAL(clicked()), this, SLOT(buttonSelectAllClicked()));

    ui->treeWidget->headerItem()->setText(COLUMN_CHECKBOX, QString());

    ui->treeWidget->setColumnWidth(COLUMN_CHECKBOX, 84);
    ui->treeWidget->setColumnWidth(COLUMN_AMOUNT, 100);
    ui->treeWidget->setColumnWidth(COLUMN_LABEL, 170);
    ui->treeWidget->setColumnWidth(COLUMN_ADDRESS, 290);
    ui->treeWidget->setColumnWidth(COLUMN_DATE, 110);
    ui->treeWidget->setColumnWidth(COLUMN_CONFIRMATIONS, 100);
    ui->treeWidget->setColumnWidth(COLUMN_PRIORITY, 100);
    ui->treeWidget->setColumnHidden(COLUMN_TXHASH, true); // store transaction hash in this column, but don't show it
    ui->treeWidget->setColumnHidden(COLUMN_VOUT_INDEX, true); // store vout index in this column, but don't show it
    ui->treeWidget->setColumnHidden(COLUMN_AMOUNT_INT64, true); // store amount int64 in this column, but don't show it
    ui->treeWidget->setColumnHidden(COLUMN_PRIORITY_INT64, true); // store priority int64 in this column, but don't show it
    ui->treeWidget->setColumnHidden(COLUMN_DATE_INT64, true); // store date int64 in this column, but don't show it

    sortView(COLUMN_AMOUNT_INT64, Qt::DescendingOrder);

    QSettings settings;
    if (settings.contains("nCoinControlMode") && !settings.value("nCoinControlMode").toBool())
        ui->radioTreeMode->click();
    if (settings.contains("nCoinControlSortColumn") && settings.contains("nCoinControlSortOrder"))
        sortView(settings.value("nCoinControlSortColumn").toInt(), ((Qt::SortOrder)settings.value("nCoinControlSortOrder").toInt()));
}

CoinControlDialog::~CoinControlDialog()
{
    QSettings settings;
    settings.setValue("nCoinControlMode", ui->radioListMode->isChecked());
    settings.setValue("nCoinControlSortColumn", sortColumn);
    settings.setValue("nCoinControlSortOrder", (int)sortOrder);

    delete ui;
}

void CoinControlDialog::setModel(WalletModel* model)
{
    this->model = model;

    if (model && model->getOptionsModel() && model->getAddressTableModel()) {
        updateView();
        updateLabelLocked();
        CoinControlDialog::updateLabels(model, this);
    }
}

QString CoinControlDialog::strPad(QString s, int nPadLength, QString sPadding)
{
    while (s.length() < nPadLength)
        s = sPadding + s;

    return s;
}

void CoinControlDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole)
        done(QDialog::Accepted); // closes the dialog
}

void CoinControlDialog::buttonSelectAllClicked()
{
    Qt::CheckState state = Qt::Checked;
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
        if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != Qt::Unchecked) {
            state = Qt::Unchecked;
            break;
        }
    }
    ui->treeWidget->setEnabled(false);
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
        if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != state)
            ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, state);
    ui->treeWidget->setEnabled(true);
    if (state == Qt::Unchecked)
        coinControl->UnSelectAll(); // just to be sure
    CoinControlDialog::updateLabels(model, this);
}

void CoinControlDialog::showMenu(const QPoint& point)
{
    QTreeWidgetItem* item = ui->treeWidget->itemAt(point);
    if (item) {
        contextMenuItem = item;

        if (item->text(COLUMN_TXHASH).length() == 64) // transaction hash is 64 characters (this means its a child node, so its not a parent node in tree mode)
        {
            copyTransactionHashAction->setEnabled(true);
            if (model->isLockedCoin(uint256S(item->text(COLUMN_TXHASH).toStdString()), item->text(COLUMN_VOUT_INDEX).toUInt())) {
                lockAction->setEnabled(false);
                unlockAction->setEnabled(true);
            } else {
                lockAction->setEnabled(true);
                unlockAction->setEnabled(false);
            }
        } else // this means click on parent node in tree mode -> disable all
        {
            copyTransactionHashAction->setEnabled(false);
            lockAction->setEnabled(false);
            unlockAction->setEnabled(false);
        }

        contextMenu->exec(QCursor::pos());
    }
}

void CoinControlDialog::copyAmount()
{
    GUIUtil::setClipboard(BitcoinUnits::removeSpaces(contextMenuItem->text(COLUMN_AMOUNT)));
}

void CoinControlDialog::copyLabel()
{
    if (ui->radioTreeMode->isChecked() && contextMenuItem->text(COLUMN_LABEL).length() == 0 && contextMenuItem->parent())
        GUIUtil::setClipboard(contextMenuItem->parent()->text(COLUMN_LABEL));
    else
        GUIUtil::setClipboard(contextMenuItem->text(COLUMN_LABEL));
}

void CoinControlDialog::copyAddress()
{
    if (ui->radioTreeMode->isChecked() && contextMenuItem->text(COLUMN_ADDRESS).length() == 0 && contextMenuItem->parent())
        GUIUtil::setClipboard(contextMenuItem->parent()->text(COLUMN_ADDRESS));
    else
        GUIUtil::setClipboard(contextMenuItem->text(COLUMN_ADDRESS));
}

void CoinControlDialog::copyTransactionHash()
{
    GUIUtil::setClipboard(contextMenuItem->text(COLUMN_TXHASH));
}

void CoinControlDialog::lockCoin()
{
    if (contextMenuItem->checkState(COLUMN_CHECKBOX) == Qt::Checked)
        contextMenuItem->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

    COutPoint outpt(uint256S(contextMenuItem->text(COLUMN_TXHASH).toStdString()), contextMenuItem->text(COLUMN_VOUT_INDEX).toUInt());
    model->lockCoin(outpt);
    contextMenuItem->setDisabled(true);
    contextMenuItem->setIcon(COLUMN_CHECKBOX, platformStyle->SingleColorIcon(":/icons/lock_closed"));
    updateLabelLocked();
}

void CoinControlDialog::unlockCoin()
{
    COutPoint outpt(uint256S(contextMenuItem->text(COLUMN_TXHASH).toStdString()), contextMenuItem->text(COLUMN_VOUT_INDEX).toUInt());
    model->unlockCoin(outpt);
    contextMenuItem->setDisabled(false);
    contextMenuItem->setIcon(COLUMN_CHECKBOX, QIcon());
    updateLabelLocked();
}

void CoinControlDialog::clipboardQuantity()
{
    GUIUtil::setClipboard(ui->labelCoinControlQuantity->text());
}

void CoinControlDialog::clipboardAmount()
{
    GUIUtil::setClipboard(ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
}

void CoinControlDialog::clipboardFee()
{
    GUIUtil::setClipboard(ui->labelCoinControlFee->text().left(ui->labelCoinControlFee->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

void CoinControlDialog::clipboardAfterFee()
{
    GUIUtil::setClipboard(ui->labelCoinControlAfterFee->text().left(ui->labelCoinControlAfterFee->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

void CoinControlDialog::clipboardBytes()
{
    GUIUtil::setClipboard(ui->labelCoinControlBytes->text().replace(ASYMP_UTF8, ""));
}

void CoinControlDialog::clipboardPriority()
{
    GUIUtil::setClipboard(ui->labelCoinControlPriority->text());
}

void CoinControlDialog::clipboardLowOutput()
{
    GUIUtil::setClipboard(ui->labelCoinControlLowOutput->text());
}

void CoinControlDialog::clipboardChange()
{
    GUIUtil::setClipboard(ui->labelCoinControlChange->text().left(ui->labelCoinControlChange->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

void CoinControlDialog::sortView(int column, Qt::SortOrder order)
{
    sortColumn = column;
    sortOrder = order;
    ui->treeWidget->sortItems(column, order);
    ui->treeWidget->header()->setSortIndicator(getMappedColumn(sortColumn), sortOrder);
}

void CoinControlDialog::headerSectionClicked(int logicalIndex)
{
    if (logicalIndex == COLUMN_CHECKBOX) // click on most left column -> do nothing
    {
        ui->treeWidget->header()->setSortIndicator(getMappedColumn(sortColumn), sortOrder);
    } else {
        logicalIndex = getMappedColumn(logicalIndex, false);

        if (sortColumn == logicalIndex)
            sortOrder = ((sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder);
        else {
            sortColumn = logicalIndex;
            sortOrder = ((sortColumn == COLUMN_LABEL || sortColumn == COLUMN_ADDRESS) ? Qt::AscendingOrder : Qt::DescendingOrder); // if label or address then default => asc, else default => desc
        }

        sortView(sortColumn, sortOrder);
    }
}

void CoinControlDialog::radioTreeMode(bool checked)
{
    if (checked && model)
        updateView();
}

void CoinControlDialog::radioListMode(bool checked)
{
    if (checked && model)
        updateView();
}

void CoinControlDialog::viewItemChanged(QTreeWidgetItem* item, int column)
{
    if (column == COLUMN_CHECKBOX && item->text(COLUMN_TXHASH).length() == 64) // transaction hash is 64 characters (this means its a child node, so its not a parent node in tree mode)
    {
        COutPoint outpt(uint256S(item->text(COLUMN_TXHASH).toStdString()), item->text(COLUMN_VOUT_INDEX).toUInt());

        if (item->checkState(COLUMN_CHECKBOX) == Qt::Unchecked)
            coinControl->UnSelect(outpt);
        else if (item->isDisabled()) // locked (this happens if "check all" through parent node)
            item->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
        else
            coinControl->Select(outpt);

        if (ui->treeWidget->isEnabled()) // do not update on every click for (un)select all
            CoinControlDialog::updateLabels(model, this);
    }

#if QT_VERSION >= 0x050000
    else if (column == COLUMN_CHECKBOX && item->childCount() > 0) {
        if (item->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked && item->child(0)->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked)
            item->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
    }
#endif
}

QString CoinControlDialog::getPriorityLabel(double dPriority, double mempoolEstimatePriority)
{
    double dPriorityMedium = mempoolEstimatePriority;

    if (dPriorityMedium <= 0)
        dPriorityMedium = AllowFreeThreshold(); // not enough data, back to hard-coded

    if (dPriority / 1000000 > dPriorityMedium)
        return tr("highest");
    else if (dPriority / 100000 > dPriorityMedium)
        return tr("higher");
    else if (dPriority / 10000 > dPriorityMedium)
        return tr("high");
    else if (dPriority / 1000 > dPriorityMedium)
        return tr("medium-high");
    else if (dPriority > dPriorityMedium)
        return tr("medium");
    else if (dPriority * 10 > dPriorityMedium)
        return tr("low-medium");
    else if (dPriority * 100 > dPriorityMedium)
        return tr("low");
    else if (dPriority * 1000 > dPriorityMedium)
        return tr("lower");
    else
        return tr("lowest");
}

void CoinControlDialog::updateLabelLocked()
{
    std::vector<COutPoint> vOutpts;
    model->listLockedCoins(vOutpts);
    if (vOutpts.size() > 0) {
        ui->labelLocked->setText(tr("(%1 locked)").arg(vOutpts.size()));
        ui->labelLocked->setVisible(true);
    } else
        ui->labelLocked->setVisible(false);
}

void CoinControlDialog::updateLabels(WalletModel* model, QDialog* dialog)
{
    if (!model)
        return;

    CAmount nPayAmount = 0;
    bool fDust = false;
    CMutableTransaction txDummy;
    Q_FOREACH (const CAmount& amount, CoinControlDialog::payAmounts) {
        nPayAmount += amount;

        if (amount > 0) {
            CTxOut txout(amount, (CScript)std::vector<unsigned char>(24, 0));
            txDummy.vout.push_back(txout);
            if (txout.IsDust(::minRelayTxFee))
                fDust = true;
        }
    }

    QString sPriorityLabel = tr("none");
    CAmount nAmount = 0;
    CAmount nPayFee = 0;
    CAmount nAfterFee = 0;
    CAmount nChange = 0;
    unsigned int nBytes = 0;
    unsigned int nBytesInputs = 0;
    double dPriority = 0;
    double dPriorityInputs = 0;
    unsigned int nQuantity = 0;
    int nQuantityUncompressed = 0;
    bool fAllowFree = false;
    bool fWitness = false;

    std::vector<COutPoint> vCoinControl;
    std::vector<COutput> vOutputs;
    coinControl->ListSelected(vCoinControl);
    model->getOutputs(vCoinControl, vOutputs);

    BOOST_FOREACH (const COutput& out, vOutputs) {

        uint256 txhash = out.tx->GetHash();
        COutPoint outpt(txhash, out.i);
        if (model->isSpent(outpt)) {
            coinControl->UnSelect(outpt);
            continue;
        }

        nQuantity++;

        nAmount += out.tx->vout[out.i].nValue;

        dPriorityInputs += (double)out.tx->vout[out.i].nValue * (out.nDepth + 1);

        CTxDestination address;
        int witnessversion = 0;
        std::vector<unsigned char> witnessprogram;
        if (out.tx->vout[out.i].scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
            nBytesInputs += (32 + 4 + 1 + (107 / WITNESS_SCALE_FACTOR) + 4);
            fWitness = true;
        } else if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, address)) {
            CPubKey pubkey;
            CKeyID* keyid = boost::get<CKeyID>(&address);
            if (keyid && model->getPubKey(*keyid, pubkey)) {
                nBytesInputs += (pubkey.IsCompressed() ? 148 : 180);
                if (!pubkey.IsCompressed())
                    nQuantityUncompressed++;
            } else
                nBytesInputs += 148; // in all error cases, simply assume 148 here
        } else
            nBytesInputs += 148;
    }

    if (nQuantity > 0) {

        nBytes = nBytesInputs + ((CoinControlDialog::payAmounts.size() > 0 ? CoinControlDialog::payAmounts.size() + 1 : 2) * 34) + 10; // always assume +1 output for change here
        if (fWitness) {

            nBytes += 2; // account for the serialized marker and flag bytes
            nBytes += nQuantity; // account for the witness byte that holds the number of stack items for each input.
        }

        double mempoolEstimatePriority = mempool.estimateSmartPriority(nTxConfirmTarget);
        dPriority = dPriorityInputs / (nBytes - nBytesInputs + (nQuantityUncompressed * 29)); // 29 = 180 - 151 (uncompressed public keys are over the limit. max 151 bytes of the input are ignored for priority)
        sPriorityLabel = CoinControlDialog::getPriorityLabel(dPriority, mempoolEstimatePriority);

        if (CoinControlDialog::fSubtractFeeFromAmount)
            if (nAmount - nPayAmount == 0)
                nBytes -= 34;

        nPayFee = CWallet::GetMinimumFee(nBytes, nTxConfirmTarget, mempool);
        if (nPayFee > 0 && coinControl->nMinimumTotalFee > nPayFee)
            nPayFee = coinControl->nMinimumTotalFee;

        double dPriorityNeeded = std::max(mempoolEstimatePriority, AllowFreeThreshold());
        fAllowFree = (dPriority >= dPriorityNeeded);

        if (fSendFreeTransactions)
            if (fAllowFree && nBytes <= MAX_FREE_TRANSACTION_CREATE_SIZE)
                nPayFee = 0;

        if (nPayAmount > 0) {
            nChange = nAmount - nPayAmount;
            if (!CoinControlDialog::fSubtractFeeFromAmount)
                nChange -= nPayFee;

            if (nChange > 0 && nChange < MIN_CHANGE) {
                CTxOut txout(nChange, (CScript)std::vector<unsigned char>(24, 0));
                if (txout.IsDust(::minRelayTxFee)) {
                    if (CoinControlDialog::fSubtractFeeFromAmount) // dust-change will be raised until no dust
                        nChange = txout.GetDustThreshold(::minRelayTxFee);
                    else {
                        nPayFee += nChange;
                        nChange = 0;
                    }
                }
            }

            if (nChange == 0 && !CoinControlDialog::fSubtractFeeFromAmount)
                nBytes -= 34;
        }

        nAfterFee = nAmount - nPayFee;
        if (nAfterFee < 0)
            nAfterFee = 0;
    }

    int nDisplayUnit = BitcoinUnits::BTC;
    if (model && model->getOptionsModel())
        nDisplayUnit = model->getOptionsModel()->getDisplayUnit();

    QLabel* l1 = dialog->findChild<QLabel*>("labelCoinControlQuantity");
    QLabel* l2 = dialog->findChild<QLabel*>("labelCoinControlAmount");
    QLabel* l3 = dialog->findChild<QLabel*>("labelCoinControlFee");
    QLabel* l4 = dialog->findChild<QLabel*>("labelCoinControlAfterFee");
    QLabel* l5 = dialog->findChild<QLabel*>("labelCoinControlBytes");
    QLabel* l6 = dialog->findChild<QLabel*>("labelCoinControlPriority");
    QLabel* l7 = dialog->findChild<QLabel*>("labelCoinControlLowOutput");
    QLabel* l8 = dialog->findChild<QLabel*>("labelCoinControlChange");

    dialog->findChild<QLabel*>("labelCoinControlLowOutputText")->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel*>("labelCoinControlLowOutput")->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel*>("labelCoinControlChangeText")->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel*>("labelCoinControlChange")->setEnabled(nPayAmount > 0);

    l1->setText(QString::number(nQuantity)); // Quantity
    l2->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nAmount)); // Amount
    l3->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nPayFee)); // Fee
    l4->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nAfterFee)); // After Fee
    l5->setText(((nBytes > 0) ? ASYMP_UTF8 : "") + QString::number(nBytes)); // Bytes
    l6->setText(sPriorityLabel); // Priority
    l7->setText(fDust ? tr("yes") : tr("no")); // Dust
    l8->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nChange)); // Change
    if (nPayFee > 0 && (coinControl->nMinimumTotalFee < nPayFee)) {
        l3->setText(ASYMP_UTF8 + l3->text());
        l4->setText(ASYMP_UTF8 + l4->text());
        if (nChange > 0 && !CoinControlDialog::fSubtractFeeFromAmount)
            l8->setText(ASYMP_UTF8 + l8->text());
    }

    l5->setStyleSheet((nBytes >= MAX_FREE_TRANSACTION_CREATE_SIZE) ? "color:red;" : ""); // Bytes >= 1000
    l6->setStyleSheet((dPriority > 0 && !fAllowFree) ? "color:red;" : ""); // Priority < "medium"
    l7->setStyleSheet((fDust) ? "color:red;" : ""); // Dust = "yes"

    QString toolTip1 = tr("This label turns red if the transaction size is greater than 1000 bytes.") + "<br /><br />";
    toolTip1 += tr("This means a fee of at least %1 per kB is required.").arg(BitcoinUnits::formatHtmlWithUnit(nDisplayUnit, CWallet::GetRequiredFee(1000))) + "<br /><br />";
    toolTip1 += tr("Can vary +/- 1 byte per input.");

    QString toolTip2 = tr("Transactions with higher priority are more likely to get included into a block.") + "<br /><br />";
    toolTip2 += tr("This label turns red if the priority is smaller than \"medium\".") + "<br /><br />";
    toolTip2 += tr("This means a fee of at least %1 per kB is required.").arg(BitcoinUnits::formatHtmlWithUnit(nDisplayUnit, CWallet::GetRequiredFee(1000)));

    QString toolTip3 = tr("This label turns red if any recipient receives an amount smaller than the current dust threshold.");

    double dFeeVary;
    if (payTxFee.GetFeePerK() > 0)
        dFeeVary = (double)std::max(CWallet::GetRequiredFee(1000), payTxFee.GetFeePerK()) / 1000;
    else {
        dFeeVary = (double)std::max(CWallet::GetRequiredFee(1000), mempool.estimateSmartFee(nTxConfirmTarget).GetFeePerK()) / 1000;
    }
    QString toolTip4 = tr("Can vary +/- %1 satoshi(s) per input.").arg(dFeeVary);

    l3->setToolTip(toolTip4);
    l4->setToolTip(toolTip4);
    l5->setToolTip(toolTip1);
    l6->setToolTip(toolTip2);
    l7->setToolTip(toolTip3);
    l8->setToolTip(toolTip4);
    dialog->findChild<QLabel*>("labelCoinControlFeeText")->setToolTip(l3->toolTip());
    dialog->findChild<QLabel*>("labelCoinControlAfterFeeText")->setToolTip(l4->toolTip());
    dialog->findChild<QLabel*>("labelCoinControlBytesText")->setToolTip(l5->toolTip());
    dialog->findChild<QLabel*>("labelCoinControlPriorityText")->setToolTip(l6->toolTip());
    dialog->findChild<QLabel*>("labelCoinControlLowOutputText")->setToolTip(l7->toolTip());
    dialog->findChild<QLabel*>("labelCoinControlChangeText")->setToolTip(l8->toolTip());

    QLabel* label = dialog->findChild<QLabel*>("labelCoinControlInsuffFunds");
    if (label)
        label->setVisible(nChange < 0);
}

void CoinControlDialog::updateView()
{
    if (!model || !model->getOptionsModel() || !model->getAddressTableModel())
        return;

    bool treeMode = ui->radioTreeMode->isChecked();

    ui->treeWidget->clear();
    ui->treeWidget->setEnabled(false); // performance, otherwise updateLabels would be called for every checked checkbox
    ui->treeWidget->setAlternatingRowColors(!treeMode);
    QFlags<Qt::ItemFlag> flgCheckbox = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
    QFlags<Qt::ItemFlag> flgTristate = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;

    int nDisplayUnit = model->getOptionsModel()->getDisplayUnit();
    double mempoolEstimatePriority = mempool.estimateSmartPriority(nTxConfirmTarget);

    std::map<QString, std::vector<COutput> > mapCoins;
    model->listCoins(mapCoins);

    BOOST_FOREACH (const PAIRTYPE(QString, std::vector<COutput>)&coins, mapCoins) {
        QTreeWidgetItem* itemWalletAddress = new QTreeWidgetItem();
        itemWalletAddress->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
        QString sWalletAddress = coins.first;
        QString sWalletLabel = model->getAddressTableModel()->labelForAddress(sWalletAddress);
        if (sWalletLabel.isEmpty())
            sWalletLabel = tr("(no label)");

        if (treeMode) {

            ui->treeWidget->addTopLevelItem(itemWalletAddress);

            itemWalletAddress->setFlags(flgTristate);
            itemWalletAddress->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

            itemWalletAddress->setText(COLUMN_LABEL, sWalletLabel);

            itemWalletAddress->setText(COLUMN_ADDRESS, sWalletAddress);
        }

        CAmount nSum = 0;
        double dPrioritySum = 0;
        int nChildren = 0;
        int nInputSum = 0;
        BOOST_FOREACH (const COutput& out, coins.second) {
            int nInputSize = 0;
            nSum += out.tx->vout[out.i].nValue;
            nChildren++;

            QTreeWidgetItem* itemOutput;
            if (treeMode)
                itemOutput = new QTreeWidgetItem(itemWalletAddress);
            else
                itemOutput = new QTreeWidgetItem(ui->treeWidget);
            itemOutput->setFlags(flgCheckbox);
            itemOutput->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

            CTxDestination outputAddress;
            QString sAddress = "";
            if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, outputAddress)) {
                sAddress = QString::fromStdString(CBitcoinAddress(outputAddress).ToString());

                if (!treeMode || (!(sAddress == sWalletAddress)))
                    itemOutput->setText(COLUMN_ADDRESS, sAddress);

                CPubKey pubkey;
                CKeyID* keyid = boost::get<CKeyID>(&outputAddress);
                if (keyid && model->getPubKey(*keyid, pubkey) && !pubkey.IsCompressed())
                    nInputSize = 29; // 29 = 180 - 151 (public key is 180 bytes, priority free area is 151 bytes)
            }

            if (!(sAddress == sWalletAddress)) // change
            {

                itemOutput->setToolTip(COLUMN_LABEL, tr("change from %1 (%2)").arg(sWalletLabel).arg(sWalletAddress));
                itemOutput->setText(COLUMN_LABEL, tr("(change)"));
            } else if (!treeMode) {
                QString sLabel = model->getAddressTableModel()->labelForAddress(sAddress);
                if (sLabel.isEmpty())
                    sLabel = tr("(no label)");
                itemOutput->setText(COLUMN_LABEL, sLabel);
            }

            itemOutput->setText(COLUMN_AMOUNT, BitcoinUnits::format(nDisplayUnit, out.tx->vout[out.i].nValue));
            itemOutput->setText(COLUMN_AMOUNT_INT64, strPad(QString::number(out.tx->vout[out.i].nValue), 15, " ")); // padding so that sorting works correctly

            itemOutput->setText(COLUMN_DATE, GUIUtil::dateTimeStr(out.tx->GetTxTime()));
            itemOutput->setText(COLUMN_DATE_INT64, strPad(QString::number(out.tx->GetTxTime()), 20, " "));

            itemOutput->setText(COLUMN_CONFIRMATIONS, strPad(QString::number(out.nDepth), 8, " "));

            double dPriority = ((double)out.tx->vout[out.i].nValue / (nInputSize + 78)) * (out.nDepth + 1); // 78 = 2 * 34 + 10
            itemOutput->setText(COLUMN_PRIORITY, CoinControlDialog::getPriorityLabel(dPriority, mempoolEstimatePriority));
            itemOutput->setText(COLUMN_PRIORITY_INT64, strPad(QString::number((int64_t)dPriority), 20, " "));
            dPrioritySum += (double)out.tx->vout[out.i].nValue * (out.nDepth + 1);
            nInputSum += nInputSize;

            uint256 txhash = out.tx->GetHash();
            itemOutput->setText(COLUMN_TXHASH, QString::fromStdString(txhash.GetHex()));

            itemOutput->setText(COLUMN_VOUT_INDEX, QString::number(out.i));

            if (model->isLockedCoin(txhash, out.i)) {
                COutPoint outpt(txhash, out.i);
                coinControl->UnSelect(outpt); // just to be sure
                itemOutput->setDisabled(true);
                itemOutput->setIcon(COLUMN_CHECKBOX, platformStyle->SingleColorIcon(":/icons/lock_closed"));
            }

            if (coinControl->IsSelected(COutPoint(txhash, out.i)))
                itemOutput->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
        }

        if (treeMode) {
            dPrioritySum = dPrioritySum / (nInputSum + 78);
            itemWalletAddress->setText(COLUMN_CHECKBOX, "(" + QString::number(nChildren) + ")");
            itemWalletAddress->setText(COLUMN_AMOUNT, BitcoinUnits::format(nDisplayUnit, nSum));
            itemWalletAddress->setText(COLUMN_AMOUNT_INT64, strPad(QString::number(nSum), 15, " "));
            itemWalletAddress->setText(COLUMN_PRIORITY, CoinControlDialog::getPriorityLabel(dPrioritySum, mempoolEstimatePriority));
            itemWalletAddress->setText(COLUMN_PRIORITY_INT64, strPad(QString::number((int64_t)dPrioritySum), 20, " "));
        }
    }

    if (treeMode) {
        for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked)
                ui->treeWidget->topLevelItem(i)->setExpanded(true);
    }

    sortView(sortColumn, sortOrder);
    ui->treeWidget->setEnabled(true);
}
