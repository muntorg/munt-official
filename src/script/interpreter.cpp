// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "interpreter.h"

#include "primitives/transaction.h"
#include "crypto/ripemd160.h"
#include "crypto/sha1.h"
#include "crypto/sha256.h"
#include "pubkey.h"
#include "script/script.h"
#include "uint256.h"

using namespace std;

typedef vector<unsigned char> valtype;

namespace {

inline bool set_success(ScriptError* ret)
{
    if (ret)
        *ret = SCRIPT_ERR_OK;
    return true;
}

inline bool set_error(ScriptError* ret, const ScriptError serror)
{
    if (ret)
        *ret = serror;
    return false;
}

} // anon namespace

bool CastToBool(const valtype& vch)
{
    for (unsigned int i = 0; i < vch.size(); i++) {
        if (vch[i] != 0) {

            if (i == vch.size() - 1 && vch[i] == 0x80)
                return false;
            return true;
        }
    }
    return false;
}

/**
 * Script is a stack machine (like Forth) that evaluates a predicate
 * returning a bool indicating valid or not.  There are no loops.
 */
#define stacktop(i) (stack.at(stack.size() + (i)))
#define altstacktop(i) (altstack.at(altstack.size() + (i)))
static inline void popstack(vector<valtype>& stack)
{
    if (stack.empty())
        throw runtime_error("popstack(): stack empty");
    stack.pop_back();
}

bool static IsCompressedOrUncompressedPubKey(const valtype& vchPubKey)
{
    if (vchPubKey.size() < 33) {

        return false;
    }
    if (vchPubKey[0] == 0x04) {
        if (vchPubKey.size() != 65) {

            return false;
        }
    } else if (vchPubKey[0] == 0x02 || vchPubKey[0] == 0x03) {
        if (vchPubKey.size() != 33) {

            return false;
        }
    } else {

        return false;
    }
    return true;
}

/**
 * A canonical signature exists of: <30> <total len> <02> <len R> <R> <02> <len S> <S> <hashtype>
 * Where R and S are not negative (their first byte has its highest bit not set), and not
 * excessively padded (do not start with a 0 byte, unless an otherwise negative number follows,
 * in which case a single 0 byte is necessary and even required).
 * 
 * See https://bitcointalk.org/index.php?topic=8392.msg127623#msg127623
 *
 * This function is consensus-critical since BIP66.
 */
bool static IsValidSignatureEncoding(const std::vector<unsigned char>& sig)
{

    if (sig.size() < 9)
        return false;
    if (sig.size() > 73)
        return false;

    if (sig[0] != 0x30)
        return false;

    if (sig[1] != sig.size() - 3)
        return false;

    unsigned int lenR = sig[3];

    if (5 + lenR >= sig.size())
        return false;

    unsigned int lenS = sig[5 + lenR];

    if ((size_t)(lenR + lenS + 7) != sig.size())
        return false;

    if (sig[2] != 0x02)
        return false;

    if (lenR == 0)
        return false;

    if (sig[4] & 0x80)
        return false;

    if (lenR > 1 && (sig[4] == 0x00) && !(sig[5] & 0x80))
        return false;

    if (sig[lenR + 4] != 0x02)
        return false;

    if (lenS == 0)
        return false;

    if (sig[lenR + 6] & 0x80)
        return false;

    if (lenS > 1 && (sig[lenR + 6] == 0x00) && !(sig[lenR + 7] & 0x80))
        return false;

    return true;
}

bool static IsLowDERSignature(const valtype& vchSig, ScriptError* serror)
{
    if (!IsValidSignatureEncoding(vchSig)) {
        return set_error(serror, SCRIPT_ERR_SIG_DER);
    }
    std::vector<unsigned char> vchSigCopy(vchSig.begin(), vchSig.begin() + vchSig.size() - 1);
    if (!CPubKey::CheckLowS(vchSigCopy)) {
        return set_error(serror, SCRIPT_ERR_SIG_HIGH_S);
    }
    return true;
}

bool static IsDefinedHashtypeSignature(const valtype& vchSig)
{
    if (vchSig.size() == 0) {
        return false;
    }
    unsigned char nHashType = vchSig[vchSig.size() - 1] & (~(SIGHASH_ANYONECANPAY));
    if (nHashType < SIGHASH_ALL || nHashType > SIGHASH_SINGLE)
        return false;

    return true;
}

bool CheckSignatureEncoding(const vector<unsigned char>& vchSig, unsigned int flags, ScriptError* serror)
{

    if (vchSig.size() == 0) {
        return true;
    }
    if ((flags & (SCRIPT_VERIFY_DERSIG | SCRIPT_VERIFY_LOW_S | SCRIPT_VERIFY_STRICTENC)) != 0 && !IsValidSignatureEncoding(vchSig)) {
        return set_error(serror, SCRIPT_ERR_SIG_DER);
    } else if ((flags & SCRIPT_VERIFY_LOW_S) != 0 && !IsLowDERSignature(vchSig, serror)) {

        return false;
    } else if ((flags & SCRIPT_VERIFY_STRICTENC) != 0 && !IsDefinedHashtypeSignature(vchSig)) {
        return set_error(serror, SCRIPT_ERR_SIG_HASHTYPE);
    }
    return true;
}

