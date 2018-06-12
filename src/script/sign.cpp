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

#include "script/sign.h"

#include "key.h"
#include "keystore.h"
#include "policy/policy.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "uint256.h"
#include "validation/validation.h"



typedef std::vector<unsigned char> valtype;

TransactionSignatureCreator::TransactionSignatureCreator(CKeyID signingKeyID, const CKeyStore* keystoreIn, const CTransaction* txToIn, unsigned int nInIn, const CAmount& amountIn, int nHashTypeIn) : BaseSignatureCreator(keystoreIn), txTo(txToIn), nIn(nInIn), nHashType(nHashTypeIn), amount(amountIn), checker(signingKeyID, txTo, nIn, amountIn) {}

bool TransactionSignatureCreator::CreateSig(std::vector<unsigned char>& vchSig, const CKeyID& address, const CScript& scriptCode, SigVersion sigversion) const
{
    CKey key;
    if (!keystore->GetKey(address, key))
        return false;

    // Signing with uncompressed keys is disabled for segsig transactions
    if (sigversion == SIGVERSION_SEGSIG && !key.IsCompressed())
        return false;

    //LogPrintf(">>>>SignSignatureSigHash scriptcode=%s txto=%s nIn=%d nHashType=%d amount=%d sigversion=%d\n", HexStr(scriptCode.begin(), scriptCode.end()), txTo->ToString(), nIn, nHashType, amount, sigversion);
    //LogPrintf(">>>>SignSignatureSigHash hashPrevouts=%s hashSequence=%s hashOutputs=%s\n", this->txdata->hashPrevouts.ToString(), this->txdata->hashSequence.ToString(), this->txdata->hashOutputs.ToString());
    uint256 hash = SignatureHash(scriptCode, *txTo, nIn, nHashType, amount, sigversion);
    if (sigversion == SIGVERSION_SEGSIG)
    {
        //fixme: (2.0) MAKE SURE THIS WORKS FOR NORMAL (BITCOIN STYLE) TRANSACTIONS!!! - unit tests required.
        if (!key.SignCompact(hash, vchSig))
            return false;
    }
    else
    {
        if (!key.Sign(hash, vchSig))
            return false;
    }
    CPubKey pub = key.GetPubKey();
    //LogPrintf(">>>>SignSignature sig=%s key=%s hash=%s\n", HexStr(vchSig.begin(), vchSig.end()), HexStr(pub.begin(), pub.end()), HexStr(hash.begin(), hash.end()));
    vchSig.push_back((unsigned char)nHashType);
    return true;
}

static bool Sign1(const CKeyID& address, const BaseSignatureCreator& creator, const CScript& scriptCode, std::vector<valtype>& ret, SigVersion sigversion)
{
    std::vector<unsigned char> vchSig;
    if (!creator.CreateSig(vchSig, address, scriptCode, sigversion))
        return false;
    ret.push_back(vchSig);
    return true;
}

static bool SignN(const std::vector<valtype>& multisigdata, const BaseSignatureCreator& creator, const CScript& scriptCode, std::vector<valtype>& ret, SigVersion sigversion)
{
    int nSigned = 0;
    int nRequired = multisigdata.front()[0];
    for (unsigned int i = 1; i < multisigdata.size()-1 && nSigned < nRequired; i++)
    {
        const valtype& pubkey = multisigdata[i];
        CKeyID keyID = CPubKey(pubkey).GetID();
        if (Sign1(keyID, creator, scriptCode, ret, sigversion))
            ++nSigned;
    }
    return nSigned==nRequired;
}

/**
 * Sign scriptPubKey using signature made with creator.
 * Signatures are returned in scriptSigRet (or returns false if scriptPubKey can't be signed),
 * unless whichTypeRet is TX_SCRIPTHASH, in which case scriptSigRet is the redemption script.
 * Returns false if scriptPubKey could not be completely satisfied.
 */
