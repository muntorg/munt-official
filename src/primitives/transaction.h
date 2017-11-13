// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_TRANSACTION_H
#define BITCOIN_PRIMITIVES_TRANSACTION_H

#include "amount.h"
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"

//Gulden
#include "pubkey.h"
#include "streams.h"
#include "utilstrencodings.h"
#include <new> // Required for placement 'new'.

static const int SERIALIZE_TRANSACTION_NO_WITNESS = 0x40000000;

static const int WITNESS_SCALE_FACTOR = 4;

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256 hash;
    uint32_t n;

    COutPoint(): n((uint32_t) -1) { }
    COutPoint(const uint256& hashIn, uint32_t nIn): hash(hashIn), n(nIn) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(hash);
        READWRITE(n);
    }

    void SetNull() { hash.SetNull(); n = (uint32_t) -1; }
    bool IsNull() const { return (hash.IsNull() && n == (uint32_t) -1); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        int cmp = a.hash.Compare(b.hash);
        return cmp < 0 || (cmp == 0 && a.n < b.n);
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;
    CScriptWitness scriptWitness; //! Only serialized through CTransaction

    /* Setting nSequence to this value for every input in a transaction
     * disables nLockTime. */
    static const uint32_t SEQUENCE_FINAL = 0xffffffff;

    /* Below flags apply in the context of BIP 68*/
    /* If this flag set, CTxIn::nSequence is NOT interpreted as a
     * relative lock-time. */
    static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1 << 31);

    /* If CTxIn::nSequence encodes a relative lock-time and this flag
     * is set, the relative lock-time has units of 512 seconds,
     * otherwise it specifies blocks with a granularity of 1. */
    static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22);

    /* If CTxIn::nSequence encodes a relative lock-time, this mask is
     * applied to extract that lock-time from the sequence field. */
    static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

    /* In order to use the same number of bits to encode roughly the
     * same wall-clock duration, and because blocks are naturally
     * limited to occur every 600s on average, the minimum granularity
     * for time-based relative lock-time is fixed at 512 seconds.
     * Converting from CTxIn::nSequence to seconds is performed by
     * multiplying by 512 = 2^9, or equivalently shifting up by
     * 9 bits. */
    static const int SEQUENCE_LOCKTIME_GRANULARITY = 9;

    CTxIn()
    {
        nSequence = SEQUENCE_FINAL;
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(prevout);
        READWRITE(*(CScriptBase*)(&scriptSig));
        READWRITE(nSequence);
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

enum CTxOutType : uint8_t
{
    //General purpose output types start from 0 counting upward
    ScriptLegacyOutput = 0,
    ScriptOutput = 1,
    
    //Specific/fixed purpose output types start from max counting backwards
    PoW2WitnessOutput = 31,
    StandardKeyHashOutput = 30
};


class CTxOutPoW2Witness
{
public:
    CKeyID spendingKeyID;
    CKeyID witnessKeyID;
    uint64_t lockFromBlock;
    uint64_t lockUntilBlock;
    uint64_t failCount;

    CTxOutPoW2Witness() {clear();}

    void clear()
    {
        spendingKeyID.SetNull();
        witnessKeyID.SetNull();
        lockFromBlock = 0;
        lockUntilBlock = 0;
        failCount = 0;
    }
    
    bool operator==(const CTxOutPoW2Witness& compare) const
    {
        return spendingKeyID == compare.spendingKeyID &&
               witnessKeyID == compare.witnessKeyID &&
               lockFromBlock == compare.lockFromBlock &&
               lockUntilBlock == compare.lockUntilBlock &&
               failCount == compare.failCount;
    }
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(spendingKeyID);
        READWRITE(witnessKeyID);
        READWRITE(lockFromBlock);
        READWRITE(lockUntilBlock);
        READWRITE(failCount);
    }
};

class CTxOutStandardKeyHash
{
public:
    CKeyID keyID;
    
    void clear()
    {
        keyID.SetNull();
    }
    
    bool operator==(const CTxOutStandardKeyHash& compare) const
    {
        return keyID == compare.keyID;
    }
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(keyID);
    }
};

