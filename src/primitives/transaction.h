// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_PRIMITIVES_TRANSACTION_H
#define GULDEN_PRIMITIVES_TRANSACTION_H

#include "amount.h"
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"

//Gulden
#include "pubkey.h"
#include "streams.h"
#include "utilstrencodings.h"
#include <new> // Required for placement 'new'.
#include <bitset>


static const int SERIALIZE_TRANSACTION_NO_SEGREGATED_SIGNATURES = 0x40000000;

inline bool IsOldTransactionVersion(const unsigned int nVersion)
{
    return (nVersion < 5) || (nVersion == 536870912);
}

struct CBlockPosition
{
    uint64_t blockNumber; // Position of block on blockchain that contains our transaction.
    uint64_t transactionIndex; // Position of transaction within the block.
    CBlockPosition(uint64_t blockNumber_, uint64_t transactionIndex_) : blockNumber(blockNumber_), transactionIndex(transactionIndex_) {}

    //fixme: (2.1) (MOBILE) (SPV) (SEGSIG) Look closer at how to handle this in relation to mobile SPV wallets
    uint256 getHash() const
    {
        std::vector<unsigned char> serData;
        {
            CVectorWriter serializedBlockPosition(SER_NETWORK, INIT_PROTO_VERSION, serData, 0);
            serializedBlockPosition << blockNumber;
            serializedBlockPosition << transactionIndex;
        }
        std::string sHex = HexStr(serData);
        return Hash(sHex.begin(), sHex.end());
    }

    friend bool operator<(const CBlockPosition& a, const CBlockPosition& b)
    {
        if (a.blockNumber == b.blockNumber)
        {
            return a.blockNumber < b.blockNumber;
        }
        else
        {
            return a.transactionIndex < b.transactionIndex;
        }
    }

    friend bool operator==(const CBlockPosition& a, const CBlockPosition& b)
    {
        return (a.blockNumber == b.blockNumber && a.transactionIndex == b.transactionIndex);
    }
};


//fixme: (2.1) (SEGSIG) Ensure again that network rules for the other 7 types are consistently handled.
// Represented in class as 3 bits.
// Maximum of 8 values
enum CTxInType : uint8_t
{
    CURRENT_TX_IN_TYPE = 0,
    FUTURE_TX_IN_TYPE2 = 1,
    FUTURE_TX_IN_TYPE3 = 2,
    FUTURE_TX_IN_TYPE4 = 3,
    FUTURE_TX_IN_TYPE5 = 4,
    FUTURE_TX_IN_TYPE6 = 5,
    FUTURE_TX_IN_TYPE7 = 6,
    FUTURE_TX_IN_TYPE8 = 7
};

//fixme: (2.1) (SEGSIG) we forbid index based outpoint for now.
//fixme: (2.1) (SEGSIG) Double check all RBF/AbsoluteLock/RelativeLock behaviour
// Only 5 bits available for TxInFlags.
// The are used as bit flags so only 5 values possible each with an on/off state.
// All 5 values are currently in use.
enum CTxInFlags : uint8_t
{
    // The actual flag values
    IndexBasedOutpoint = 1,                                                         // Outpoint is an index instead of a hash.
    OptInRBF = 2,                                                                   // CTxIn allows RBF.
    HasAbsoluteLock = 4,                                                            // CTxIn uses "absolute" locktime.
    HasTimeBasedRelativeLock = 8,                                                   // CTxIn uses time based "relative" locktime.
    HasBlockBasedRelativeLock = 16,                                                 // CTxIn uses block based "relative" locktime.
    // Below are mask/setting helpers
    None = 0,
    HasRelativeLock = HasTimeBasedRelativeLock | HasBlockBasedRelativeLock,
    HasLock = HasRelativeLock | HasAbsoluteLock
};

#define UINT31_MAX 2147483647

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
private:
     // fixme: (2.1) (MED) - We can reduce memory consumption here by using something like prevector for hash cases.
    // Outpoint either uses hash or 'block position' never both.
    union
    {
        uint256 hash;
        CBlockPosition prevBlock;
    };