static bool SignStep(const BaseSignatureCreator& creator, const CScript& scriptPubKey,
                     std::vector<valtype>& ret, txnouttype& whichTypeRet, SigVersion sigversion, SignType type)
{
    CScript scriptRet;
    uint160 h160;
    ret.clear();

    std::vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, whichTypeRet, vSolutions))
        return false;

    CKeyID keyID;
    switch (whichTypeRet)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        return false;
    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        return Sign1(keyID, creator, scriptPubKey, ret, sigversion);
    case TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        if (!Sign1(keyID, creator, scriptPubKey, ret, sigversion))
            return false;
        else
        {
            CPubKey vch;
            creator.KeyStore().GetPubKey(keyID, vch);
            ret.push_back(ToByteVector(vch));
        }
        return true;
    case TX_SCRIPTHASH:
        if (creator.KeyStore().GetCScript(uint160(vSolutions[0]), scriptRet)) {
            ret.push_back(std::vector<unsigned char>(scriptRet.begin(), scriptRet.end()));
            return true;
        }
        return false;

    case TX_MULTISIG:
        ret.push_back(valtype()); // workaround CHECKMULTISIG bug
        return (SignN(vSolutions, creator, scriptPubKey, ret, sigversion));

    case TX_PUBKEYHASH_POW2WITNESS:
    {
        CKeyID spendingKeyID = CKeyID(uint160(vSolutions[0]));
        CKeyID witnessKeyID = CKeyID(uint160(vSolutions[1]));

        switch(type)
        {
            case Spend:
            {
                if (!Sign1(spendingKeyID, creator, scriptPubKey, ret, sigversion))
                    return false;
                else
                {
                    CPubKey vch;
                    creator.KeyStore().GetPubKey(spendingKeyID, vch);
                    ret.push_back(ToByteVector(vch));
                }
                if (!Sign1(witnessKeyID, creator, scriptPubKey, ret, sigversion))
                    return false;
                else
                {
                    CPubKey vch;
                    creator.KeyStore().GetPubKey(witnessKeyID, vch);
                    ret.push_back(ToByteVector(vch));
                }
                break;
            }
            case Witness:
            {
                if (!Sign1(witnessKeyID, creator, scriptPubKey, ret, sigversion))
                    return false;
                else
                {
                    CPubKey vch;
                    creator.KeyStore().GetPubKey(witnessKeyID, vch);
                    ret.push_back(ToByteVector(vch));
                }
                break;
            }
        }
        return true;
    }

    default:
        return false;
    }
}

//fixme: (2.0) (HIGH) testme...
static bool SignStep(const BaseSignatureCreator& creator, const CTxOutPoW2Witness& pow2Witness, std::vector<valtype>& ret, SigVersion sigversion, SignType type)
{
    CScript scriptRet;
    uint160 h160;
    ret.clear();

    //fixme: (2.0) HIGH - Should this incorporate unique transaction data to avoid weakening the signature?
    //As we have no segregated signature data to sign we instead sign a standard placeholder.
    std::vector<unsigned char> sSignatureDataPlaceholder = {'p','o','w','2','w','i','t','n','e','s','s'};
    CScript scriptSignatureDataPlaceholder(sSignatureDataPlaceholder.begin(), sSignatureDataPlaceholder.end());

    switch(type)
    {
        case Spend:
        {
            if (!Sign1(pow2Witness.witnessKeyID, creator, scriptSignatureDataPlaceholder, ret, SIGVERSION_SEGSIG))
                return false;
            if (!Sign1(pow2Witness.spendingKeyID, creator, scriptSignatureDataPlaceholder, ret, SIGVERSION_SEGSIG))
                return false;
            return true;
        }
        case Witness:
        {
            if (!Sign1(pow2Witness.witnessKeyID, creator, scriptSignatureDataPlaceholder, ret, SIGVERSION_SEGSIG))
                return false;
            return true;
        }
    }
    return false;
}

static bool SignStep(const BaseSignatureCreator& creator, const CTxOutStandardKeyHash& standardKeyHash, std::vector<valtype>& ret, SigVersion sigversion, SignType type)
{
    CScript scriptRet;
    uint160 h160;
    ret.clear();

    //fixme: (2.0) HIGH - Should this incorporate unique transaction data to avoid weakening the signature?
    //As we have no segregated signature data to sign we instead sign a standard placeholder.
    std::vector<unsigned char> sSignatureDataPlaceholder = {'k','e','y','h','a','s','h'};
    CScript scriptSignatureDataPlaceholder(sSignatureDataPlaceholder.begin(), sSignatureDataPlaceholder.end());

    if (!Sign1(standardKeyHash.keyID, creator, scriptSignatureDataPlaceholder, ret, SIGVERSION_SEGSIG))
        return false;
    return true;
}