bool static CheckPubKeyEncoding(const valtype& vchSig, unsigned int flags, ScriptError* serror)
{
    if ((flags & SCRIPT_VERIFY_STRICTENC) != 0 && !IsCompressedOrUncompressedPubKey(vchSig)) {
        return set_error(serror, SCRIPT_ERR_PUBKEYTYPE);
    }
    return true;
}

bool static CheckMinimalPush(const valtype& data, opcodetype opcode)
{
    if (data.size() == 0) {

        return opcode == OP_0;
    } else if (data.size() == 1 && data[0] >= 1 && data[0] <= 16) {

        return opcode == OP_1 + (data[0] - 1);
    } else if (data.size() == 1 && data[0] == 0x81) {

        return opcode == OP_1NEGATE;
    } else if (data.size() <= 75) {

        return opcode == data.size();
    } else if (data.size() <= 255) {

        return opcode == OP_PUSHDATA1;
    } else if (data.size() <= 65535) {

        return opcode == OP_PUSHDATA2;
    }
    return true;
}

bool EvalScript(vector<vector<unsigned char> >& stack, const CScript& script, unsigned int flags, const BaseSignatureChecker& checker, SigVersion sigversion, ScriptError* serror)
{
    static const CScriptNum bnZero(0);
    static const CScriptNum bnOne(1);
    static const CScriptNum bnFalse(0);
    static const CScriptNum bnTrue(1);
    static const valtype vchFalse(0);
    static const valtype vchZero(0);
    static const valtype vchTrue(1, 1);

    CScript::const_iterator pc = script.begin();
    CScript::const_iterator pend = script.end();
    CScript::const_iterator pbegincodehash = script.begin();
    opcodetype opcode;
    valtype vchPushValue;
    vector<bool> vfExec;
    vector<valtype> altstack;
    set_error(serror, SCRIPT_ERR_UNKNOWN_ERROR);
    if (script.size() > MAX_SCRIPT_SIZE)
        return set_error(serror, SCRIPT_ERR_SCRIPT_SIZE);
    int nOpCount = 0;
    bool fRequireMinimal = (flags & SCRIPT_VERIFY_MINIMALDATA) != 0;

    try {
        while (pc < pend) {
            bool fExec = !count(vfExec.begin(), vfExec.end(), false);

            if (!script.GetOp(pc, opcode, vchPushValue))
                return set_error(serror, SCRIPT_ERR_BAD_OPCODE);
            if (vchPushValue.size() > MAX_SCRIPT_ELEMENT_SIZE)
                return set_error(serror, SCRIPT_ERR_PUSH_SIZE);

            if (opcode > OP_16 && ++nOpCount > MAX_OPS_PER_SCRIPT)
                return set_error(serror, SCRIPT_ERR_OP_COUNT);

            if (opcode == OP_CAT || opcode == OP_SUBSTR || opcode == OP_LEFT || opcode == OP_RIGHT || opcode == OP_INVERT || opcode == OP_AND || opcode == OP_OR || opcode == OP_XOR || opcode == OP_2MUL || opcode == OP_2DIV || opcode == OP_MUL || opcode == OP_DIV || opcode == OP_MOD || opcode == OP_LSHIFT || opcode == OP_RSHIFT)
                return set_error(serror, SCRIPT_ERR_DISABLED_OPCODE); // Disabled opcodes.

            if (fExec && 0 <= opcode && opcode <= OP_PUSHDATA4) {
                if (fRequireMinimal && !CheckMinimalPush(vchPushValue, opcode)) {
                    return set_error(serror, SCRIPT_ERR_MINIMALDATA);
                }
                stack.push_back(vchPushValue);
            } else if (fExec || (OP_IF <= opcode && opcode <= OP_ENDIF))
                switch (opcode) {

                case OP_1NEGATE:
                case OP_1:
                case OP_2:
                case OP_3:
                case OP_4:
                case OP_5:
                case OP_6:
                case OP_7:
                case OP_8:
                case OP_9:
                case OP_10:
                case OP_11:
                case OP_12:
                case OP_13:
                case OP_14:
                case OP_15:
                case OP_16: {

                    CScriptNum bn((int)opcode - (int)(OP_1 - 1));
                    stack.push_back(bn.getvch());

                } break;

                case OP_NOP:
                    break;

                case OP_CHECKLOCKTIMEVERIFY: {
                    if (!(flags & SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY)) {

                        if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS) {
                            return set_error(serror, SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS);
                        }
                        break;
                    }

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

                    const CScriptNum nLockTime(stacktop(-1), fRequireMinimal, 5);

                    if (nLockTime < 0)
                        return set_error(serror, SCRIPT_ERR_NEGATIVE_LOCKTIME);

                    if (!checker.CheckLockTime(nLockTime))
                        return set_error(serror, SCRIPT_ERR_UNSATISFIED_LOCKTIME);

                    break;
                }

                case OP_CHECKSEQUENCEVERIFY: {
                    if (!(flags & SCRIPT_VERIFY_CHECKSEQUENCEVERIFY)) {

                        if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS) {
                            return set_error(serror, SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS);
                        }
                        break;
                    }

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

                    const CScriptNum nSequence(stacktop(-1), fRequireMinimal, 5);

                    if (nSequence < 0)
                        return set_error(serror, SCRIPT_ERR_NEGATIVE_LOCKTIME);

                    if ((nSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) != 0)
                        break;

                    if (!checker.CheckSequence(nSequence))
                        return set_error(serror, SCRIPT_ERR_UNSATISFIED_LOCKTIME);

                    break;
                }

                case OP_NOP1:
                case OP_NOP4:
                case OP_NOP5:
                case OP_NOP6:
                case OP_NOP7:
                case OP_NOP8:
                case OP_NOP9:
                case OP_NOP10: {
                    if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS)
                        return set_error(serror, SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS);
                } break;

                case OP_IF:
                case OP_NOTIF: {

                    bool fValue = false;
                    if (fExec) {
                        if (stack.size() < 1)
                            return set_error(serror, SCRIPT_ERR_UNBALANCED_CONDITIONAL);
                        valtype& vch = stacktop(-1);
                        fValue = CastToBool(vch);
                        if (opcode == OP_NOTIF)
                            fValue = !fValue;
                        popstack(stack);
                    }
                    vfExec.push_back(fValue);
                } break;

                case OP_ELSE: {
                    if (vfExec.empty())
                        return set_error(serror, SCRIPT_ERR_UNBALANCED_CONDITIONAL);
                    vfExec.back() = !vfExec.back();
                } break;

                case OP_ENDIF: {
                    if (vfExec.empty())
                        return set_error(serror, SCRIPT_ERR_UNBALANCED_CONDITIONAL);
                    vfExec.pop_back();
                } break;

                case OP_VERIFY: {

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    bool fValue = CastToBool(stacktop(-1));
                    if (fValue)
                        popstack(stack);
                    else
                        return set_error(serror, SCRIPT_ERR_VERIFY);
                } break;

                case OP_RETURN: {
                    return set_error(serror, SCRIPT_ERR_OP_RETURN);
                } break;

                case OP_TOALTSTACK: {
                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    altstack.push_back(stacktop(-1));
                    popstack(stack);
                } break;

                case OP_FROMALTSTACK: {
                    if (altstack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_ALTSTACK_OPERATION);
                    stack.push_back(altstacktop(-1));
                    popstack(altstack);
                } break;

                case OP_2DROP: {

                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    popstack(stack);
                    popstack(stack);
                } break;

                case OP_2DUP: {

                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-2);
                    valtype vch2 = stacktop(-1);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                } break;

                case OP_3DUP: {

                    if (stack.size() < 3)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-3);
                    valtype vch2 = stacktop(-2);
                    valtype vch3 = stacktop(-1);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                    stack.push_back(vch3);
                } break;

                case OP_2OVER: {

                    if (stack.size() < 4)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-4);
                    valtype vch2 = stacktop(-3);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                } break;

                case OP_2ROT: {

                    if (stack.size() < 6)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-6);
                    valtype vch2 = stacktop(-5);
                    stack.erase(stack.end() - 6, stack.end() - 4);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                } break;

                case OP_2SWAP: {

                    if (stack.size() < 4)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    swap(stacktop(-4), stacktop(-2));
                    swap(stacktop(-3), stacktop(-1));
                } break;

                case OP_IFDUP: {

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-1);
                    if (CastToBool(vch))
                        stack.push_back(vch);
                } break;

                case OP_DEPTH: {

                    CScriptNum bn(stack.size());
                    stack.push_back(bn.getvch());
                } break;

                case OP_DROP: {

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    popstack(stack);
                } break;

                case OP_DUP: {

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-1);
                    stack.push_back(vch);
                } break;

                case OP_NIP: {

                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    stack.erase(stack.end() - 2);
                } break;

                case OP_OVER: {

                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-2);
                    stack.push_back(vch);
                } break;

                case OP_PICK:
                case OP_ROLL: {

                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    int n = CScriptNum(stacktop(-1), fRequireMinimal).getint();
                    popstack(stack);
                    if (n < 0 || n >= (int)stack.size())
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-n - 1);
                    if (opcode == OP_ROLL)
                        stack.erase(stack.end() - n - 1);
                    stack.push_back(vch);
                } break;

                case OP_ROT: {

                    if (stack.size() < 3)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    swap(stacktop(-3), stacktop(-2));
                    swap(stacktop(-2), stacktop(-1));
                } break;

                case OP_SWAP: {

                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    swap(stacktop(-2), stacktop(-1));
                } break;

                case OP_TUCK: {

                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-1);
                    stack.insert(stack.end() - 2, vch);
                } break;

                case OP_SIZE: {

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CScriptNum bn(stacktop(-1).size());
                    stack.push_back(bn.getvch());
                } break;

                case OP_EQUAL:
                case OP_EQUALVERIFY:

                {

                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype& vch1 = stacktop(-2);
                    valtype& vch2 = stacktop(-1);
                    bool fEqual = (vch1 == vch2);

                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fEqual ? vchTrue : vchFalse);
                    if (opcode == OP_EQUALVERIFY) {
                        if (fEqual)
                            popstack(stack);
                        else
                            return set_error(serror, SCRIPT_ERR_EQUALVERIFY);
                    }
                } break;

                case OP_1ADD:
                case OP_1SUB:
                case OP_NEGATE:
                case OP_ABS:
                case OP_NOT:
                case OP_0NOTEQUAL: {

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CScriptNum bn(stacktop(-1), fRequireMinimal);
                    switch (opcode) {
                    case OP_1ADD:
                        bn += bnOne;
                        break;
                    case OP_1SUB:
                        bn -= bnOne;
                        break;
                    case OP_NEGATE:
                        bn = -bn;
                        break;
                    case OP_ABS:
                        if (bn < bnZero)
                            bn = -bn;
                        break;
                    case OP_NOT:
                        bn = (bn == bnZero);
                        break;
                    case OP_0NOTEQUAL:
                        bn = (bn != bnZero);
                        break;
                    default:
                        assert(!"invalid opcode");
                        break;
                    }
                    popstack(stack);
                    stack.push_back(bn.getvch());
                } break;

                case OP_ADD:
                case OP_SUB:
                case OP_BOOLAND:
                case OP_BOOLOR:
                case OP_NUMEQUAL:
                case OP_NUMEQUALVERIFY:
                case OP_NUMNOTEQUAL:
                case OP_LESSTHAN:
                case OP_GREATERTHAN:
                case OP_LESSTHANOREQUAL:
                case OP_GREATERTHANOREQUAL:
                case OP_MIN:
                case OP_MAX: {

                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CScriptNum bn1(stacktop(-2), fRequireMinimal);
                    CScriptNum bn2(stacktop(-1), fRequireMinimal);
                    CScriptNum bn(0);
                    switch (opcode) {
                    case OP_ADD:
                        bn = bn1 + bn2;
                        break;

                    case OP_SUB:
                        bn = bn1 - bn2;
                        break;

                    case OP_BOOLAND:
                        bn = (bn1 != bnZero && bn2 != bnZero);
                        break;
                    case OP_BOOLOR:
                        bn = (bn1 != bnZero || bn2 != bnZero);
                        break;
                    case OP_NUMEQUAL:
                        bn = (bn1 == bn2);
                        break;
                    case OP_NUMEQUALVERIFY:
                        bn = (bn1 == bn2);
                        break;
                    case OP_NUMNOTEQUAL:
                        bn = (bn1 != bn2);
                        break;
                    case OP_LESSTHAN:
                        bn = (bn1 < bn2);
                        break;
                    case OP_GREATERTHAN:
                        bn = (bn1 > bn2);
                        break;
                    case OP_LESSTHANOREQUAL:
                        bn = (bn1 <= bn2);
                        break;
                    case OP_GREATERTHANOREQUAL:
                        bn = (bn1 >= bn2);
                        break;
                    case OP_MIN:
                        bn = (bn1 < bn2 ? bn1 : bn2);
                        break;
                    case OP_MAX:
                        bn = (bn1 > bn2 ? bn1 : bn2);
                        break;
                    default:
                        assert(!"invalid opcode");
                        break;
                    }
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(bn.getvch());

                    if (opcode == OP_NUMEQUALVERIFY) {
                        if (CastToBool(stacktop(-1)))
                            popstack(stack);
                        else
                            return set_error(serror, SCRIPT_ERR_NUMEQUALVERIFY);
                    }
                } break;

                case OP_WITHIN: {

                    if (stack.size() < 3)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CScriptNum bn1(stacktop(-3), fRequireMinimal);
                    CScriptNum bn2(stacktop(-2), fRequireMinimal);
                    CScriptNum bn3(stacktop(-1), fRequireMinimal);
                    bool fValue = (bn2 <= bn1 && bn1 < bn3);
                    popstack(stack);
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fValue ? vchTrue : vchFalse);
                } break;

                case OP_RIPEMD160:
                case OP_SHA1:
                case OP_SHA256:
                case OP_HASH160:
                case OP_HASH256: {

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype& vch = stacktop(-1);
                    valtype vchHash((opcode == OP_RIPEMD160 || opcode == OP_SHA1 || opcode == OP_HASH160) ? 20 : 32);
                    if (opcode == OP_RIPEMD160)
                        CRIPEMD160().Write(begin_ptr(vch), vch.size()).Finalize(begin_ptr(vchHash));
                    else if (opcode == OP_SHA1)
                        CSHA1().Write(begin_ptr(vch), vch.size()).Finalize(begin_ptr(vchHash));
                    else if (opcode == OP_SHA256)
                        CSHA256().Write(begin_ptr(vch), vch.size()).Finalize(begin_ptr(vchHash));
                    else if (opcode == OP_HASH160)
                        CHash160().Write(begin_ptr(vch), vch.size()).Finalize(begin_ptr(vchHash));
                    else if (opcode == OP_HASH256)
                        CHash256().Write(begin_ptr(vch), vch.size()).Finalize(begin_ptr(vchHash));
                    popstack(stack);
                    stack.push_back(vchHash);
                } break;

                case OP_CODESEPARATOR: {

                    pbegincodehash = pc;
                } break;

                case OP_CHECKSIG:
                case OP_CHECKSIGVERIFY: {

                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

                    valtype& vchSig = stacktop(-2);
                    valtype& vchPubKey = stacktop(-1);

                    CScript scriptCode(pbegincodehash, pend);

                    if (sigversion == SIGVERSION_BASE) {
                        scriptCode.FindAndDelete(CScript(vchSig));
                    }

                    if (!CheckSignatureEncoding(vchSig, flags, serror) || !CheckPubKeyEncoding(vchPubKey, flags, serror)) {

                        return false;
                    }
                    bool fSuccess = checker.CheckSig(vchSig, vchPubKey, scriptCode, sigversion);

                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fSuccess ? vchTrue : vchFalse);
                    if (opcode == OP_CHECKSIGVERIFY) {
                        if (fSuccess)
                            popstack(stack);
                        else
                            return set_error(serror, SCRIPT_ERR_CHECKSIGVERIFY);
                    }
                } break;

                case OP_CHECKMULTISIG:
                case OP_CHECKMULTISIGVERIFY: {

                    int i = 1;
                    if ((int)stack.size() < i)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

                    int nKeysCount = CScriptNum(stacktop(-i), fRequireMinimal).getint();
                    if (nKeysCount < 0 || nKeysCount > MAX_PUBKEYS_PER_MULTISIG)
                        return set_error(serror, SCRIPT_ERR_PUBKEY_COUNT);
                    nOpCount += nKeysCount;
                    if (nOpCount > MAX_OPS_PER_SCRIPT)
                        return set_error(serror, SCRIPT_ERR_OP_COUNT);
                    int ikey = ++i;
                    i += nKeysCount;
                    if ((int)stack.size() < i)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

                    int nSigsCount = CScriptNum(stacktop(-i), fRequireMinimal).getint();
                    if (nSigsCount < 0 || nSigsCount > nKeysCount)
                        return set_error(serror, SCRIPT_ERR_SIG_COUNT);
                    int isig = ++i;
                    i += nSigsCount;
                    if ((int)stack.size() < i)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

                    CScript scriptCode(pbegincodehash, pend);

                    for (int k = 0; k < nSigsCount; k++) {
                        valtype& vchSig = stacktop(-isig - k);
                        if (sigversion == SIGVERSION_BASE) {
                            scriptCode.FindAndDelete(CScript(vchSig));
                        }
                    }

                    bool fSuccess = true;
                    while (fSuccess && nSigsCount > 0) {
                        valtype& vchSig = stacktop(-isig);
                        valtype& vchPubKey = stacktop(-ikey);

                        if (!CheckSignatureEncoding(vchSig, flags, serror) || !CheckPubKeyEncoding(vchPubKey, flags, serror)) {

                            return false;
                        }

                        bool fOk = checker.CheckSig(vchSig, vchPubKey, scriptCode, sigversion);

                        if (fOk) {
                            isig++;
                            nSigsCount--;
                        }
                        ikey++;
                        nKeysCount--;

                        if (nSigsCount > nKeysCount)
                            fSuccess = false;
                    }

                    while (i-- > 1)
                        popstack(stack);

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    if ((flags & SCRIPT_VERIFY_NULLDUMMY) && stacktop(-1).size())
                        return set_error(serror, SCRIPT_ERR_SIG_NULLDUMMY);
                    popstack(stack);

                    stack.push_back(fSuccess ? vchTrue : vchFalse);

                    if (opcode == OP_CHECKMULTISIGVERIFY) {
                        if (fSuccess)
                            popstack(stack);
                        else
                            return set_error(serror, SCRIPT_ERR_CHECKMULTISIGVERIFY);
                    }
                } break;

                default:
                    return set_error(serror, SCRIPT_ERR_BAD_OPCODE);
                }

            if (stack.size() + altstack.size() > 1000)
                return set_error(serror, SCRIPT_ERR_STACK_SIZE);
        }
    }
    catch (...) {
        return set_error(serror, SCRIPT_ERR_UNKNOWN_ERROR);
    }

    if (!vfExec.empty())
        return set_error(serror, SCRIPT_ERR_UNBALANCED_CONDITIONAL);

    return set_success(serror);
}

