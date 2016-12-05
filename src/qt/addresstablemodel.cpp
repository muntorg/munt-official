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

#include "addresstablemodel.h"

#include "guiutil.h"
#include "walletmodel.h"

#include "base58.h"
#include "wallet/wallet.h"

#include <boost/foreach.hpp>

#include <QFont>
#include <QDebug>

const QString AddressTableModel::Send = "S";
const QString AddressTableModel::Receive = "R";

struct AddressTableEntry {
    enum Type {
        Sending,
        Receiving,
        Hidden /* QSortFilterProxyModel will filter these out */
    };

    Type type;
    QString label;
    QString address;

    AddressTableEntry() {}
    AddressTableEntry(Type type, const QString& label, const QString& address)
        : type(type)
        , label(label)
        , address(address)
    {
    }
};

struct AddressTableEntryLessThan {
    bool operator()(const AddressTableEntry& a, const AddressTableEntry& b) const
    {
        return a.address < b.address;
    }
    bool operator()(const AddressTableEntry& a, const QString& b) const
    {
        return a.address < b;
    }
    bool operator()(const QString& a, const AddressTableEntry& b) const
    {
        return a < b.address;
    }
};

/* Determine address type from address purpose */
static AddressTableEntry::Type translateTransactionType(const QString& strPurpose, bool isMine)
{
    AddressTableEntry::Type addressType = AddressTableEntry::Hidden;

    if (strPurpose == "send")
        addressType = AddressTableEntry::Sending;
    else if (strPurpose == "receive")
        addressType = AddressTableEntry::Receiving;
    else if (strPurpose == "unknown" || strPurpose == "") // if purpose not set, guess
        addressType = (isMine ? AddressTableEntry::Receiving : AddressTableEntry::Sending);
    return addressType;
}

class AddressTablePriv {
public:
    CWallet* wallet;
    QList<AddressTableEntry> cachedAddressTable;
    AddressTableModel* parent;

    AddressTablePriv(CWallet* wallet, AddressTableModel* parent)
        : wallet(wallet)
        , parent(parent)
    {
    }

    void refreshAddressTable()
    {
        cachedAddressTable.clear();
        {
            LOCK(wallet->cs_wallet);
            BOOST_FOREACH (const PAIRTYPE(std::string, CAddressBookData) & item, wallet->mapAddressBook) {
                const std::string& address = item.first;
                bool fMine = IsMine(*wallet, CBitcoinAddress(address).Get());
                AddressTableEntry::Type addressType = translateTransactionType(
                    QString::fromStdString(item.second.purpose), fMine);
                const std::string& strName = item.second.name;
                cachedAddressTable.append(AddressTableEntry(addressType,
                                                            QString::fromStdString(strName),
                                                            QString::fromStdString(address)));
            }
        }

        qSort(cachedAddressTable.begin(), cachedAddressTable.end(), AddressTableEntryLessThan());
    }

    void updateEntry(const QString& address, const QString& label, bool isMine, const QString& purpose, int status)
    {

        QList<AddressTableEntry>::iterator lower = qLowerBound(
            cachedAddressTable.begin(), cachedAddressTable.end(), address, AddressTableEntryLessThan());
        QList<AddressTableEntry>::iterator upper = qUpperBound(
            cachedAddressTable.begin(), cachedAddressTable.end(), address, AddressTableEntryLessThan());
        int lowerIndex = (lower - cachedAddressTable.begin());
        int upperIndex = (upper - cachedAddressTable.begin());
        bool inModel = (lower != upper);
        AddressTableEntry::Type newEntryType = translateTransactionType(purpose, isMine);

        switch (status) {
        case CT_NEW:
            if (inModel) {
                qWarning() << "AddressTablePriv::updateEntry: Warning: Got CT_NEW, but entry is already in model";
                break;
            }
            parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex);
            cachedAddressTable.insert(lowerIndex, AddressTableEntry(newEntryType, label, address));
            parent->endInsertRows();
            break;
        case CT_UPDATED:
            if (!inModel) {
                qWarning() << "AddressTablePriv::updateEntry: Warning: Got CT_UPDATED, but entry is not in model";
                break;
            }
            lower->type = newEntryType;
            lower->label = label;
            parent->emitDataChanged(lowerIndex);
            break;
        case CT_DELETED:
            if (!inModel) {
                qWarning() << "AddressTablePriv::updateEntry: Warning: Got CT_DELETED, but entry is not in model";
                break;
            }
            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex - 1);
            cachedAddressTable.erase(lower, upper);
            parent->endRemoveRows();
            break;
        }
    }

    int size()
    {
        return cachedAddressTable.size();
    }

    AddressTableEntry* index(int idx)
    {
        if (idx >= 0 && idx < cachedAddressTable.size()) {
            return &cachedAddressTable[idx];
        } else {
            return 0;
        }
    }
};

AddressTableModel::AddressTableModel(CWallet* wallet, WalletModel* parent)
    : QAbstractTableModel(parent)
    , walletModel(parent)
    , wallet(wallet)
    , priv(0)
{
    columns << tr("Label") << tr("Address");
    priv = new AddressTablePriv(wallet, this);
    priv->refreshAddressTable();
}

AddressTableModel::~AddressTableModel()
{
    delete priv;
}

int AddressTableModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int AddressTableModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant AddressTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    AddressTableEntry* rec = static_cast<AddressTableEntry*>(index.internalPointer());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case Label:
            if (rec->label.isEmpty() && role == Qt::DisplayRole) {
                return tr("(no label)");
            } else {
                return rec->label;
            }
        case Address:
            return rec->address;
        }
    }
    /*else if (role == Qt::FontRole)
    {
        QFont font;fixedPitchFont
        if(index.column() == Address)
        {
            font = GUIUtil::fixedPitchFont();
        }
        return font;
    }*/
    else if (role == TypeRole) {
        switch (rec->type) {
        case AddressTableEntry::Sending:
            return Send;
        case AddressTableEntry::Receiving:
            return Receive;
        default:
            break;
        }
    } else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case Address:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    }
    return QVariant();
}