static CScript PushAll(const std::vector<valtype>& values)
{
    CScript result;
    for(const valtype& v : values) {
        if (v.size() == 0) {
            result << OP_0;
        } else if (v.size() == 1 && v[0] >= 1 && v[0] <= 16) {
            result << CScript::EncodeOP_N(v[0]);
        } else {
            result << v;
        }
    }
    return result;
}

//fixme: (2.0) (HIGH) (MULTISIG?)
class CSigningKeysVisitor : public boost::static_visitor<void> {
public:
    std::vector<CKeyID> vKeys;
    SignType type;
    CSigningKeysVisitor(SignType type_) : type(type_) {}

    void Process(const CTxDestination& dest)
    {
        boost::apply_visitor(*this, dest);
    }

    void operator()(const CKeyID &keyId)
    {
        vKeys.push_back(keyId);
    }

    void operator()(const CScriptID &scriptId)
    {
        //fixme: (2.0) (HIGH)
    }

    void operator()(const CPoW2WitnessDestination &dest) {
        //fixme: (2.0) (HIGH) (stacked signing?)
        if (type == SignType::Witness)
            vKeys.push_back(dest.witnessKey);
        else if (type == SignType::Spend)
            vKeys.push_back(dest.spendingKey);
    }

    void operator()(const CNoDestination &none) {}
};

CKeyID ExtractSigningPubkeyFromTxOutput(const CTxOut& txOut, SignType type)
{
    switch(txOut.GetType())
    {
        case ScriptLegacyOutput:
        {
            //fixme: (2.0) HIGH NEXT - implement.
            //=== CDestination(pubkey))
            CTxDestination dest;
            if (!ExtractDestination(txOut.output.scriptPubKey, dest))
                return CKeyID();

            CSigningKeysVisitor getSigningKeys(type);
            getSigningKeys.Process(dest);
            //fixme: (2.0) (HIGH) MULTISIG
            if (getSigningKeys.vKeys.size() != 1)
                return CKeyID();
            return getSigningKeys.vKeys[0];
        }
        case PoW2WitnessOutput:
        {
            //fixme: (2.0) (stacked signing?) HIGH NEXT
            if (type == SignType::Spend)
                return txOut.output.witnessDetails.spendingKeyID;
            else if(type == SignType::Witness)
                return txOut.output.witnessDetails.witnessKeyID;
            return CKeyID();
        }
        case StandardKeyHashOutput:
            return txOut.output.standardKeyHash.keyID;
    }
    return CKeyID();
}

bool ProduceSignature(const BaseSignatureCreator& creator, const CTxOut& fromOutput, SignatureData& sigdata, SignType type, uint64_t nVersion)
{
    if (fromOutput.GetType() <= CTxOutType::ScriptLegacyOutput)
    {
        CScript script = fromOutput.output.scriptPubKey;
        std::vector<valtype> result;
        txnouttype whichType;
        bool solved = SignStep(creator, script, result, whichType, SIGVERSION_BASE, type);
        CScript subscript;
        sigdata.segregatedSignatureData.stack.clear();

        if (!IsOldTransactionVersion(nVersion))
        {
            if (solved && whichType == TX_SCRIPTHASH)
            {
                //fixme: (2.0) HIGH NEXTNEXTNEXT
                 sigdata.segregatedSignatureData.stack = result;
            }
            else
            {
                sigdata.segregatedSignatureData.stack = result;
            }
        }
        else
        {
            if (solved && whichType == TX_SCRIPTHASH)
            {
                // Solver returns the subscript that needs to be evaluated;
                // the final scriptSig is the signatures from that
                // and then the serialized subscript:
                script = subscript = CScript(result[0].begin(), result[0].end());
                solved = solved && SignStep(creator, script, result, whichType, SIGVERSION_BASE, type) && whichType != TX_SCRIPTHASH;
                result.push_back(std::vector<unsigned char>(subscript.begin(), subscript.end()));
            }
            sigdata.scriptSig = PushAll(result);
        }
        // Test solution
        return solved;
    }
    else if (fromOutput.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        //fixme: (2.0) TESTME
        std::vector<valtype> result;
        bool solved = SignStep(creator, fromOutput.output.witnessDetails, result, SIGVERSION_BASE, type);
        sigdata.segregatedSignatureData.stack = result;

        //fixme: (2.0) - Do we need to verify anything here?
        return solved;
    }
    else if (fromOutput.GetType() == CTxOutType::StandardKeyHashOutput)
    {
        //fixme: (2.0) TESTME
        std::vector<valtype> result;
        bool solved = SignStep(creator, fromOutput.output.standardKeyHash, result, SIGVERSION_BASE, type);
        sigdata.segregatedSignatureData.stack = result;

        return solved;
    }
    else
    {
        assert(0);
    }
    return false;
}

