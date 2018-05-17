// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_WALLET_WALLETTX_H
#define GULDEN_WALLET_WALLETTX_H

#include "merkletx.h"
#include <uint256.h>
#include "amount.h"
#include <script/standard.h>
#include <script/ismine.h>

class CAccount;
class CWallet;
class CKeyStore;
class CConnman;



typedef std::map<std::string, std::string> mapValue_t;


static inline void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n"))
    {
        nOrderPos = -1; // TODO: calculate elsewhere
        return;
    }
    nOrderPos = atoi64(mapValue["n"].c_str());
}


static inline void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
}

struct COutputEntry
{
    CTxDestination destination;
    CAmount amount;
    int vout;
};

/** 
 * A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

public:
    /**
     * Key/value map with information about the transaction.
     *
     * The following keys can be read and written through the map and are
     * serialized in the wallet database:
     *
     *     "comment", "to"   - comment strings provided to sendtoaddress,
     *                         sendfrom, sendmany wallet RPCs
     *     "replaces_txid"   - txid (as HexStr) of transaction replaced by
     *                         bumpfee on transaction created by bumpfee
     *     "replaced_by_txid" - txid (as HexStr) of transaction created by
     *                         bumpfee on transaction replaced by bumpfee
     *     "from", "message" - obsolete fields that could be set in UI prior to
     *                         2011 (removed in commit 4d9b223)
     *
     * The following keys are serialized in the wallet database, but shouldn't
     * be read or written through the map (they will be temporarily added and
     * removed from the map during serialization):
     *
     *     "fromaccount"     - serialized strFromAccount value
     *     "n"               - serialized nOrderPos value
     *     "timesmart"       - serialized nTimeSmart value
     *     "spent"           - serialized vfSpent value that existed prior to
     *                         2014 (removed in commit 93a18a3)
     */
    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived; //!< time received by this node
    /**
     * Stable timestamp that never changes, and reflects the order a transaction
     * was added to the wallet. Timestamp is based on the block time for a
     * transaction added as part of a block, or else the time when the
     * transaction was received if it wasn't part of a block, with the timestamp
     * adjusted in both cases so timestamp order matches the order transactions
     * were added to the wallet. More details can be found in
     * CWallet::ComputeTimeSmart().
     */
    unsigned int nTimeSmart;
    /**
     * From me flag is set to 1 for transactions that were created by the wallet
     * on this Gulden node, and set to 0 for transactions that were created
     * externally and came in through the network or sendrawtransaction RPC.
     */
    char fFromMe;
    std::string strFromAccount;
    int64_t nOrderPos; //!< position in ordered transaction list

    // memory only
    mutable bool fChangeCached;
    mutable CAmount nChangeCached;
    mutable std::map<const CAccount*, CAmount> debitCached;
    mutable std::map<const CAccount*, CAmount> creditCached;
    mutable std::map<const CAccount*, CAmount> immatureCreditCached;
    mutable std::map<const CAccount*, CAmount> availableCreditCached;
    mutable std::map<const CAccount*, CAmount> availableCreditCachedIncludingLockedWitnesses;
    mutable std::map<const CAccount*, CAmount> watchDebitCached;
    mutable std::map<const CAccount*, CAmount> watchCreditCached;
    mutable std::map<const CAccount*, CAmount> immatureWatchCreditCached;
    mutable std::map<const CAccount*, CAmount> availableWatchCreditCached;
    void clearAllCaches()
    {
        fChangeCached = false;
        nChangeCached = 0;
        debitCached.clear();
        creditCached.clear();
        immatureCreditCached.clear();
        availableCreditCached.clear();
        availableCreditCachedIncludingLockedWitnesses.clear();
        watchDebitCached.clear();
        watchCreditCached.clear();
        immatureWatchCreditCached.clear();
        availableWatchCreditCached.clear();
    }

    CWalletTx()
    {
        Init(NULL);
    }

    CWalletTx(const CWallet* pwalletIn, CTransactionRef arg) : CMerkleTx(std::move(arg))
    {
        Init(pwalletIn);
    }

    void Init(const CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        strFromAccount.clear();
        //fDebitCached = false;
        //fCreditCached = false;
        //fImmatureCreditCached = false;
        //fAvailableCreditCached = false;
        //fWatchDebitCached = false;
        //fWatchCreditCached = false;
        //fImmatureWatchCreditCached = false;
        //fAvailableWatchCreditCached = false;
        fChangeCached = false;
        //nDebitCached = 0;
        //nCreditCached = 0;
        //nImmatureCreditCached = 0;
        //nAvailableCreditCached = 0;
        //nWatchDebitCached = 0;
        //nWatchCreditCached = 0;
        //nAvailableWatchCreditCached = 0;
        //nImmatureWatchCreditCached = 0;
        debitCached.clear();
        creditCached.clear();
        immatureCreditCached.clear();
        availableCreditCached.clear();
        availableCreditCachedIncludingLockedWitnesses.clear();
        watchDebitCached.clear();
        watchCreditCached.clear();
        immatureWatchCreditCached.clear();
        availableWatchCreditCached.clear();
        nChangeCached = 0;
        nOrderPos = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (ser_action.ForRead())
            Init(NULL);
        char fSpent = false;

        if (!ser_action.ForRead())
        {
            mapValue["fromaccount"] = strFromAccount;

            WriteOrderPos(nOrderPos, mapValue);

            if (nTimeSmart)
                mapValue["timesmart"] = strprintf("%u", nTimeSmart);
        }

        READWRITE(*(CMerkleTx*)this);
        std::vector<CMerkleTx> vUnused; //!< Used to be vtxPrev
        READWRITECOMPACTSIZEVECTOR(vUnused);
        READWRITE(mapValue);
        READWRITECOMPACTSIZEVECTOR(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (ser_action.ForRead())
        {
            strFromAccount = mapValue["fromaccount"];

            ReadOrderPos(nOrderPos, mapValue);

            nTimeSmart = mapValue.count("timesmart") ? (unsigned int)atoi64(mapValue["timesmart"]) : 0;
        }

        mapValue.erase("fromaccount");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
    }

    //! make sure balances are recalculated
    void MarkDirty()
    {
        fChangeCached = false;
        debitCached.clear();
        creditCached.clear();
        immatureCreditCached.clear();
        availableCreditCached.clear();
        watchDebitCached.clear();
        watchCreditCached.clear();
        immatureWatchCreditCached.clear();
        availableWatchCreditCached.clear();
    }

    void BindWallet(CWallet *pwalletIn)
    {
        pwallet = pwalletIn;
        MarkDirty();
    }

    //! filter decides which addresses will count towards the debit
    CAmount GetDebit(const isminefilter& filter, CAccount* forAccount=NULL) const;
    CAmount GetCredit(const isminefilter& filter, CAccount* forAccount=NULL) const;
    CAmount GetImmatureCredit(bool fUseCache=true, const CAccount* forAccount=NULL) const;
    CAmount GetAvailableCredit(bool fUseCache=true, const CAccount* forAccount=NULL) const;
    CAmount GetAvailableCreditIncludingLockedWitnesses(bool fUseCache=true, const CAccount* forAccount=NULL) const;
    CAmount GetImmatureWatchOnlyCredit(const bool& fUseCache=true, const CAccount* forAccount=NULL) const;
    CAmount GetAvailableWatchOnlyCredit(const bool& fUseCache=true, const CAccount* forAccount=NULL) const;
    CAmount GetChange() const;

    void GetAmounts(std::list<COutputEntry>& listReceived,
                    std::list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter, CKeyStore* from=NULL) const;

    bool IsFromMe(const isminefilter& filter) const
    {
        return (GetDebit(filter) > 0);
    }

    // True if only scriptSigs are different
    bool IsEquivalentTo(const CWalletTx& tx) const;

    bool InMempool() const;
    bool IsTrusted() const;

    int64_t GetTxTime() const;
    int GetRequestCount() const;

    bool RelayWalletTransaction(CConnman* connman);

    std::set<uint256> GetConflicts() const;
};

#endif // GULDEN_WALLET_WALLETTX_H