/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    //fixme: gcc - future - In an ideal world we would just have nType be of type 'CTxOutType' - however GCC spits out unavoidable warnings when using an enum as part of a bitfield, so we use these getter/setter methods to work around it.
    CTxOutType GetType() const
    {
        return (CTxOutType)output.nType;
    }
    std::string GetTypeAsString() const
    {
        switch(CTxOutType(output.nType))
        {
            case CTxOutType::ScriptLegacyOutput:
            case CTxOutType::ScriptOutput:
                return "SCRIPT";
            case CTxOutType::PoW2WitnessOutput:
                return "POW2WITNESS";
            case CTxOutType::StandardKeyHashOutput:
                return "STANDARDKEYHASH";
        }
        return "";
    }
    void SetType(CTxOutType nType_)
    {
        DeleteOutput(output.nType);
        AllocOutput(nType_);
        output.nType = nType_;
    }

    CAmount nValue;

    //Size of CTxOut in memory is very important - so we use a union here to try minimise the space taken.
    struct output
    {
        int64_t nType;
        union
        {
            CScript scriptPubKey;
            CTxOutPoW2Witness witnessDetails;
            CTxOutStandardKeyHash standardKeyHash;
        };
        output()
        {
            new(&scriptPubKey) CScript();
        }
        ~output()
        {
        }

        template <typename Stream, typename Operation>
        inline void SerializeOp(Stream& s, Operation ser_action, CTxOutType nType)
        {
            switch(nType)
            {
                case CTxOutType::ScriptLegacyOutput:
                case CTxOutType::ScriptOutput:
                    READWRITE(*(CScriptBase*)(&scriptPubKey)); break;
                case CTxOutType::PoW2WitnessOutput:
                    READWRITE(witnessDetails); break;
                case CTxOutType::StandardKeyHashOutput:
                    READWRITE(standardKeyHash); break;
            }
        }
        
        std::string GetHex(CTxOutType nType) const
        {
            std::vector<unsigned char> serData;
            {
                CVectorWriter serialisedWitnessHeaderInfoStream(SER_NETWORK, INIT_PROTO_VERSION, serData, 0);
                const_cast<output*>(this)->SerializeOp(serialisedWitnessHeaderInfoStream, CSerActionSerialize(), nType);
            }
            return HexStr(serData);
        }
        
        bool operator==(const output& compare) const
        {
            if (nType != compare.nType)
                return false;

            switch(CTxOutType(nType))
            {
                case CTxOutType::ScriptLegacyOutput:
                case CTxOutType::ScriptOutput:
                    return scriptPubKey == compare.scriptPubKey;
                case CTxOutType::PoW2WitnessOutput:
                    return witnessDetails == compare.witnessDetails;
                case CTxOutType::StandardKeyHashOutput:
                    return standardKeyHash == compare.standardKeyHash;
            }
            return false;
        }
    } output;
    
    void DeleteOutput(int64_t nDeleteType)
    {
        switch(nDeleteType)
        {
            case CTxOutType::ScriptLegacyOutput:
            case CTxOutType::ScriptOutput:
            {
                output.scriptPubKey.clear();
                output.scriptPubKey.~CScript(); break;
            }
            case CTxOutType::PoW2WitnessOutput:
            {
                output.witnessDetails.clear();
                output.witnessDetails.~CTxOutPoW2Witness(); break;
            }
            case CTxOutType::StandardKeyHashOutput:
            {
                output.standardKeyHash.clear();
                output.standardKeyHash.~CTxOutStandardKeyHash(); break;
            }
        }
    }
    
    void AllocOutput(int64_t nAllocType)
    {
        switch(nAllocType)
        {
            case CTxOutType::ScriptLegacyOutput:
            case CTxOutType::ScriptOutput:
                new(&output.scriptPubKey) CScript(); break;
            case CTxOutType::PoW2WitnessOutput:
                new(&output.witnessDetails) CTxOutPoW2Witness(); break;
            case CTxOutType::StandardKeyHashOutput:
                new(&output.standardKeyHash) CTxOutStandardKeyHash(); break;
        }
    }
    
    bool IsUnspendable() const
    {
        if (GetType() <= CTxOutType::ScriptOutput)
            return output.scriptPubKey.IsUnspendable();
        
        //fixme: (GULDEN) (2.0) - Can our 'standard' outputs still be unspendable?
        return false;
    }
    
    virtual ~CTxOut()
    {
        //fixme: (GULDEN) (2.0) (IMPLEMENT)
        DeleteOutput(output.nType);
    }
    
    CTxOut operator=(const CTxOut& copyFrom)
    {
        SetType(CTxOutType(copyFrom.output.nType));
        nValue = copyFrom.nValue;
        switch(CTxOutType(output.nType))
        {
            case CTxOutType::ScriptLegacyOutput:
            case CTxOutType::ScriptOutput:
                output.scriptPubKey = copyFrom.output.scriptPubKey; break;
            case CTxOutType::PoW2WitnessOutput:
                output.witnessDetails = copyFrom.output.witnessDetails; break;
            case CTxOutType::StandardKeyHashOutput:
                output.standardKeyHash = copyFrom.output.standardKeyHash; break;
        }
        return *this;
    }
    
    CTxOut(const CTxOut& copyFrom)
    {
        SetType(CTxOutType(copyFrom.output.nType));
        nValue = copyFrom.nValue;
        switch(CTxOutType(output.nType))
        {
            case CTxOutType::ScriptLegacyOutput:
            case CTxOutType::ScriptOutput:
                output.scriptPubKey = copyFrom.output.scriptPubKey; break;
            case CTxOutType::PoW2WitnessOutput:
                output.witnessDetails = copyFrom.output.witnessDetails; break;
            case CTxOutType::StandardKeyHashOutput:
                output.standardKeyHash = copyFrom.output.standardKeyHash; break;
        }
    }

    CTxOut(CTxOut&& copyFrom)
    {
        SetType(CTxOutType(copyFrom.output.nType));
        nValue = copyFrom.nValue;
        switch(CTxOutType(output.nType))
        {
            case CTxOutType::ScriptLegacyOutput:
            case CTxOutType::ScriptOutput:
                output.scriptPubKey = copyFrom.output.scriptPubKey; break;
            case CTxOutType::PoW2WitnessOutput:
                output.witnessDetails = copyFrom.output.witnessDetails; break;
            case CTxOutType::StandardKeyHashOutput:
                output.standardKeyHash = copyFrom.output.standardKeyHash; break;
        }
    }
    
    CTxOut& operator=(CTxOut&& copyFrom)
    {
        SetType(CTxOutType(copyFrom.output.nType));
        nValue = copyFrom.nValue;
        switch(CTxOutType(output.nType))
        {
            case CTxOutType::ScriptLegacyOutput:
            case CTxOutType::ScriptOutput:
                output.scriptPubKey = copyFrom.output.scriptPubKey; break;
            case CTxOutType::PoW2WitnessOutput:
                output.witnessDetails = copyFrom.output.witnessDetails; break;
            case CTxOutType::StandardKeyHashOutput:
                output.standardKeyHash = copyFrom.output.standardKeyHash; break;
        }
    }

    CTxOut()
    {
        SetNull();
    }

    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        //fixme: (GULDEN) (2.0) (HIGH) backwards compat.
        READWRITE(VARINT(output.nType));
        READWRITE(nValue);
        output.SerializeOp(s, ser_action, GetType());
    }

    void SetNull()
    {
        SetType(ScriptLegacyOutput);
        nValue = -1;
        switch(output.nType)
        {
            case CTxOutType::ScriptLegacyOutput:
            case CTxOutType::ScriptOutput:
                output.scriptPubKey.clear(); break;
            case CTxOutType::PoW2WitnessOutput:
                output.witnessDetails.clear(); break;
            case CTxOutType::StandardKeyHashOutput:
                output.standardKeyHash.clear(); break;
        }
    }

    bool IsNull() const
    {
        return (nValue == -1);
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.output == b.output);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