namespace {

/**
 * Wrapper that serializes like CTransaction, but with the modifications
 *  required for the signature hash done in-place
 */
class CTransactionSignatureSerializer {
private:
    const CTransaction& txTo; //!< reference to the spending transaction (the one being serialized)
    const CScript& scriptCode; //!< output script being consumed
    const unsigned int nIn; //!< input index of txTo being signed
    const bool fAnyoneCanPay; //!< whether the hashtype has the SIGHASH_ANYONECANPAY flag set
    const bool fHashSingle; //!< whether the hashtype is SIGHASH_SINGLE
    const bool fHashNone; //!< whether the hashtype is SIGHASH_NONE

public:
    CTransactionSignatureSerializer(const CTransaction& txToIn, const CScript& scriptCodeIn, unsigned int nInIn, int nHashTypeIn)
        : txTo(txToIn)
        , scriptCode(scriptCodeIn)
        , nIn(nInIn)
        , fAnyoneCanPay(!!(nHashTypeIn & SIGHASH_ANYONECANPAY))
        , fHashSingle((nHashTypeIn & 0x1f) == SIGHASH_SINGLE)
        , fHashNone((nHashTypeIn & 0x1f) == SIGHASH_NONE)
    {
    }

    /** Serialize the passed scriptCode, skipping OP_CODESEPARATORs */
    template <typename S>
    void SerializeScriptCode(S& s, int nType, int nVersion) const
    {
        CScript::const_iterator it = scriptCode.begin();
        CScript::const_iterator itBegin = it;
        opcodetype opcode;
        unsigned int nCodeSeparators = 0;
        while (scriptCode.GetOp(it, opcode)) {
            if (opcode == OP_CODESEPARATOR)
                nCodeSeparators++;
        }
        ::WriteCompactSize(s, scriptCode.size() - nCodeSeparators);
        it = itBegin;
        while (scriptCode.GetOp(it, opcode)) {
            if (opcode == OP_CODESEPARATOR) {
                s.write((char*)&itBegin[0], it - itBegin - 1);
                itBegin = it;
            }
        }
        if (itBegin != scriptCode.end())
            s.write((char*)&itBegin[0], it - itBegin);
    }

