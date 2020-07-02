// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2020 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "addresstablemodel.h"

#include "guiutil.h"
#include "walletmodel.h"

#include "base58.h"
#include "wallet/wallet.h"

#define BOOST_UUID_RANDOM_PROVIDER_FORCE_POSIX
#include <boost/uuid/nil_generator.hpp>

#include <QFont>
#include <QDebug>

const QString AddressTableModel::Send = "S";
const QString AddressTableModel::Receive = "R";

struct AddressTableEntry
{
    enum Type {
        Sending,
        Receiving,
        Hidden /* QSortFilterProxyModel will filter these out */
    };

    QString address;
    QString label;
    QString description;
    Type type;


    AddressTableEntry() {}
    AddressTableEntry(const QString &_address, const QString &_label, const QString &_description, Type _type)
    : address(_address)
    , label(_label)
    , description(_description)
    , type(_type)
    {}
};

struct AddressTableEntryLessThan
{
    bool operator()(const AddressTableEntry &a, const AddressTableEntry &b) const
    {
        return a.address < b.address;
    }
    bool operator()(const AddressTableEntry &a, const QString &b) const
    {
        return a.address < b;
    }
    bool operator()(const QString &a, const AddressTableEntry &b) const
    {
        return a < b.address;
    }
};

/* Determine address type from address purpose */
static AddressTableEntry::Type translateTransactionType(const QString &strPurpose, bool isMine)
{
    AddressTableEntry::Type addressType = AddressTableEntry::Hidden;
    // "refund" addresses aren't shown, and change addresses aren't in mapAddressBook at all.
    if (strPurpose == "send")
        addressType = AddressTableEntry::Sending;
    else if (strPurpose == "receive")
        addressType = AddressTableEntry::Receiving;
    else if (strPurpose == "unknown" || strPurpose == "") // if purpose not set, guess
        addressType = (isMine ? AddressTableEntry::Receiving : AddressTableEntry::Sending);
    return addressType;
}

// Private implementation
class AddressTablePriv
{
public:
    CWallet *wallet;
    QList<AddressTableEntry> cachedAddressTable;
    AddressTableModel *parent;

    AddressTablePriv(CWallet *_wallet, AddressTableModel *_parent):
        wallet(_wallet), parent(_parent) {}

    void refreshAddressTable()
    {
        cachedAddressTable.clear();
        {
            LOCK(wallet->cs_wallet);
            for(const auto&[address, data] : wallet->mapAddressBook)
            {
                bool fMine = IsMine(*wallet, CNativeAddress(address).Get());
                AddressTableEntry::Type addressType = translateTransactionType(QString::fromStdString(data.purpose), fMine);
                cachedAddressTable.append(AddressTableEntry(QString::fromStdString(address), QString::fromStdString(data.name), QString::fromStdString(data.description), addressType));
            }
        }
        // std::upper_bound() and std::lower_bound() require our cachedAddressTable list to be sorted in asc order
        // Even though the map is already sorted this re-sorting step is needed because the originating map
        // is sorted by binary address, not by base58() address.
        std::sort(cachedAddressTable.begin(), cachedAddressTable.end(), AddressTableEntryLessThan());
    }

    void updateEntry(const QString& address, const QString& label, const QString& description, bool isMine, const QString& purpose, int status)
    {
        // Find address / label in model
        QList<AddressTableEntry>::iterator lower = std::lower_bound(
            cachedAddressTable.begin(), cachedAddressTable.end(), address, AddressTableEntryLessThan());
        QList<AddressTableEntry>::iterator upper = std::upper_bound(
            cachedAddressTable.begin(), cachedAddressTable.end(), address, AddressTableEntryLessThan());
        int lowerIndex = (lower - cachedAddressTable.begin());
        int upperIndex = (upper - cachedAddressTable.begin());
        bool inModel = (lower != upper);
        AddressTableEntry::Type newEntryType = translateTransactionType(purpose, isMine);

        switch(status)
        {
        case CT_NEW:
            if(inModel)
            {
                qWarning() << "AddressTablePriv::updateEntry: Warning: Got CT_NEW, but entry is already in model";
                break;
            }
            parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex);
            cachedAddressTable.insert(lowerIndex, AddressTableEntry(address, label, description, newEntryType));
            parent->endInsertRows();
            break;
        case CT_UPDATED:
            if(!inModel)
            {
                qWarning() << "AddressTablePriv::updateEntry: Warning: Got CT_UPDATED, but entry is not in model";
                break;
            }
            lower->type = newEntryType;
            lower->label = label;
            parent->emitDataChanged(lowerIndex);
            break;
        case CT_DELETED:
            if(!inModel)
            {
                qWarning() << "AddressTablePriv::updateEntry: Warning: Got CT_DELETED, but entry is not in model";
                break;
            }
            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex-1);
            cachedAddressTable.erase(lower, upper);
            parent->endRemoveRows();
            break;
        }
    }

    int size()
    {
        return cachedAddressTable.size();
    }

    AddressTableEntry *index(int idx)
    {
        if(idx >= 0 && idx < cachedAddressTable.size())
        {
            return &cachedAddressTable[idx];
        }
        else
        {
            return 0;
        }
    }
};