struct CMutableTransaction;

/**
 * Basic transaction serialization format:
 * - int32_t nVersion
 * - std::vector<CTxIn> vin
 * - std::vector<CTxOut> vout
 * - uint32_t nLockTime
 *
 * Extended transaction serialization format:
 * - int32_t nVersion
 * - unsigned char dummy = 0x00
 * - unsigned char flags (!= 0)
 * - std::vector<CTxIn> vin
 * - std::vector<CTxOut> vout
 * - if (flags & 1):
 *   - CTxWitness wit;
 * - uint32_t nLockTime
 */
template<typename Stream, typename TxType>
inline void UnserializeTransaction(TxType& tx, Stream& s) {
    const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);

    s >> tx.nVersion;
    unsigned char flags = 0;
    tx.vin.clear();
    tx.vout.clear();
    /* Try to read the vin. In case the dummy is there, this will be read as an empty vector. */
    s >> COMPACTSIZEVECTOR(tx.vin);
    if (tx.vin.size() == 0 && fAllowWitness) {
        /* We read a dummy or an empty vin. */
        s >> flags;
        if (flags != 0) {
            s >> COMPACTSIZEVECTOR(tx.vin);
            s >> COMPACTSIZEVECTOR(tx.vout);
        }
    } else {
        /* We read a non-empty vin. Assume a normal vout follows. */
        s >> COMPACTSIZEVECTOR(tx.vout);
    }
    if ((flags & 1) && fAllowWitness) {
        /* The witness flag is present, and we support witnesses. */
        flags ^= 1;
        for (size_t i = 0; i < tx.vin.size(); i++) {
            s >> COMPACTSIZEVECTOR(tx.vin[i].scriptWitness.stack);
        }
    }
    if (flags) {
        /* Unknown flag in the serialization */
        throw std::ios_base::failure("Unknown transaction optional data");
    }
    s >> tx.nLockTime;
}