SignatureData DataFromTransaction(const CMutableTransaction& tx, unsigned int nIn)
{
    SignatureData data;
    assert(tx.vin.size() > nIn);
    data.scriptSig = tx.vin[nIn].scriptSig;
    data.segregatedSignatureData = tx.vin[nIn].segregatedSignatureData;
    return data;
}

void UpdateTransaction(CMutableTransaction& tx, unsigned int nIn, const SignatureData& data)
{
    assert(tx.vin.size() > nIn);
    tx.vin[nIn].scriptSig = data.scriptSig;
    tx.vin[nIn].segregatedSignatureData = data.segregatedSignatureData;
}

bool SignSignature(const CKeyStore &keystore, const CTxOut& fromOutput, CMutableTransaction& txTo, unsigned int nIn, const CAmount& amount, int nHashType, SignType type)
{
    assert(nIn < txTo.vin.size());

    CTransaction txToConst(txTo);
    //fixme: (2.0) (HIGH) (sign type)
    CKeyID signingKeyID = ExtractSigningPubkeyFromTxOutput(fromOutput, SignType::Spend);
    TransactionSignatureCreator creator(signingKeyID, &keystore, &txToConst, nIn, amount, nHashType);

    SignatureData sigdata;
    bool ret = ProduceSignature(creator, fromOutput, sigdata, type, txToConst.nVersion);
    UpdateTransaction(txTo, nIn, sigdata);
    return ret;
}

bool SignSignature(const CKeyStore &keystore, const CTransaction& txFrom, CMutableTransaction& txTo, unsigned int nIn, int nHashType, SignType type)
{
    assert(nIn < txTo.vin.size());
    CTxIn& txin = txTo.vin[nIn];
    assert(txin.prevout.n < txFrom.vout.size());
    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    return SignSignature(keystore, txout, txTo, nIn, txout.nValue, nHashType, type);
}

static std::vector<valtype> CombineMultisig(const CScript& scriptPubKey, const BaseSignatureChecker& checker,
                               const std::vector<valtype>& vSolutions,
                               const std::vector<valtype>& sigs1, const std::vector<valtype>& sigs2, SigVersion sigversion)
{
    // Combine all the signatures we've got:
    std::set<valtype> allsigs;
    for(const valtype& v : sigs1)
    {
        if (!v.empty())
            allsigs.insert(v);
    }
    for(const valtype& v : sigs2)
    {
        if (!v.empty())
            allsigs.insert(v);
    }

    // Build a map of pubkey -> signature by matching sigs to pubkeys:
    assert(vSolutions.size() > 1);
    unsigned int nSigsRequired = vSolutions.front()[0];
    unsigned int nPubKeys = vSolutions.size()-2;
    std::map<valtype, valtype> sigs;
    for(const valtype& sig : allsigs)
    {
        for (unsigned int i = 0; i < nPubKeys; i++)
        {
            const valtype& pubkey = vSolutions[i+1];
            if (sigs.count(pubkey))
                continue; // Already got a sig for this pubkey

            if (checker.CheckSig(sig, pubkey, scriptPubKey, sigversion))
            {
                sigs[pubkey] = sig;
                break;
            }
        }
    }
    // Now build a merged CScript:
    unsigned int nSigsHave = 0;
    std::vector<valtype> result; result.push_back(valtype()); // pop-one-too-many workaround
    for (unsigned int i = 0; i < nPubKeys && nSigsHave < nSigsRequired; i++)
    {
        if (sigs.count(vSolutions[i+1]))
        {
            result.push_back(sigs[vSolutions[i+1]]);
            ++nSigsHave;
        }
    }
    // Fill any missing with OP_0:
    for (unsigned int i = nSigsHave; i < nSigsRequired; i++)
        result.push_back(valtype());

    return result;
}

