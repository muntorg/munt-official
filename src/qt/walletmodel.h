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

#ifndef BITCOIN_QT_WALLETMODEL_H
#define BITCOIN_QT_WALLETMODEL_H

#include "paymentrequestplus.h"
#include "walletmodeltransaction.h"

#include "support/allocators/secure.h"
#include "account.h"

#include <map>
#include <vector>

#include <QObject>
#include <QRegularExpression>

class AddressTableModel;
class AccountTableModel;
class OptionsModel;
class PlatformStyle;
class RecentRequestsTableModel;
class TransactionTableModel;
class WalletModelTransaction;

class CCoinControl;
class CKeyID;
class COutPoint;
class COutput;
class CPubKey;
class CWallet;
class uint256;
class CAccount;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class SendCoinsRecipient {
public:
    explicit SendCoinsRecipient()
        : amount(0)
        , fSubtractFeeFromAmount(false)
        , nVersion(SendCoinsRecipient::CURRENT_VERSION)
    {
    }
    explicit SendCoinsRecipient(const QString& addr, const QString& label, const CAmount& amount, const QString& message)
        : address(addr)
        , label(label)
        , amount(amount)
        , message(message)
        , fSubtractFeeFromAmount(false)
        , paymentType(PaymentType::NormalPayment)
        , nVersion(SendCoinsRecipient::CURRENT_VERSION)
    {
    }

    QString address;
    QString label;
    CAmount amount;

    QString message;

    PaymentRequestPlus paymentRequest;

    QString authenticatedMerchant;

    enum PaymentType {
        NormalPayment,
        IBANPayment,
        BCOINPayment,
        InvalidPayment
    };
    bool fSubtractFeeFromAmount; // memory only
    bool addToAddressBook; //memory only
    PaymentType paymentType; //memory only
    PaymentType forexPaymentType; //memory only
    QString forexAddress; //memory only
    CAmount forexAmount; //memory only
    std::string forexFailCode; //memory only
    int64_t expiry; //memory only

    static const int CURRENT_VERSION = 1;
    int nVersion;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        std::string sAddress = address.toStdString();
        std::string sLabel = label.toStdString();
        std::string sMessage = message.toStdString();
        std::string sPaymentRequest;
        if (!ser_action.ForRead() && paymentRequest.IsInitialized())
            paymentRequest.SerializeToString(&sPaymentRequest);
        std::string sAuthenticatedMerchant = authenticatedMerchant.toStdString();

        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(sAddress);
        READWRITE(sLabel);
        READWRITE(amount);
        READWRITE(sMessage);
        READWRITE(sPaymentRequest);
        READWRITE(sAuthenticatedMerchant);

        if (ser_action.ForRead()) {
            address = QString::fromStdString(sAddress);
            label = QString::fromStdString(sLabel);
            message = QString::fromStdString(sMessage);
            if (!sPaymentRequest.empty())
                paymentRequest.parse(QByteArray::fromRawData(sPaymentRequest.data(), sPaymentRequest.size()));
            authenticatedMerchant = QString::fromStdString(sAuthenticatedMerchant);
        }
    }
};

/** Interface to Bitcoin wallet from Qt view code. */
class WalletModel : public QObject {
    Q_OBJECT

public:
    explicit WalletModel(const PlatformStyle* platformStyle, CWallet* wallet, OptionsModel* optionsModel, QObject* parent = 0);
    ~WalletModel();

    enum StatusCode // Returned by sendCoins
    {
        OK,
        InvalidAmount,
        InvalidAddress,
        AmountExceedsBalance,
        AmountWithFeeExceedsBalance,
        DuplicateAddress,
        TransactionCreationFailed, // Error returned when wallet is still locked
        TransactionCommitFailed,
        AbsurdFee,
        PaymentRequestExpired,
        ForexFailed
    };

    enum EncryptionStatus {
        Unencrypted, // !wallet->IsCrypted()
        Locked, // wallet->IsCrypted() && wallet->IsLocked()
        Unlocked // wallet->IsCrypted() && !wallet->IsLocked()
    };

    OptionsModel* getOptionsModel();
    AddressTableModel* getAddressTableModel();
    AccountTableModel* getAccountTableModel();
    TransactionTableModel* getTransactionTableModel();
    RecentRequestsTableModel* getRecentRequestsTableModel();

    CAmount getBalance(CAccount* forAccount = NULL, const CCoinControl* coinControl = NULL) const;
    CAmount getUnconfirmedBalance(CAccount* forAccount = NULL) const;
    CAmount getImmatureBalance() const;
    bool haveWatchOnly() const;
    CAmount getWatchBalance() const;
    CAmount getWatchUnconfirmedBalance() const;
    CAmount getWatchImmatureBalance() const;
    EncryptionStatus getEncryptionStatus() const;