template<typename Stream, typename TxType>
inline void SerializeTransaction(const TxType& tx, Stream& s) {
    const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);

    s << tx.nVersion;
    unsigned char flags = 0;
    // Consistency check
    if (fAllowWitness) {
        /* Check whether witnesses need to be serialized. */
        if (tx.HasWitness()) {
            flags |= 1;
        }
    }
    if (flags) {
        /* Use extended format in case witnesses are to be serialized. */
        std::vector<CTxIn> vinDummy;
        s << COMPACTSIZEVECTOR(vinDummy);
        s << flags;
    }
    s << COMPACTSIZEVECTOR(tx.vin);
    s << COMPACTSIZEVECTOR(tx.vout);
    if (flags & 1) {
        for (size_t i = 0; i < tx.vin.size(); i++) {
            s << COMPACTSIZEVECTOR(tx.vin[i].scriptWitness.stack);
        }
    }
    s << tx.nLockTime;
}


/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
public:
    // Default transaction version.
    static const int32_t CURRENT_VERSION=2;

    // Changing the default transaction version requires a two step process: first
    // adapting relay policy by bumping MAX_STANDARD_VERSION, and then later date
    // bumping the default CURRENT_VERSION at which point both CURRENT_VERSION and
    // MAX_STANDARD_VERSION will be equal.
    static const int32_t MAX_STANDARD_VERSION=2;

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const int32_t nVersion;
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const uint32_t nLockTime;

private:
    /** Memory only. */
    const uint256 hash;

    uint256 ComputeHash() const;

public:
    /** Construct a CTransaction that qualifies as IsNull() */
    CTransaction();

    /** Convert a CMutableTransaction into a CTransaction. */
    CTransaction(const CMutableTransaction &tx);
    CTransaction(CMutableTransaction &&tx);

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeTransaction(*this, s);
    }

    /** This deserializing constructor is provided instead of an Unserialize method.
     *  Unserialize is not possible, since it would require overwriting const fields. */
    template <typename Stream>
    CTransaction(deserialize_type, Stream& s) : CTransaction(CMutableTransaction(deserialize, s)) {}

    bool IsNull() const {
        return vin.empty() && vout.empty();
    }

    const uint256& GetHash() const {
        return hash;
    }

    // Compute a hash that includes both transaction and witness data
    uint256 GetWitnessHash() const;

    // Return sum of txouts.
    CAmount GetValueOut() const;
    // GetValueIn() is a method on CCoinsViewCache, because
    // inputs must be known to compute value in.

    /**
     * Get the total transaction size in bytes, including witness data.
     * "Total Size" defined in BIP141 and BIP144.
     * @return Total transaction size in bytes
     */
    unsigned int GetTotalSize() const;

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull()) || IsPoW2WitnessCoinBase();
    }

    //fixme: (GULDEN) (2.0) - check second vin is a witness transaction.
    bool IsPoW2WitnessCoinBase() const
    {
        return (vin.size() == 2 && vin[0].prevout.IsNull());
    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return a.hash == b.hash;
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return a.hash != b.hash;
    }

    std::string ToString() const;

    bool HasWitness() const
    {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }
};

/** A mutable version of CTransaction. */
struct CMutableTransaction
{
    int32_t nVersion;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    uint32_t nLockTime;

    CMutableTransaction();
    CMutableTransaction(const CTransaction& tx);

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeTransaction(*this, s);
    }


    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializeTransaction(*this, s);
    }

    template <typename Stream>
    CMutableTransaction(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    /** Compute the hash of this CMutableTransaction. This is computed on the
     * fly, as opposed to GetHash() in CTransaction, which uses a cached result.
     */
    uint256 GetHash() const;

    friend bool operator==(const CMutableTransaction& a, const CMutableTransaction& b)
    {
        return a.GetHash() == b.GetHash();
    }

    bool HasWitness() const
    {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }
};

typedef std::shared_ptr<const CTransaction> CTransactionRef;
static inline CTransactionRef MakeTransactionRef() { return std::make_shared<const CTransaction>(); }
template <typename Tx> static inline CTransactionRef MakeTransactionRef(Tx&& txIn) { return std::make_shared<const CTransaction>(std::forward<Tx>(txIn)); }

/** Compute the weight of a transaction, as defined by BIP 141 */
int64_t GetTransactionWeight(const CTransaction &tx);

#endif // BITCOIN_PRIMITIVES_TRANSACTION_H