    /** Serialize an input of txTo */
    template <typename S>
    void SerializeInput(S& s, unsigned int nInput, int nType, int nVersion) const
    {

        if (fAnyoneCanPay)
            nInput = nIn;

        ::Serialize(s, txTo.vin[nInput].prevout, nType, nVersion);

        if (nInput != nIn)

            ::Serialize(s, CScriptBase(), nType, nVersion);
        else
            SerializeScriptCode(s, nType, nVersion);

        if (nInput != nIn && (fHashSingle || fHashNone))

            ::Serialize(s, (int)0, nType, nVersion);
        else
            ::Serialize(s, txTo.vin[nInput].nSequence, nType, nVersion);
    }

    /** Serialize an output of txTo */
    template <typename S>
    void SerializeOutput(S& s, unsigned int nOutput, int nType, int nVersion) const
    {
        if (fHashSingle && nOutput != nIn)

            ::Serialize(s, CTxOut(), nType, nVersion);
        else
            ::Serialize(s, txTo.vout[nOutput], nType, nVersion);
    }

    /** Serialize txTo */
    template <typename S>
    void Serialize(S& s, int nType, int nVersion) const
    {

        ::Serialize(s, txTo.nVersion, nType, nVersion);

        unsigned int nInputs = fAnyoneCanPay ? 1 : txTo.vin.size();
        ::WriteCompactSize(s, nInputs);
        for (unsigned int nInput = 0; nInput < nInputs; nInput++)
            SerializeInput(s, nInput, nType, nVersion);

        unsigned int nOutputs = fHashNone ? 0 : (fHashSingle ? nIn + 1 : txTo.vout.size());
        ::WriteCompactSize(s, nOutputs);
        for (unsigned int nOutput = 0; nOutput < nOutputs; nOutput++)
            SerializeOutput(s, nOutput, nType, nVersion);

        ::Serialize(s, txTo.nLockTime, nType, nVersion);
    }
};

uint256 GetPrevoutHash(const CTransaction& txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (unsigned int n = 0; n < txTo.vin.size(); n++) {
        ss << txTo.vin[n].prevout;
    }
    return ss.GetHash();
}

uint256 GetSequenceHash(const CTransaction& txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (unsigned int n = 0; n < txTo.vin.size(); n++) {
        ss << txTo.vin[n].nSequence;
    }
    return ss.GetHash();
}

uint256 GetOutputsHash(const CTransaction& txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (unsigned int n = 0; n < txTo.vout.size(); n++) {
        ss << txTo.vout[n];
    }
    return ss.GetHash();
}

} // anon namespace