public:
    uint32_t isHash: 1; // Set to 0 when using prevBlock, 1 when using hash.
    uint32_t n : 31;

    COutPoint()
    : hash(uint256())
    , isHash(1)
    , n(UINT31_MAX)
    {
    }
    COutPoint(const uint256& hashIn, uint32_t nIn)
    :
    hash(hashIn)
    , isHash(1)
    , n(nIn)
    {
    }
    COutPoint(const uint64_t blockNumber, const uint64_t transactionIndex, uint32_t nIn)
    : prevBlock(blockNumber, transactionIndex)
    , isHash(0)
    , n(nIn)
    {
    }

    uint256 getHash() const
    {
        if (isHash)
        {
            return hash;
        }
        else
        {
            return prevBlock.getHash();
        }
    }
    void setHash(uint256 hash_)
    {
        hash = hash_;
        isHash = 1;
    }

    template <typename Stream> inline void ReadFromStream(Stream& s, CTxInType nType, uint8_t nFlags, int nTransactionVersion)
    {
        CSerActionUnserialize ser_action;

        if (IsOldTransactionVersion(nTransactionVersion))
        {
            isHash = 1;
            STRREAD(hash);
            uint32_t n_;
            STRREAD(n_);
            n = n_;
        }
        else
        {
            std::bitset<3> flags(nFlags);
            if (!flags[IndexBasedOutpoint])
            {
                isHash = 1;
                STRREAD(hash);
            }
            else
            {
                isHash = 0;
                STRREAD(VARINT(prevBlock.blockNumber));
                STRREAD(VARINT(prevBlock.transactionIndex));
            }
            uint32_t n_;
            STRREAD(VARINT(n_));
            n = n_;
        }
    }

    template <typename Stream> inline void WriteToStream(Stream& s, CTxInType nType, uint8_t nFlags, int nTransactionVersion) const
    {
        CSerActionSerialize ser_action;

        if (IsOldTransactionVersion(nTransactionVersion))
        {
            STRWRITE(hash);
            uint32_t nTemp = (n == UINT31_MAX ? std::numeric_limits<uint32_t>::max() : (uint32_t)n);
            STRWRITE(nTemp);
        }
        else
        {
            std::bitset<3> flags(nFlags);
            if (!flags[IndexBasedOutpoint])
            {
                STRWRITE(hash);
            }
            else
            {
                STRWRITE(VARINT(prevBlock.blockNumber));
                STRWRITE(VARINT(prevBlock.transactionIndex));
            }
            uint32_t n_ = n;
            STRWRITE(VARINT(n_));
        }
    }

    void SetNull() { hash.SetNull(); n = UINT31_MAX; }
    bool IsNull() const { return (hash.IsNull() && n == UINT31_MAX); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        if (a.isHash == b.isHash)
        {
            if (a.isHash)
            {
                if (a.hash == b.hash)
                {
                    return a.n < b.n;
                }
                else
                {
                    return a.hash < b.hash;
                }
            }
            else
            {
                return a.prevBlock < b.prevBlock;
            }
        }
        else
        {
            return a.isHash < b.isHash;
        }
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.isHash == b.isHash && ((a.isHash && a.hash == b.hash) || (!a.isHash && a.prevBlock == b.prevBlock)) && a.n == b.n);
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
    static const uint8_t CURRENT_TYPE=0;
    // First 3 bits are type, last 5 bits are flags.
    mutable uint8_t nTypeAndFlags;
    //fixme: (Post-2.1) gcc - future - In an ideal world we would just have nType be of type 'CTxOutType' - however GCC spits out unavoidable warnings when using an enum as part of a bitfield, so we use these getter/setter methods to work around it.
    CTxInType GetType() const
    {
        return (CTxInType) ( (nTypeAndFlags & 0b11100000) >> 5 );
    }
    CTxInFlags GetFlags() const
    {
        return (CTxInFlags) ( nTypeAndFlags & 0b00011111 );
    }
    bool FlagIsSet(CTxInFlags flag) const
    {
        return (GetFlags() & flag) != 0;
    }
    void SetFlag(CTxInFlags flag) const
    {
        nTypeAndFlags |= flag;
        if (!FlagIsSet(CTxInFlags::HasRelativeLock))
        {
            nSequence = 0;
        }
    }
    void UnsetFlag(CTxInFlags flag) const
    {
        nTypeAndFlags &= ~flag;
        if (!FlagIsSet(CTxInFlags::HasRelativeLock))
        {
            nSequence = 0;
        }
    }
    uint32_t GetSequence(int nTransactionVersion) const
    {
        if (IsOldTransactionVersion(nTransactionVersion) || FlagIsSet(CTxInFlags::HasRelativeLock))
            return nSequence;
        return 0;
    }
    void SetSequence(uint32_t nSequence_, int nTransactionVersion, CTxInFlags sequenceType)
    {
        if (IsOldTransactionVersion(nTransactionVersion))
        {
            nSequence = nSequence_;
        }
        else
        {
            SetFlag(sequenceType);
            if (FlagIsSet(CTxInFlags::HasRelativeLock))
            {
                nSequence = nSequence_;
            }
        }
    }
    COutPoint prevout;
    CScript scriptSig;