namespace
{
struct Stacks
{
    std::vector<valtype> script;
    std::vector<valtype> segregatedSignatureData;

    Stacks() {}
    explicit Stacks(const std::vector<valtype>& scriptSigStack_) : script(scriptSigStack_), segregatedSignatureData() {}
    explicit Stacks(const SignatureData& data) : segregatedSignatureData(data.segregatedSignatureData.stack) {
        EvalScript(script, data.scriptSig, SCRIPT_VERIFY_STRICTENC, BaseSignatureChecker(), SIGVERSION_BASE);
    }

    SignatureData Output() const {
        SignatureData result;
        result.scriptSig = PushAll(script);
        result.segregatedSignatureData.stack = segregatedSignatureData;
        return result;
    }
};
}

static Stacks CombineSignatures(const CScript& scriptPubKey, const BaseSignatureChecker& checker,
                                 const txnouttype txType, const std::vector<valtype>& vSolutions,
                                 Stacks sigs1, Stacks sigs2, SigVersion sigversion)
{
    switch (txType)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        // Don't know anything about this, assume bigger one is correct:
        if (sigs1.script.size() >= sigs2.script.size())
            return sigs1;
        return sigs2;
    case TX_PUBKEY:
    case TX_PUBKEYHASH:
        // Signatures are bigger than placeholders or empty scripts:
        if (sigs1.script.empty() || sigs1.script[0].empty())
            return sigs2;
        return sigs1;
    case TX_PUBKEYHASH_POW2WITNESS:
        //fixme: (2.0) NEXTNEXT
        //We need to devise a way to sign with the right key here (both keys if it is a spend, witness key if just witnessing)
        return sigs1;
    case TX_SCRIPTHASH:
        if (sigs1.script.empty() || sigs1.script.back().empty())
            return sigs2;
        else if (sigs2.script.empty() || sigs2.script.back().empty())
            return sigs1;
        else
        {
            // Recur to combine:
            valtype spk = sigs1.script.back();
            CScript pubKey2(spk.begin(), spk.end());

            txnouttype txType2;
            std::vector<std::vector<unsigned char> > vSolutions2;
            Solver(pubKey2, txType2, vSolutions2);
            sigs1.script.pop_back();
            sigs2.script.pop_back();
            Stacks result = CombineSignatures(pubKey2, checker, txType2, vSolutions2, sigs1, sigs2, sigversion);
            result.script.push_back(spk);
            return result;
        }
    case TX_MULTISIG:
        return Stacks(CombineMultisig(scriptPubKey, checker, vSolutions, sigs1.script, sigs2.script, sigversion));
    default:
        return Stacks();
    }
}

SignatureData CombineSignatures(const CScript& scriptPubKey, const BaseSignatureChecker& checker,
                          const SignatureData& scriptSig1, const SignatureData& scriptSig2)
{
    txnouttype txType;
    std::vector<std::vector<unsigned char> > vSolutions;
    Solver(scriptPubKey, txType, vSolutions);

    return CombineSignatures(scriptPubKey, checker, txType, vSolutions, Stacks(scriptSig1), Stacks(scriptSig2), SIGVERSION_BASE).Output();
}

namespace {
/** Dummy signature checker which accepts all signatures. */
class DummySignatureChecker : public BaseSignatureChecker
{
public:
    DummySignatureChecker() {}

    bool CheckSig(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const
    {
        return true;
    }
};
const DummySignatureChecker dummyChecker;
}

const BaseSignatureChecker& DummySignatureCreator::Checker() const
{
    return dummyChecker;
}

bool DummySignatureCreator::CreateSig(std::vector<unsigned char>& vchSig, const CKeyID& keyid, const CScript& scriptCode, SigVersion sigversion) const
{
    // Create a dummy signature that is a valid DER-encoding
    vchSig.assign(72, '\000');
    vchSig[0] = 0x30;
    vchSig[1] = 69;
    vchSig[2] = 0x02;
    vchSig[3] = 33;
    vchSig[4] = 0x01;
    vchSig[4 + 33] = 0x02;
    vchSig[5 + 33] = 32;
    vchSig[6 + 33] = 0x01;
    vchSig[6 + 33 + 32] = SIGHASH_ALL;
    return true;
}