PrecomputedTransactionData::PrecomputedTransactionData(const CTransaction& txTo)
{
    hashPrevouts = GetPrevoutHash(txTo);
    hashSequence = GetSequenceHash(txTo);
    hashOutputs = GetOutputsHash(txTo);
}

uint256 SignatureHash(const CScript& scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType, const CAmount& amount, SigVersion sigversion, const PrecomputedTransactionData* cache)
{
    if (sigversion == SIGVERSION_WITNESS_V0) {
        uint256 hashPrevouts;
        uint256 hashSequence;
        uint256 hashOutputs;

        if (!(nHashType & SIGHASH_ANYONECANPAY)) {
            hashPrevouts = cache ? cache->hashPrevouts : GetPrevoutHash(txTo);
        }

        if (!(nHashType & SIGHASH_ANYONECANPAY) && (nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
            hashSequence = cache ? cache->hashSequence : GetSequenceHash(txTo);
        }

        if ((nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
            hashOutputs = cache ? cache->hashOutputs : GetOutputsHash(txTo);
        } else if ((nHashType & 0x1f) == SIGHASH_SINGLE && nIn < txTo.vout.size()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << txTo.vout[nIn];
            hashOutputs = ss.GetHash();
        }

        CHashWriter ss(SER_GETHASH, 0);

        ss << txTo.nVersion;

        ss << hashPrevouts;
        ss << hashSequence;

        ss << txTo.vin[nIn].prevout;
        ss << static_cast<const CScriptBase&>(scriptCode);
        ss << amount;
        ss << txTo.vin[nIn].nSequence;

        ss << hashOutputs;

        ss << txTo.nLockTime;

        ss << nHashType;

        return ss.GetHash();
    }

    static const uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    if (nIn >= txTo.vin.size()) {

        return one;
    }

    if ((nHashType & 0x1f) == SIGHASH_SINGLE) {
        if (nIn >= txTo.vout.size()) {

            return one;
        }
    }

    CTransactionSignatureSerializer txTmp(txTo, scriptCode, nIn, nHashType);

    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << nHashType;
    return ss.GetHash();
}

bool TransactionSignatureChecker::VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& pubkey, const uint256& sighash) const
{
    return pubkey.Verify(sighash, vchSig);
}

bool TransactionSignatureChecker::CheckSig(const vector<unsigned char>& vchSigIn, const vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const
{
    CPubKey pubkey(vchPubKey);
    if (!pubkey.IsValid())
        return false;

    vector<unsigned char> vchSig(vchSigIn);
    if (vchSig.empty())
        return false;
    int nHashType = vchSig.back();
    vchSig.pop_back();

    uint256 sighash = SignatureHash(scriptCode, *txTo, nIn, nHashType, amount, sigversion, this->txdata);

    if (!VerifySignature(vchSig, pubkey, sighash))
        return false;

    return true;
}

bool TransactionSignatureChecker::CheckLockTime(const CScriptNum& nLockTime) const
{

    if (!(
            (txTo->nLockTime < LOCKTIME_THRESHOLD && nLockTime < LOCKTIME_THRESHOLD) || (txTo->nLockTime >= LOCKTIME_THRESHOLD && nLockTime >= LOCKTIME_THRESHOLD)))
        return false;

    if (nLockTime > (int64_t)txTo->nLockTime)
        return false;

    if (CTxIn::SEQUENCE_FINAL == txTo->vin[nIn].nSequence)
        return false;

    return true;
}

bool TransactionSignatureChecker::CheckSequence(const CScriptNum& nSequence) const
{

    const int64_t txToSequence = (int64_t)txTo->vin[nIn].nSequence;

    if (static_cast<uint32_t>(txTo->nVersion) < 2)
        return false;

    if (txToSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG)
        return false;

    const uint32_t nLockTimeMask = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | CTxIn::SEQUENCE_LOCKTIME_MASK;
    const int64_t txToSequenceMasked = txToSequence & nLockTimeMask;
    const CScriptNum nSequenceMasked = nSequence & nLockTimeMask;

    if (!(
            (txToSequenceMasked < CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG && nSequenceMasked < CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) || (txToSequenceMasked >= CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG && nSequenceMasked >= CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG))) {
        return false;
    }

    if (nSequenceMasked > txToSequenceMasked)
        return false;

    return true;
}

static bool VerifyWitnessProgram(const CScriptWitness& witness, int witversion, const std::vector<unsigned char>& program, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror)
{
    vector<vector<unsigned char> > stack;
    CScript scriptPubKey;

    if (witversion == 0) {
        if (program.size() == 32) {

            if (witness.stack.size() == 0) {
                return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_WITNESS_EMPTY);
            }
            scriptPubKey = CScript(witness.stack.back().begin(), witness.stack.back().end());
            stack = std::vector<std::vector<unsigned char> >(witness.stack.begin(), witness.stack.end() - 1);
            uint256 hashScriptPubKey;
            CSHA256().Write(&scriptPubKey[0], scriptPubKey.size()).Finalize(hashScriptPubKey.begin());
            if (memcmp(hashScriptPubKey.begin(), &program[0], 32)) {
                return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
            }
        } else if (program.size() == 20) {

            if (witness.stack.size() != 2) {
                return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH); // 2 items in witness
            }
            scriptPubKey << OP_DUP << OP_HASH160 << program << OP_EQUALVERIFY << OP_CHECKSIG;
            stack = witness.stack;
        } else {
            return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH);
        }
    } else if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM) {
        return set_error(serror, SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM);
    } else {

        return set_success(serror);
    }

    for (unsigned int i = 0; i < stack.size(); i++) {
        if (stack.at(i).size() > MAX_SCRIPT_ELEMENT_SIZE)
            return set_error(serror, SCRIPT_ERR_PUSH_SIZE);
    }

    if (!EvalScript(stack, scriptPubKey, flags, checker, SIGVERSION_WITNESS_V0, serror)) {
        return false;
    }

    if (stack.size() != 1)
        return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
    if (!CastToBool(stack.back()))
        return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
    return true;
}

bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror)
{
    static const CScriptWitness emptyWitness;
    if (witness == NULL) {
        witness = &emptyWitness;
    }
    bool hadWitness = false;

    set_error(serror, SCRIPT_ERR_UNKNOWN_ERROR);

    if ((flags & SCRIPT_VERIFY_SIGPUSHONLY) != 0 && !scriptSig.IsPushOnly()) {
        return set_error(serror, SCRIPT_ERR_SIG_PUSHONLY);
    }

    vector<vector<unsigned char> > stack, stackCopy;
    if (!EvalScript(stack, scriptSig, flags, checker, SIGVERSION_BASE, serror))

        return false;
    if (flags & SCRIPT_VERIFY_P2SH)
        stackCopy = stack;
    if (!EvalScript(stack, scriptPubKey, flags, checker, SIGVERSION_BASE, serror))

        return false;
    if (stack.empty())
        return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
    if (CastToBool(stack.back()) == false)
        return set_error(serror, SCRIPT_ERR_EVAL_FALSE);

    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (flags & SCRIPT_VERIFY_WITNESS) {
        if (scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
            hadWitness = true;
            if (scriptSig.size() != 0) {

                return set_error(serror, SCRIPT_ERR_WITNESS_MALLEATED);
            }
            if (!VerifyWitnessProgram(*witness, witnessversion, witnessprogram, flags, checker, serror)) {
                return false;
            }

            stack.resize(1);
        }
    }

    if ((flags & SCRIPT_VERIFY_P2SH) && scriptPubKey.IsPayToScriptHash()) {

        if (!scriptSig.IsPushOnly())
            return set_error(serror, SCRIPT_ERR_SIG_PUSHONLY);

        swap(stack, stackCopy);

        assert(!stack.empty());

        const valtype& pubKeySerialized = stack.back();
        CScript pubKey2(pubKeySerialized.begin(), pubKeySerialized.end());
        popstack(stack);

        if (!EvalScript(stack, pubKey2, flags, checker, SIGVERSION_BASE, serror))

            return false;
        if (stack.empty())
            return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
        if (!CastToBool(stack.back()))
            return set_error(serror, SCRIPT_ERR_EVAL_FALSE);

        if (flags & SCRIPT_VERIFY_WITNESS) {
            if (pubKey2.IsWitnessProgram(witnessversion, witnessprogram)) {
                hadWitness = true;
                if (scriptSig != CScript() << std::vector<unsigned char>(pubKey2.begin(), pubKey2.end())) {

                    return set_error(serror, SCRIPT_ERR_WITNESS_MALLEATED_P2SH);
                }
                if (!VerifyWitnessProgram(*witness, witnessversion, witnessprogram, flags, checker, serror)) {
                    return false;
                }

                stack.resize(1);
            }
        }
    }

    if ((flags & SCRIPT_VERIFY_CLEANSTACK) != 0) {

        assert((flags & SCRIPT_VERIFY_P2SH) != 0);
        assert((flags & SCRIPT_VERIFY_WITNESS) != 0);
        if (stack.size() != 1) {
            return set_error(serror, SCRIPT_ERR_CLEANSTACK);
        }
    }

    if (flags & SCRIPT_VERIFY_WITNESS) {

        assert((flags & SCRIPT_VERIFY_P2SH) != 0);
        if (!hadWitness && !witness->IsNull()) {
            return set_error(serror, SCRIPT_ERR_WITNESS_UNEXPECTED);
        }
    }

    return set_success(serror);
}