AddressTableModel::AddressTableModel(CWallet *_wallet, WalletModel *parent)
: QAbstractTableModel(parent)
, walletModel(parent)
, wallet(_wallet)
, priv(nullptr)
{
    columns << tr("Label") << tr("Description") << tr("Address");
    priv = new AddressTablePriv(wallet, this);
    priv->refreshAddressTable();
}

AddressTableModel::~AddressTableModel()
{
    LogPrintf("AddressTableModel::~AddressTableModel\n");
    delete priv;
}

int AddressTableModel::rowCount(const QModelIndex &parent) const
{
    (unused)parent;
    return priv->size();
}

int AddressTableModel::columnCount(const QModelIndex &parent) const
{
    (unused)parent;
    return columns.length();
}

QVariant AddressTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    AddressTableEntry *rec = static_cast<AddressTableEntry*>(index.internalPointer());

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(index.column())
        {
            case ColumnIndex::Label:
            {
                if(rec->label.isEmpty() && role == Qt::DisplayRole)
                {
                    return tr("(no label)");
                }
                else
                {
                    return rec->label;
                }
            }
            case ColumnIndex::Description:
            {
                if(rec->description.isEmpty() && role == Qt::DisplayRole)
                {
                    return tr("(no description)");
                }
                else
                {
                    return rec->description;
                }
            }
            case ColumnIndex::Address:
            {
                return rec->address;
            }
        }
    }
    /* GULDEN - no fixed pitch font.
    else if (role == Qt::FontRole)
    {
        QFont font;
        if(index.column() == Address)
        {
            font = GUIUtil::fixedPitchFont();
        }
        return font;
    }
    */
    else if (role == TypeRole)
    {
        switch(rec->type)
        {
            case AddressTableEntry::Sending:
                return Send;
            case AddressTableEntry::Receiving:
                return Receive;
            case AddressTableEntry::Hidden:
            default: break;
        }
    }
    //Align address table to right, all other columns to left.
    else if(role == Qt::TextAlignmentRole)
    {
        switch(index.column())
        {
            case Address:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    }
    return QVariant();
}

bool AddressTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid())
        return false;
    AddressTableEntry *rec = static_cast<AddressTableEntry*>(index.internalPointer());
    std::string strPurpose = (rec->type == AddressTableEntry::Sending ? "send" : "receive");
    editStatus = OK;

    if(role == Qt::EditRole)
    {
        LOCK(wallet->cs_wallet); /* For SetAddressBook / DelAddressBook */
        std::string curAddress = rec->address.toStdString();
        std::string curLabel = rec->label.toStdString();
        std::string curDesc = rec->description.toStdString();
        std::string val = value.toString().toStdString();
        if(index.column() == ColumnIndex::Label)
        {
            // Do nothing, if old label == new label
            if(curLabel == val)
            {
                editStatus = NO_CHANGES;
                return false;
            }
            wallet->SetAddressBook(curAddress, val, curDesc, strPurpose);
        }
        else if(index.column() == ColumnIndex::Description)
        {
            // Do nothing, if old description == new description
            if(curDesc == val)
            {
                editStatus = NO_CHANGES;
                return false;
            }
            wallet->SetAddressBook(curAddress, curLabel, val, strPurpose);
        }
        else if(index.column() == Address)
        {
            QString newAddress = value.toString();
            // Refuse to set invalid address, set error status and return false
            if( !walletModel->validateAddress(newAddress) && !walletModel->validateAddressBitcoin(newAddress) && !walletModel->validateAddressIBAN(newAddress) )
            {
                editStatus = INVALID_ADDRESS;
                return false;
            }
            // Do nothing, if old address == new address
            else if(newAddress.toStdString() == curAddress)
            {
                editStatus = NO_CHANGES;
                return false;
            }
            // Check for duplicate addresses to prevent accidental deletion of addresses, if you try
            // to paste an existing address over another address (with a different label)
            else if(wallet->mapAddressBook.count(newAddress.toStdString()))
            {
                editStatus = DUPLICATE_ADDRESS;
                return false;
            }
            // Double-check that we're not overwriting a receiving address
            else if(rec->type == AddressTableEntry::Sending)
            {
                // Remove old entry
                wallet->DelAddressBook(curAddress);
                // Add new entry with new address
                wallet->SetAddressBook(newAddress.toStdString(), curLabel, curDesc, strPurpose);
            }
        }
        return true;
    }
    return false;
}