private:
    mutable uint32_t nSequence;
public:
    CSegregatedSignatureData segregatedSignatureData; //! Only serialized through CTransaction

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
     * limited to occur every 150s on average, the minimum granularity
     * for time-based relative lock-time is fixed at 128 seconds.
     * Converting from CTxIn::nSequence to seconds is performed by
     * multiplying by 128 = 2^7, or equivalently shifting up by
     * 7 bits. */
    static const int SEQUENCE_LOCKTIME_GRANULARITY = 7;

    CTxIn()
    {
        nSequence = SEQUENCE_FINAL;
        nTypeAndFlags = CURRENT_TYPE;
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn, uint8_t nFlagsIn);
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn, uint32_t nSequenceIn, uint8_t nFlagsIn);

    template <typename Stream> inline void ReadFromStream(Stream& s, int nTransactionVersion)
    {
        CSerActionUnserialize ser_action;

        //2.0 onwards we have versioning for CTxIn
        if (!IsOldTransactionVersion(nTransactionVersion))
        {
            uint8_t nTypeAndFlags_;
            STRREAD(nTypeAndFlags_);

            prevout.ReadFromStream(s, GetType(), GetFlags(), nTransactionVersion);
            //scriptSig is no longer used - everything goes in segregatedSignatureData.
            if (FlagIsSet(CTxInFlags::HasRelativeLock))
            {
                s >> VARINT(nSequence);
            }
        }
        else
        {
            prevout.ReadFromStream(s, GetType(), GetFlags(), nTransactionVersion);
            STRREAD(*(CScriptBase*)(&scriptSig));
            STRREAD(nSequence);
        }
    }
    template <typename Stream> inline void WriteToStream(Stream& s, int nTransactionVersion) const
    {
        CSerActionSerialize ser_action;

        if (!IsOldTransactionVersion(nTransactionVersion))
        {
            //if (nSequence != SEQUENCE_FINAL)
                //nTypeAndFlags |= HasSequenceNumberMask;
            STRWRITE(nTypeAndFlags);

            prevout.WriteToStream(s, GetType(), GetFlags(), nTransactionVersion);

            if (FlagIsSet(CTxInFlags::HasRelativeLock))
            {
                s << VARINT(nSequence);
            }
        }
        else
        {
            prevout.WriteToStream(s, GetType(), GetFlags(), nTransactionVersion);
            STRWRITE(*(CScriptBase*)(&scriptSig));
            STRWRITE(nSequence);
        }
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
    uint64_t actionNonce;

    CTxOutPoW2Witness() {clear();}

    void clear()
    {
        spendingKeyID.SetNull();
        witnessKeyID.SetNull();
        lockFromBlock = 0;
        lockUntilBlock = 0;
        failCount = 0;
        actionNonce = 0;
    }

    bool operator==(const CTxOutPoW2Witness& compare) const
    {
        return spendingKeyID == compare.spendingKeyID &&
               witnessKeyID == compare.witnessKeyID &&
               lockFromBlock == compare.lockFromBlock &&
               lockUntilBlock == compare.lockUntilBlock &&
               failCount == compare.failCount &&
               actionNonce == compare.actionNonce;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(spendingKeyID);
        READWRITE(witnessKeyID);
        READWRITE(lockFromBlock);
        READWRITE(lockUntilBlock);
        READWRITE(VARINT(failCount));
        READWRITE(VARINT(actionNonce));
    }
};