bool AddressTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;
    AddressTableEntry* rec = static_cast<AddressTableEntry*>(index.internalPointer());
    std::string strPurpose = (rec->type == AddressTableEntry::Sending ? "send" : "receive");
    editStatus = OK;

    if (role == Qt::EditRole) {
        LOCK(wallet->cs_wallet); /* For SetAddressBook / DelAddressBook */
        std::string curAddress = rec->address.toStdString();
        if (index.column() == Label) {

            if (rec->label == value.toString()) {
                editStatus = NO_CHANGES;
                return false;
            }
            wallet->SetAddressBook(curAddress, value.toString().toStdString(), strPurpose);
        } else if (index.column() == Address) {
            QString newAddress = value.toString();

            if (!walletModel->validateAddress(newAddress) && !walletModel->validateAddressBCOIN(newAddress) && !walletModel->validateAddressIBAN(newAddress)) {
                editStatus = INVALID_ADDRESS;
                return false;
            }

            else if (newAddress.toStdString() == curAddress) {
                editStatus = NO_CHANGES;
                return false;
            }

            else if (wallet->mapAddressBook.count(newAddress.toStdString())) {
                editStatus = DUPLICATE_ADDRESS;
                return false;
            }

            else if (rec->type == AddressTableEntry::Sending) {

                wallet->DelAddressBook(curAddress);

                wallet->SetAddressBook(newAddress.toStdString(), rec->label.toStdString(), strPurpose);
            }
        }
        return true;
    }
    return false;
}

QVariant AddressTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole && section < columns.size()) {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags AddressTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return 0;
    AddressTableEntry* rec = static_cast<AddressTableEntry*>(index.internalPointer());

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (rec->type == AddressTableEntry::Sending || (rec->type == AddressTableEntry::Receiving && index.column() == Label)) {
        retval |= Qt::ItemIsEditable;
    }
    return retval;
}

QModelIndex AddressTableModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    AddressTableEntry* data = priv->index(row);
    if (data) {
        return createIndex(row, column, priv->index(row));
    } else {
        return QModelIndex();
    }
}

void AddressTableModel::updateEntry(const QString& address,
                                    const QString& label, bool isMine, const QString& purpose, int status)
{

    priv->updateEntry(address, label, isMine, purpose, status);
}

QString AddressTableModel::addRow(const QString& type, const QString& label, const QString& address)
{
    std::string strLabel = label.toStdString();
    std::string strAddress = address.toStdString();

    std::string strAccount = "";
    editStatus = OK;

    if (type == Send) {
        if (!walletModel->validateAddress(address) && !walletModel->validateAddressBCOIN(address) && !walletModel->validateAddressIBAN(address)) {
            editStatus = INVALID_ADDRESS;
            return QString();
        }

        {
            LOCK(wallet->cs_wallet);
            if (wallet->mapAddressBook.count(strAddress)) {
                editStatus = DUPLICATE_ADDRESS;
                return QString();
            }
        }
    } else if (type == Receive) {

        CPubKey newKey;

        LOCK(wallet->cs_wallet);

        if (wallet->mapAccounts.count(strAccount) == 0)
            return QString();

        if (!wallet->GetKeyFromPool(newKey, wallet->mapAccounts[strAccount], KEYCHAIN_EXTERNAL)) {
            WalletModel::UnlockContext ctx(walletModel->requestUnlock());
            if (!ctx.isValid()) {

                editStatus = WALLET_UNLOCK_FAILURE;
                return QString();
            }
            if (!wallet->GetKeyFromPool(newKey, wallet->mapAccounts[strAccount], KEYCHAIN_EXTERNAL)) {
                editStatus = KEY_GENERATION_FAILURE;
                return QString();
            }
        }
        strAddress = CBitcoinAddress(newKey.GetID()).ToString();
    } else {
        return QString();
    }

    {
        LOCK(wallet->cs_wallet);
        wallet->SetAddressBook(strAddress, strLabel,
                               (type == Send ? "send" : "receive"));
    }
    return QString::fromStdString(strAddress);
}

bool AddressTableModel::removeRows(int row, int count, const QModelIndex& parent)
{
    Q_UNUSED(parent);
    AddressTableEntry* rec = priv->index(row);
    if (count != 1 || !rec || rec->type == AddressTableEntry::Receiving) {

        return false;
    }
    {
        LOCK(wallet->cs_wallet);
        wallet->DelAddressBook(rec->address.toStdString());
    }
    return true;
}

/* Look up label for address in address book, if not found return empty string.
 */
QString AddressTableModel::labelForAddress(const QString& address) const
{
    {
        LOCK(wallet->cs_wallet);
        std::map<std::string, CAddressBookData>::iterator mi = wallet->mapAddressBook.find(address.toStdString());
        if (mi != wallet->mapAddressBook.end()) {
            return QString::fromStdString(mi->second.name);
        }
    }
    return QString();
}

int AddressTableModel::lookupAddress(const QString& address) const
{
    QModelIndexList lst = match(index(0, Address, QModelIndex()),
                                Qt::EditRole, address, 1, Qt::MatchExactly);
    if (lst.isEmpty()) {
        return -1;
    } else {
        return lst.at(0).row();
    }
}

void AddressTableModel::emitDataChanged(int idx)
{
    Q_EMIT dataChanged(index(idx, 0, QModelIndex()), index(idx, columns.length() - 1, QModelIndex()));
}