QVariant AddressTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < columns.size())
        {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags AddressTableModel::flags(const QModelIndex &index) const
{
    if(!index.isValid())
        return 0;
    AddressTableEntry *rec = static_cast<AddressTableEntry*>(index.internalPointer());

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    // Can edit everything for sending addresses,
    // Everything except address for receiving addresses
    if(rec->type == AddressTableEntry::Sending || (rec->type == AddressTableEntry::Receiving && index.column()!=ColumnIndex::Address))
    {
        retval |= Qt::ItemIsEditable;
    }
    return retval;
}

QModelIndex AddressTableModel::index(int row, int column, const QModelIndex &parent) const
{
    (unused)parent;
    AddressTableEntry *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

void AddressTableModel::updateEntry(const QString& address, const QString& label, const QString& description, bool isMine, const QString &purpose, int status)
{
    // Update address book model from Gulden core
    priv->updateEntry(address, label, description, isMine, purpose, status);
}

QString AddressTableModel::addRow(const QString& address, const QString& label, const QString& description, const QString& type)
{
    std::string strLabel = label.toStdString();
    std::string strDescription = description.toStdString();
    std::string strAddress = address.toStdString();

    //fixme: (FUT)
    boost::uuids::uuid accountUUID = boost::uuids::nil_generator()();
    editStatus = OK;

    if (!walletModel)
        return "";

    if(type == Send)
    {
        if( !walletModel->validateAddress(address) && !walletModel->validateAddressBitcoin(address) && !walletModel->validateAddressIBAN(address) )
        {
            editStatus = INVALID_ADDRESS;
            return "";
        }
        // Check for duplicate addresses
        {
            LOCK(wallet->cs_wallet);
            if(wallet->mapAddressBook.count(strAddress))
            {
                editStatus = DUPLICATE_ADDRESS;
                return "";
            }
        }
    }
    else if(type == Receive)
    {
        // Generate a new address to associate with given label
        CPubKey newKey;

        LOCK(wallet->cs_wallet);

        if(wallet->mapAccounts.count(accountUUID) == 0)
            return QString();

        if(!wallet->GetKeyFromPool(newKey, wallet->mapAccounts[accountUUID], KEYCHAIN_EXTERNAL))
        {
            WalletModel::UnlockContext ctx(walletModel->requestUnlock());
            if(!ctx.isValid())
            {
                // Unlock wallet failed or was cancelled
                editStatus = WALLET_UNLOCK_FAILURE;
                return QString();
            }
            if(!wallet->GetKeyFromPool(newKey, wallet->mapAccounts[accountUUID], KEYCHAIN_EXTERNAL))
            {
                editStatus = KEY_GENERATION_FAILURE;
                return QString();
            }
        }
        strAddress = CNativeAddress(newKey.GetID()).ToString();
    }
    else
    {
        return QString();
    }

    // Add entry
    {
        LOCK(wallet->cs_wallet);
        wallet->SetAddressBook(strAddress, strLabel, strDescription, (type == Send ? "send" : "receive"));
    }
    return QString::fromStdString(strAddress);
}

bool AddressTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    (unused)parent;
    AddressTableEntry *rec = priv->index(row);
    if(count != 1 || !rec || rec->type == AddressTableEntry::Receiving)
    {
        // Can only remove one row at a time, and cannot remove rows not in model.
        // Also refuse to remove receiving addresses.
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
QString AddressTableModel::labelForAddress(const QString &address) const
{
    {
        LOCK(wallet->cs_wallet);
        std::map<std::string, CAddressBookData>::iterator mi = wallet->mapAddressBook.find(address.toStdString());
        if (mi != wallet->mapAddressBook.end())
        {
            return QString::fromStdString(mi->second.name);
        }
    }
    return QString();
}

int AddressTableModel::lookupAddress(const QString &address) const
{
    QModelIndexList lst = match(index(0, Address, QModelIndex()),
                                Qt::EditRole, address, 1, Qt::MatchExactly);
    if(lst.isEmpty())
    {
        return -1;
    }
    else
    {
        return lst.at(0).row();
    }
}

void AddressTableModel::emitDataChanged(int idx)
{
    Q_EMIT dataChanged(index(idx, 0, QModelIndex()), index(idx, columns.length()-1, QModelIndex()));
}