class CTxOutStandardKeyHash
{
public:
    CKeyID keyID;

    CTxOutStandardKeyHash(const CKeyID  keyID_) : keyID(keyID_) {}
    CTxOutStandardKeyHash() { clear(); }

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
    //fixme: (Post-2.1) gcc - future - In an ideal world we would just have nType be of type 'CTxOutType' - however GCC spits out unavoidable warnings when using an enum as part of a bitfield, so we use these getter/setter methods to work around it.
    CTxOutType GetType() const
    {
        return (CTxOutType)output.nType;
    }
    std::string GetTypeAsString() const
    {
        switch(CTxOutType(output.nType))
        {
            case CTxOutType::ScriptLegacyOutput:
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
        output.DeleteOutput();
        output.nType = nType_;
        output.AllocOutput();
    }

    CAmount nValue;

    //Size of CTxOut in memory is very important - so we use a union here to try minimise the space taken.
    struct output
    {
        uint8_t nType: 5;
        mutable uint8_t nValueBase: 3;
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

        void DeleteOutput()
        {
            switch(nType)
            {
                case CTxOutType::ScriptLegacyOutput:
                {
                    scriptPubKey.clear();
                    scriptPubKey.~CScript(); break;
                }
                case CTxOutType::PoW2WitnessOutput:
                {
                    witnessDetails.clear();
                    witnessDetails.~CTxOutPoW2Witness(); break;
                }
                case CTxOutType::StandardKeyHashOutput:
                {
                    standardKeyHash.clear();
                    standardKeyHash.~CTxOutStandardKeyHash(); break;
                }
            }
        }

        void AllocOutput()
        {
            switch(nType)
            {
                case CTxOutType::ScriptLegacyOutput:
                    new(&scriptPubKey) CScript(); break;
                case CTxOutType::PoW2WitnessOutput:
                    new(&witnessDetails) CTxOutPoW2Witness(); break;
                case CTxOutType::StandardKeyHashOutput:
                    new(&standardKeyHash) CTxOutStandardKeyHash(); break;
            }
        }

        template <typename Stream> inline void WriteToStream(Stream& s) const
        {
            static CSerActionSerialize ser_action;
            switch(nType)
            {
                case CTxOutType::ScriptLegacyOutput:
                    STRWRITE(*(CScriptBase*)(&scriptPubKey)); break;
                case CTxOutType::PoW2WitnessOutput:
                    STRWRITE(witnessDetails); break;
                case CTxOutType::StandardKeyHashOutput:
                    STRWRITE(standardKeyHash); break;
            }
        }

        template <typename Stream> inline void ReadFromStream(Stream& s)
        {
            static CSerActionUnserialize ser_action;
            switch(nType)
            {
                case CTxOutType::ScriptLegacyOutput:
                    STRREAD(*(CScriptBase*)(&scriptPubKey)); break;
                case CTxOutType::PoW2WitnessOutput:
                    STRREAD(witnessDetails); break;
                case CTxOutType::StandardKeyHashOutput:
                    STRREAD(standardKeyHash); break;
            }
        }

        std::string GetHex() const
        {
            std::vector<unsigned char> serData;
            {
                CVectorWriter serialisedWitnessHeaderInfoStream(SER_NETWORK, INIT_PROTO_VERSION, serData, 0);
                const_cast<output*>(this)->WriteToStream(serialisedWitnessHeaderInfoStream);
            }
            return HexStr(serData);
        }

        uint256 GetHash() const
        {
            std::string sHex = GetHex();
            return Hash(sHex.begin(), sHex.end());
        }

        bool operator==(const output& compare) const
        {
            if (nType != compare.nType)
                return false;

            switch(CTxOutType(nType))
            {
                case CTxOutType::ScriptLegacyOutput:
                    return scriptPubKey == compare.scriptPubKey;
                case CTxOutType::PoW2WitnessOutput:
                    return witnessDetails == compare.witnessDetails;
                case CTxOutType::StandardKeyHashOutput:
                    return standardKeyHash == compare.standardKeyHash;
            }
            return false;
        }
    } output;


    bool IsUnspendable() const
    {
        if (GetType() <= CTxOutType::ScriptLegacyOutput)
            return output.scriptPubKey.IsUnspendable();
        return false;
    }

    virtual ~CTxOut()
    {
        output.DeleteOutput();
    }

    CTxOut operator=(const CTxOut& copyFrom)
    {
        SetType(CTxOutType(copyFrom.output.nType));
        output.nValueBase = copyFrom.output.nValueBase;
        nValue = copyFrom.nValue;
        switch(CTxOutType(output.nType))
        {
            case CTxOutType::ScriptLegacyOutput:
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
        output.nValueBase = copyFrom.output.nValueBase;
        switch(CTxOutType(output.nType))
        {
            case CTxOutType::ScriptLegacyOutput:
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
        output.nValueBase = copyFrom.output.nValueBase;
        switch(CTxOutType(output.nType))
        {
            case CTxOutType::ScriptLegacyOutput:
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
        output.nValueBase = copyFrom.output.nValueBase;
        switch(CTxOutType(output.nType))
        {
            case CTxOutType::ScriptLegacyOutput:
                output.scriptPubKey = copyFrom.output.scriptPubKey; break;
            case CTxOutType::PoW2WitnessOutput:
                output.witnessDetails = copyFrom.output.witnessDetails; break;
            case CTxOutType::StandardKeyHashOutput:
                output.standardKeyHash = copyFrom.output.standardKeyHash; break;
        }
        return *this;
    }

    CTxOut()
    {
        SetNull();
    }

    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn);
    CTxOut(const CAmount& nValueIn, CTxOutPoW2Witness witnessDetails);
    CTxOut(const CAmount& nValueIn, CTxOutStandardKeyHash standardKeyHash);

    template <typename Stream> void WriteToStream(Stream& s, int32_t nTransactionVersion) const
    {
        static CSerActionSerialize ser_action;
        if (IsOldTransactionVersion(nTransactionVersion))
        {
            assert(output.nType == CTxOutType::ScriptLegacyOutput);

            // Old transaction format.
            STRWRITE(nValue);
        }
        else
        {
            CAmount nValueWrite = nValue;

            output.nValueBase = 0; // 8 decimal precision.
            //fixme: (2.1) Is there some 'trick' to calculate this faster without so much branching?
            if (nValue % 1000000000000 == 0)    { output.nValueBase = 7; } // 4 significant digit precision
            else if (nValue % 10000000000 == 0) { output.nValueBase = 6; } // 2 significant digit precision
            else if (nValue % 100000000 == 0)   { output.nValueBase = 5; } // 1 significant digit precision
            else if (nValue % 10000000 == 0)    { output.nValueBase = 4; } // 1 decimal precision
            else if (nValue % 1000000 == 0)     { output.nValueBase = 3; } // 2 decimal precision
            else if (nValue % 10000 == 0)       { output.nValueBase = 2; } // 4 decimal precision
            else if (nValue % 100 == 0)         { output.nValueBase = 1; } // 6 decimal precision

            // Adjust nValueWrite to the new base.
            switch (output.nValueBase)
            {
                case 7: nValueWrite /= 1000000000000; break;
                case 6: nValueWrite /= 10000000000; break;
                case 5: nValueWrite /= 100000000; break;
                case 4: nValueWrite /= 10000000; break;
                case 3: nValueWrite /= 1000000; break;
                case 2: nValueWrite /= 10000; break;
                case 1: nValueWrite /= 100; break;
            }

            uint8_t nTypeAndValueBase = 0;
            nTypeAndValueBase = output.nType;
            nTypeAndValueBase <<= 3;
            nTypeAndValueBase |= output.nValueBase;
            STRWRITE(nTypeAndValueBase);
            STRWRITE(VARINT(nValueWrite));
        }
        output.WriteToStream(s);
    }

    template <typename Stream> void ReadFromStream(Stream& s, int32_t nTransactionVersion)
    {
        static CSerActionUnserialize ser_action;
        if (IsOldTransactionVersion(nTransactionVersion))
        {
            assert(output.nType == CTxOutType::ScriptLegacyOutput);
            output.nValueBase = 0;
            STRREAD(nValue);
        }
        else // Read in the new transaction format value, which is specified in a format that is much more compact in most circumstances.
        {
            uint8_t nTypeAndValueBase;
            STRREAD(nTypeAndValueBase);
            output.nValueBase = (nTypeAndValueBase & 0b00000111);
            SetType(CTxOutType((nTypeAndValueBase & 0b11111000) >> 3));

            STRREAD(VARINT(nValue)); // Compacted value is stored as a varint.
            switch(output.nValueBase) // Which further needs to be multiplied by base to get the full int64 value.
            {
                case 0: break;                          // 8 decimal precision  (0.00000008, 87654321.12345678 etc.)
                case 1: nValue *= 100; break;           // 6 decimal precision  (0.00000600, 87654321.12345600 etc.)
                case 2: nValue *= 10000; break;         // 4 decimal precision  (0.00040000, 87654321.12340000 etc.)
                case 3: nValue *= 1000000; break;       // 2 decimal precision  (0.02000000, 87654321.12000000 etc.)
                case 4: nValue *= 10000000; break;      // 1 decimal precision  (0.10000000, 87654321.10000000 etc.)
                case 5: nValue *= 100000000; break;     // 1 significant digit precision (1.00000000, 87654321.00000000 etc.)
                case 6: nValue *= 10000000000; break;   // 3 significant digit precision (200.00000000, 876543200.00000000 etc.)
                case 7: nValue *= 1000000000000; break; // 5 significant digit precision (40000.00000000, 876540000.00000000 etc.)
            };
        }
        output.ReadFromStream(s);
    }

    void SetNull()
    {
        SetType(ScriptLegacyOutput);
        output.nValueBase = 0;
        nValue = -1;
        switch(output.nType)
        {
            case CTxOutType::ScriptLegacyOutput:
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
        return (a.nValue == b.nValue &&
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
inline void UnserializeTransactionOld(TxType& tx, Stream& s)
{
    s >> tx.nVersion;
    tx.vin.clear();
    tx.vout.clear();

    /* Try to read the vin. */
    {
        uint64_t nSize;
        s >> COMPACTSIZE(nSize);
        tx.vin.resize(nSize);
        for (auto& in : tx.vin)
        {
            in.ReadFromStream(s, tx.nVersion);
        }
    }

    /* We read a non-empty vin. Assume a normal vout follows. */
    {
        uint64_t nSize;
        s >> COMPACTSIZE(nSize);
        tx.vout.resize(nSize);
        for (auto& out : tx.vout)
        {
            out.ReadFromStream(s, tx.nVersion);
        }
    }
    s >> tx.nLockTime;
}

template<typename Stream, typename TxType>
inline void SerializeTransactionOld(const TxType& tx, Stream& s)
{
    s << tx.nVersion;
    s << COMPACTSIZE(tx.vin.size());
    for (const auto& in : tx.vin)
    {
        in.WriteToStream(s, tx.nVersion);
    }
    s << COMPACTSIZE(tx.vout.size());
    for (const auto& out : tx.vout)
    {
        out.WriteToStream(s, tx.nVersion);
    }
    s << tx.nLockTime;
}

//New transaction format:
//Version number: CVarInt [1 byte] (but forward compat for larger sizes)
//Flags: bitset [1 byte] (7 bytes used, 8th byte signals ExtraFlags)  {Current flags are for 1/2/3 input transactions, 1/2/3 output transactions and locktime}
//ExtraFlags: bitset [1 byte] (but never currently present, only there for forwards compat)
//Input count: CVarInt [0-9 byte] (only present in the event that input count flags are all unset)
//Vector of inputs
//Output count: CVarInt [0-9 byte] (only present in the event that output count flags are all unset)
//Vector of outputs
//Vector of witnesses (no size)
//Lock time: CVarInt [0-9 byte] (only present if locktime flag is set)

//>90% of transactions involve either 1/2/3 inputs or 1/2/3 outputs.
enum TransactionFlags : uint8_t
{
    HasOneInput,
    HasTwoInputs,
    HasThreeInputs,
    HasOneOutput,
    HasTwoOutputs,
    HasThreeOutputs,
    HasLockTime,
    HasExtraFlags
};

template<typename Stream, typename TxType> inline void SerializeTransaction(const TxType& tx, Stream& s) {
    if (IsOldTransactionVersion(tx.nVersion))
        return SerializeTransactionOld(tx, s);

    // Setup flags
    switch(tx.vin.size())
    {
        case 1:
            tx.flags.set(HasOneInput, true).set(HasTwoInputs, false).set(HasThreeInputs, false); break;
        case 2:
            tx.flags.set(HasOneInput, false).set(HasTwoInputs, true).set(HasThreeInputs, false); break;
        case 3:
            tx.flags.set(HasOneInput, false).set(HasTwoInputs, false).set(HasThreeInputs, true); break;
        default:
            tx.flags.set(HasOneInput, false).set(HasTwoInputs, false).set(HasThreeInputs, false);
    }
    switch(tx.vout.size())
    {
        case 1:
            tx.flags.set(HasOneOutput, true).set(HasTwoOutputs, false).set(HasThreeOutputs, false); break;
        case 2:
            tx.flags.set(HasOneOutput, false).set(HasTwoOutputs, true).set(HasThreeOutputs, false); break;
        case 3:
            tx.flags.set(HasOneOutput, false).set(HasTwoOutputs, false).set(HasThreeOutputs, true); break;
        default:
            tx.flags.set(HasOneOutput, false).set(HasTwoOutputs, false).set(HasThreeOutputs, false);
    }
    tx.flags.set(HasLockTime, false);
    if (tx.nLockTime > 0)
    {
        tx.flags.set(HasLockTime, true);
    }

    //Serialization begins.
    //Version
    s << VARINT(tx.nVersion);

    //Flags + (opt) ExtraFlags
    s << static_cast<uint8_t>(tx.flags.to_ulong());
    if(tx.flags[HasExtraFlags])
        s << static_cast<uint8_t>(tx.extraFlags.to_ulong());

    //(opt) Input count + Inputs
    if ( !(tx.flags[HasOneInput] || tx.flags[HasTwoInputs] || tx.flags[HasThreeInputs]) )
        s << VARINT(tx.vin.size());
    for (const auto& in : tx.vin)
    {
        in.WriteToStream(s, tx.nVersion);
    }

    //(opt) Output count + Outputs
    if (!tx.flags[HasOneOutput] && !tx.flags[HasTwoOutputs] && !tx.flags[HasThreeOutputs])
    {
        s << VARINT(tx.vout.size());
    }
    for (const auto& out : tx.vout)
    {
        out.WriteToStream(s, tx.nVersion);
    }

    //Witness data
    if (!(s.GetVersion() & SERIALIZE_TRANSACTION_NO_SEGREGATED_SIGNATURES)) {
        for (size_t i = 0; i < tx.vin.size(); i++)
        {
            s << VARINTVECTOR(tx.vin[i].segregatedSignatureData.stack);
        }
    }

    // (opt) lock time
    if (tx.flags[HasLockTime])
    {
        s << VARINT(tx.nLockTime);
    }
}


template<typename Stream, typename TxType>
inline void UnserializeTransaction(TxType& tx, Stream& s) {
    CSerActionUnserialize ser_action;

    //Version
    STRPEEK(tx.nVersion);
    if (IsOldTransactionVersion(tx.nVersion))
    {
        UnserializeTransactionOld(tx, s);
        return;
    }
    tx.nVersion = ReadVarInt<Stream, int32_t>(s);

    //Flags + (opt) ExtraFlags
    tx.flags.reset();
    tx.extraFlags.reset();
    unsigned char cFlags, cExtraFlags;
    s >> cFlags;
    tx.flags = std::bitset<8>(cFlags);
    if (tx.flags[HasExtraFlags])
    {
        s >> cExtraFlags;
        tx.extraFlags = std::bitset<8>(cExtraFlags);
    }

    //(opt) Input count + Inputs
    tx.vin.clear();
    if (tx.flags[HasOneInput])
    {
        tx.vin.resize(1);
    }
    else if (tx.flags[HasTwoInputs])
    {
        tx.vin.resize(2);
    }
    else if (tx.flags[HasThreeInputs])
    {
        tx.vin.resize(3);
    }
    else
    {
        int nSize;
        s >> VARINT(nSize);
        tx.vin.resize(nSize);
    }
    for (auto & txIn : tx.vin)
    {
        txIn.ReadFromStream(s, tx.nVersion);
    }

    //(opt) Output count + Outputs
    tx.vout.clear();
    if (tx.flags[HasOneOutput])
    {
        tx.vout.resize(1);
    }
    else if (tx.flags[HasTwoOutputs])
    {
        tx.vout.resize(2);
    }
    else if (tx.flags[HasThreeOutputs])
    {
        tx.vout.resize(3);
    }
    else
    {
        uint64_t nSize;
        s >> VARINT(nSize);
        tx.vout.resize(nSize);
    }
    for (auto & txOut : tx.vout)
    {
        txOut.ReadFromStream(s, tx.nVersion);
    }

    if (!(s.GetVersion() & SERIALIZE_TRANSACTION_NO_SEGREGATED_SIGNATURES)) {
        for (size_t i = 0; i < tx.vin.size(); i++) {
            s >> VARINTVECTOR(tx.vin[i].segregatedSignatureData.stack);
        }
    }

    // (opt) lock time
    if (tx.flags[HasLockTime])
    {
        tx.nLockTime = ReadVarInt<Stream, uint32_t>(s);
    }
    else
    {
        tx.nLockTime = 0;
    }
}


//fixme: (2.1) Remove
#define CURRENT_TX_VERSION_POW2 (IsSegSigEnabled(chainActive.TipPrev()) ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::CURRENT_VERSION)

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
    static const int32_t MAX_STANDARD_VERSION=5;
    static const int32_t SEGSIG_ACTIVATION_VERSION=5;

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const int32_t nVersion;
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const uint32_t nLockTime;
    mutable std::bitset<8> flags;
    mutable std::bitset<8> extraFlags;//Currently unused but present for forwards compat.

private:
    /** Memory only. */
    const uint256 hash;

    uint256 ComputeHash() const;

public:
    /** Construct a CTransaction that qualifies as IsNull() */
    CTransaction(int32_t nVersion_);

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

    //fixme: (2.0.1) - check second vin is a witness transaction.
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

    //fixme: (2.1) - We can possibly improve this test by testing transaction version instead.
    bool HasSegregatedSignatures() const
    {
        for (size_t i = 0; i < vin.size(); i++)
        {
            if (!vin[i].segregatedSignatureData.IsNull()) 
                return true;
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
    mutable std::bitset<8> flags;
    mutable std::bitset<8> extraFlags;//Currently unused but present for forwards compat.

    CMutableTransaction(int32_t nVersion_);
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

    //fixme: (2.1) - We can possibly improve this test by testing transaction version instead.
    bool HasSegregatedSignatures() const
    {
        for (size_t i = 0; i < vin.size(); i++)
        {
            if (!vin[i].segregatedSignatureData.IsNull())
                return true;
        }
        return false;
    }
};

typedef std::shared_ptr<const CTransaction> CTransactionRef;
static inline CTransactionRef MakeTransactionRef(int32_t nVersion_) { return std::make_shared<const CTransaction>(nVersion_); }
template <typename Tx> static inline CTransactionRef MakeTransactionRef(Tx&& txIn) { return std::make_shared<const CTransaction>(std::forward<Tx>(txIn)); }

/** Compute the weight of a transaction, as defined by BIP 141 */
int64_t GetTransactionWeight(const CTransaction &tx);

#endif // GULDEN_PRIMITIVES_TRANSACTION_H