    bool validateAddress(const QString& address);
    bool validateAddressBCOIN(const QString& address);
    bool validateAddressIBAN(const QString& address);

    struct SendCoinsReturn {
        SendCoinsReturn(StatusCode status = OK)
            : status(status)
        {
        }
        StatusCode status;
    };

    SendCoinsReturn prepareTransaction(CAccount* forAccount, WalletModelTransaction& transaction, const CCoinControl* coinControl = NULL);

    SendCoinsReturn sendCoins(WalletModelTransaction& transaction);

    bool setWalletEncrypted(bool encrypted, const SecureString& passphrase);

    bool setWalletLocked(bool locked, const SecureString& passPhrase = SecureString());
    bool changePassphrase(const SecureString& oldPass, const SecureString& newPass);

    bool backupWallet(const QString& filename);

    class UnlockContext {
    public:
        UnlockContext(WalletModel* wallet, bool valid, bool relock);
        ~UnlockContext();

        bool isValid() const { return valid; }

        UnlockContext(const UnlockContext& obj) { CopyFrom(obj); }
        UnlockContext& operator=(const UnlockContext& rhs)
        {
            CopyFrom(rhs);
            return *this;
        }

    private:
        WalletModel* wallet;
        bool valid;
        mutable bool relock; // mutable, as it can be set to false by copying

        void CopyFrom(const UnlockContext& rhs);
    };

    UnlockContext requestUnlock();

    bool getPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const;
    bool havePrivKey(const CKeyID& address) const;
    void getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs);
    bool isSpent(const COutPoint& outpoint) const;
    void listCoins(std::map<QString, std::vector<COutput> >& mapCoins) const;

    bool isLockedCoin(uint256 hash, unsigned int n) const;
    void lockCoin(COutPoint& output);
    void unlockCoin(COutPoint& output);
    void listLockedCoins(std::vector<COutPoint>& vOutpts);

    void loadReceiveRequests(std::vector<std::string>& vReceiveRequests);
    bool saveReceiveRequest(const std::string& sAddress, const int64_t nId, const std::string& sRequest);

    bool transactionCanBeAbandoned(uint256 hash) const;
    bool abandonTransaction(uint256 hash) const;

    void setActiveAccount(CAccount* account);
    CAccount* getActiveAccount();
    QString getAccountLabel(std::string uuid);

private:
    CWallet* wallet;
    bool fHaveWatchOnly;
    bool fForceCheckBalanceChanged;

    OptionsModel* optionsModel;

    AddressTableModel* addressTableModel;
    AccountTableModel* accountTableModel;
    TransactionTableModel* transactionTableModel;
    RecentRequestsTableModel* recentRequestsTableModel;

    CAmount cachedBalance;
    CAmount cachedUnconfirmedBalance;
    CAmount cachedImmatureBalance;
    CAmount cachedWatchOnlyBalance;
    CAmount cachedWatchUnconfBalance;
    CAmount cachedWatchImmatureBalance;
    EncryptionStatus cachedEncryptionStatus;
    int cachedNumBlocks;

    QTimer* pollTimer;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
    void checkBalanceChanged();

    QRegularExpression patternMatcherIBAN;

Q_SIGNALS:

    void balanceChanged(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                        const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance);

    void encryptionStatusChanged(int status);

    void requireUnlock();

    void message(const QString& title, const QString& message, unsigned int style);

    void coinsSent(CWallet* wallet, SendCoinsRecipient recipient, QByteArray transaction);

    void showProgress(const QString& title, int nProgress);

    void notifyWatchonlyChanged(bool fHaveWatchonly);

    void activeAccountChanged(CAccount* account);
    void accountListChanged();
    void accountAdded(CAccount* account);
    void accountDeleted(CAccount* account);

public Q_SLOTS:
    /* Wallet status might have changed */
    void updateStatus();
    /* New transaction, or transaction changed status */
    void updateTransaction();
    /* New, updated or removed address book entry */
    void updateAddressBook(const QString& address, const QString& label, bool isMine, const QString& purpose, int status);
    /* Watch-only added */
    void updateWatchOnlyFlag(bool fHaveWatchonly);
    /* Current, immature or unconfirmed balance might have changed - emit 'balanceChanged' if so */
    void pollBalanceChanged();
};

#endif // BITCOIN_QT_WALLETMODEL_H