size_t static WitnessSigOps(int witversion, const std::vector<unsigned char>& witprogram, const CScriptWitness& witness, int flags)
{
    if (witversion == 0) {
        if (witprogram.size() == 20)
            return 1;

        if (witprogram.size() == 32 && witness.stack.size() > 0) {
            CScript subscript(witness.stack.back().begin(), witness.stack.back().end());
            return subscript.GetSigOpCount(true);
        }
    }

    return 0;
}

size_t CountWitnessSigOps(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags)
{
    static const CScriptWitness witnessEmpty;

    if ((flags & SCRIPT_VERIFY_WITNESS) == 0) {
        return 0;
    }
    assert((flags & SCRIPT_VERIFY_P2SH) != 0);

    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
        return WitnessSigOps(witnessversion, witnessprogram, witness ? *witness : witnessEmpty, flags);
    }

    if (scriptPubKey.IsPayToScriptHash() && scriptSig.IsPushOnly()) {
        CScript::const_iterator pc = scriptSig.begin();
        vector<unsigned char> data;
        while (pc < scriptSig.end()) {
            opcodetype opcode;
            scriptSig.GetOp(pc, opcode, data);
        }
        CScript subscript(data.begin(), data.end());
        if (subscript.IsWitnessProgram(witnessversion, witnessprogram)) {
            return WitnessSigOps(witnessversion, witnessprogram, witness ? *witness : witnessEmpty, flags);
        }
    }

    return 0;
}
