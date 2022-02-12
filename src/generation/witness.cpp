// Copyright (c) 2015-2020 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "witness.h"
#include "generation.h"
#include "miner.h"

#include "net.h"

#include "alert.h"
#include "appname.h"
#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "consensus/tx_verify.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "key.h"
#include "validation/validation.h"
#include "validation/witnessvalidation.h"
#include "policy/feerate.h"
#include "policy/policy.h"
#include "pow/pow.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "utiltime.h"
#include "utilmoneystr.h"
#include "validation/validationinterface.h"
#include "init.h"

#include <algorithm>
#include <numeric>
#include <queue>
#include <utility>

#include <crypto/hash/hash.h>
#include <witnessutil.h>
#include <openssl/sha.h>

#include <boost/thread.hpp>

#include "txdb.h"

//Gulden includes
#include "streams.h"

RecursiveMutex processBlockCS;

#ifdef ENABLE_WALLET
CReserveKeyOrScript::CReserveKeyOrScript(CWallet* pwalletIn, CAccount* forAccount, int64_t forKeyChain)
{
    pwallet = pwalletIn;
    account = forAccount;
    nIndex = -1;
    nKeyChain = forKeyChain;
}

CReserveKeyOrScript::CReserveKeyOrScript(CScript& script)
{
    pwallet = nullptr;
    account = nullptr;
    nIndex = -1;
    nKeyChain = -1;
    reserveScript = script;
}

CReserveKeyOrScript::CReserveKeyOrScript(CPubKey &pubkey)
{
    pwallet = nullptr;
    account = nullptr;
    // By setting index as -1 we ensure key is not returned.
    nIndex = -1;
    nKeyChain = -1;
    vchPubKey = pubkey;
}

CReserveKeyOrScript::CReserveKeyOrScript(CKeyID &pubKeyID_)
{
    pwallet = nullptr;
    account = nullptr;
    // By setting index as -1 we ensure key is not returned.
    nIndex = -1;
    nKeyChain = -1;
    pubKeyID = pubKeyID_;
}

CReserveKeyOrScript::~CReserveKeyOrScript()
{
    if (shouldKeepOnDestroy)
        KeepScript();
    if (!scriptOnly())
        ReturnKey();
}

bool CReserveKeyOrScript::scriptOnly()
{
    if (account == nullptr && pwallet == nullptr && pubKeyID.IsNull())
        return true;
    return false;
}

void CReserveKeyOrScript::KeepScript()
{
    if (!scriptOnly())
        KeepKey();
}

void CReserveKeyOrScript::keepScriptOnDestroy()
{
    shouldKeepOnDestroy = true;
}

static bool SignBlockAsWitness(std::shared_ptr<CBlock> pBlock, CTxOut fittestWitnessOutput, CAccount* signingAccount)
{
    assert(pBlock->nVersionPoW2Witness != 0);

    CKeyID witnessKeyID;


    if (fittestWitnessOutput.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        witnessKeyID = fittestWitnessOutput.output.witnessDetails.witnessKeyID;
    }
    else
    {
        assert(0);
    }

    CKey key;
    if (!signingAccount->GetKey(witnessKeyID, key))
    {
        std::string strErrorMessage = strprintf("Failed to obtain key to sign as witness: chain-tip-height[%d]", chainActive.Tip()? chainActive.Tip()->nHeight : 0);
        CAlert::Notify(strErrorMessage, true, true);
        LogPrintf("%s\n", strErrorMessage.c_str());
        return false;
    }

    // Do not allow uncompressed keys.
    if (!key.IsCompressed())
    {
        std::string strErrorMessage = strprintf("Invalid witness key - uncompressed keys not allowed: chain-tip-height[%d]", chainActive.Tip()? chainActive.Tip()->nHeight : 0);
        CAlert::Notify(strErrorMessage, true, true);
        LogPrintf("%s\n", strErrorMessage.c_str());
        return false;
    }

    //Sign the hash of the block as proof that it has been witnessed.
    uint256 hash = pBlock->GetHashPoW2();
    if (!key.SignCompact(hash, pBlock->witnessHeaderPoW2Sig))
        return false;

    return true;
}

static bool alert(const std::string& msg)
{
    CAlert::Notify(msg, true, true);
    LogPrintf("%s", msg.c_str());
    return false;
}

static bool CreateWitnessSubsidyOutputs(CTxOutPoW2Witness& witnessInput, CMutableTransaction& coinbaseTx, const CWitnessRewardTemplate& rewardTemplate, std::shared_ptr<CReserveKeyOrScript> coinbaseReservedKey, const CAmount witnessBlockSubsidy, const CAmount witnessFeeSubsidy, CTxOut& selectedWitnessOutput, COutPoint& selectedWitnessOutPoint, bool bSegSigIsEnabled, unsigned int nSelectedWitnessBlockHeight)
{
    // Ammend some details of the input that must change in the new output.
    CPoW2WitnessDestination witnessDestination;
    witnessDestination.spendingKey = witnessInput.spendingKeyID;
    witnessDestination.witnessKey = witnessInput.witnessKeyID;
    witnessDestination.lockFromBlock = witnessInput.lockFromBlock;
    witnessDestination.lockUntilBlock = witnessInput.lockUntilBlock;
    witnessDestination.failCount = witnessInput.failCount;
    witnessDestination.actionNonce = witnessInput.actionNonce+1;

    // If this is the first time witnessing the lockFromBlock won't yet be filled in so fill it in now.
    if (witnessDestination.lockFromBlock == 0)
        witnessDestination.lockFromBlock = nSelectedWitnessBlockHeight;

    // If fail count is non-zero then we are allowed to decrement it by one every time we witness.
    if (witnessDestination.failCount > 0)
        witnessDestination.failCount = witnessDestination.failCount - 1;

    CAmount compoundWitnessBlockSubsidy = witnessBlockSubsidy + witnessFeeSubsidy;

    CAmount fixedTotal = rewardTemplate.fixedAmountsSum();
    if (fixedTotal > compoundWitnessBlockSubsidy)
        return alert(strprintf("%s, Witness template fixed amounts total (%s) exceed subsidy (%s)", __PRETTY_FUNCTION__, FormatMoney(fixedTotal), FormatMoney(compoundWitnessBlockSubsidy)));

    CAmount percentageSum = rewardTemplate.percentagesSum();
    if (percentageSum < 0.0 || percentageSum > 1.0)
        return alert(strprintf("%s, Witness template percentage total (%f) out of range [0..100]", __PRETTY_FUNCTION__, percentageSum * 100.0));

    CAmount flexibleTotal = compoundWitnessBlockSubsidy - fixedTotal;

    CAmount percentageTotalAmount = std::accumulate(rewardTemplate.destinations.begin(), rewardTemplate.destinations.end(), CAmount(0), [&](const CAmount& acc, const CWitnessRewardDestination& dest){
        return acc + dest.percent * flexibleTotal;
    });

    if (percentageTotalAmount > flexibleTotal)
        return alert(strprintf("%s, Witness template percentages rounding error, specifiy less percentages and use remainder", __PRETTY_FUNCTION__));

    // Calculate remainder
    bool remainderDone = false;
    CAmount remainderAmount = flexibleTotal - percentageTotalAmount;

    // Calculate compound
    CAmount compoundAmount = std::accumulate(rewardTemplate.destinations.begin(), rewardTemplate.destinations.end(), CAmount(0), [&](const CAmount& acc, const CWitnessRewardDestination& dest){
        return acc + (dest.type == CWitnessRewardDestination::DestType::Compound ? dest.amount + dest.percent * flexibleTotal : 0);
    });

    // Calculate overflow (adjusting compound)
    const CAmount effectiveCompoundLimit  = bSegSigIsEnabled ? gMaximumWitnessCompoundAmount * COIN : 0;
    bool overflowDone = false;
    CAmount overflowAmount = 0;
    if (compoundAmount > effectiveCompoundLimit) {
        overflowAmount = compoundAmount - effectiveCompoundLimit;
        compoundAmount = effectiveCompoundLimit;
    }

    // Special case for compound overflow while no overflow destination defined
    if (overflowAmount > 0 && 0 == std::count_if(rewardTemplate.destinations.begin(), rewardTemplate.destinations.end(),
                                                 [](const CWitnessRewardDestination& dest) { return dest.takesCompoundOverflow; })) {
        // must have remainder, but not on a compound destination
        bool hasRemainder = false;
        for (const CWitnessRewardDestination& dest: rewardTemplate.destinations) {
            hasRemainder = hasRemainder || dest.takesRemainder;
            if (dest.takesRemainder && dest.type == CWitnessRewardDestination::DestType::Compound)
                return alert(strprintf("%s, Witness template could not output overflow", __PRETTY_FUNCTION__));
        }
        if (!hasRemainder)
            return alert(strprintf("%s, Witness template needed remainder to output overflow", __PRETTY_FUNCTION__));

        // Move overflow to remainder
        remainderAmount += overflowAmount;
        overflowDone = true;
    }

    // Create the witness output
    coinbaseTx.vout.resize(1);
    if (bSegSigIsEnabled)
    {
        coinbaseTx.vout[0].SetType(CTxOutType::PoW2WitnessOutput);
        coinbaseTx.vout[0].output.witnessDetails.spendingKeyID = witnessDestination.spendingKey;
        coinbaseTx.vout[0].output.witnessDetails.witnessKeyID = witnessDestination.witnessKey;
        coinbaseTx.vout[0].output.witnessDetails.lockFromBlock = witnessDestination.lockFromBlock;
        coinbaseTx.vout[0].output.witnessDetails.lockUntilBlock = witnessDestination.lockUntilBlock;
        coinbaseTx.vout[0].output.witnessDetails.failCount = witnessDestination.failCount;
        coinbaseTx.vout[0].output.witnessDetails.actionNonce = witnessDestination.actionNonce;
    }
    else
    {
        coinbaseTx.vout[0].SetType(CTxOutType::ScriptLegacyOutput);
        coinbaseTx.vout[0].output.scriptPubKey = GetScriptForDestination(witnessDestination);
    }
    coinbaseTx.vout[0].nValue = selectedWitnessOutput.nValue + compoundAmount;

    for (const CWitnessRewardDestination& dest: rewardTemplate.destinations) {
        // Calculate amount for this dest. Important: exact same calculation method here as in the above accumulation
        CAmount amount = dest.amount + dest.percent * flexibleTotal;
        if (dest.takesRemainder && !remainderDone) {
            amount += remainderAmount;
            remainderDone = true;
        }
        if (dest.takesCompoundOverflow && !overflowDone) {
            amount += overflowAmount;
            overflowDone = true;
        }

        // Don't create output for 0 amounts (for example a compound_overflow where there is no overflow)
        if (amount == 0)
            continue;

        switch (dest.type) {
        case CWitnessRewardDestination::DestType::Account:
        {
            CTxOut txOut;
            txOut.nValue = amount;
            if (bSegSigIsEnabled && !coinbaseReservedKey->scriptOnly())
            {
                txOut.SetType(CTxOutType::StandardKeyHashOutput);
                CPubKey addressPubKey;
                if (!coinbaseReservedKey->GetReservedKey(addressPubKey))
                    return alert(strprintf("%s, failed to get reserved key with which to sign as witness: chain-tip-height[%d]", __PRETTY_FUNCTION__, chainActive.Tip()? chainActive.Tip()->nHeight : 0));
                txOut.output.standardKeyHash = CTxOutStandardKeyHash(addressPubKey.GetID());
            }
            else
            {
                txOut.SetType(CTxOutType::ScriptLegacyOutput);
                txOut.output.scriptPubKey = coinbaseReservedKey->reserveScript;
            }
            coinbaseTx.vout.push_back(txOut);
            break;
        }
        case CWitnessRewardDestination::DestType::Address:
            coinbaseTx.vout.push_back(CTxOut(amount, GetScriptForDestination(dest.address.Get())));
            break;
        default: // DestType::Compound is handled in witness output
            break;
        }
    }

    if (remainderAmount > 0 && !remainderDone)
        return alert(strprintf("%s, Witness template could not output remainder", __PRETTY_FUNCTION__));

    if (overflowAmount > 0 && !overflowDone)
        return alert(strprintf("%s, Witness template could not output remainder", __PRETTY_FUNCTION__));

    return true;
}

static std::tuple<bool, CMutableTransaction, CWitnessBundles> CreateWitnessCoinbase(int nWitnessHeight, CBlockIndex* pIndexPrev, int nPoW2PhaseParent, std::shared_ptr<CReserveKeyOrScript> coinbaseScript, const CAmount witnessBlockSubsidy, const CAmount witnessFeeSubsidy, CTxOut& selectedWitnessOutput, COutPoint& selectedWitnessOutPoint, unsigned int nSelectedWitnessBlockHeight, CAccount* selectedWitnessAccount, uint64_t nTransactionIndex)
{
    // We will calculate and return a witness bundle for this transaction so the caller can append it to the non mutable transaction
    CWitnessBundles coinbaseWitnessBundles;
    
    // Obtain the details of the signing witness transaction which must be consumed as an input and recreated as an output.
    CTxOutPoW2Witness witnessInput; GetPow2WitnessOutput(selectedWitnessOutput, witnessInput);

    bool bSegSigIsEnabled = IsSegSigEnabled(pIndexPrev);

    CMutableTransaction coinbaseTx(bSegSigIsEnabled ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::CURRENT_VERSION);
    coinbaseTx.vin.resize(2);
    coinbaseTx.vin[0].SetPrevOutNull();
    coinbaseTx.vin[0].SetSequence(0, coinbaseTx.nVersion, CTxInFlags::None);
    coinbaseTx.vin[1].SetPrevOut(selectedWitnessOutPoint);
    coinbaseTx.vin[1].SetSequence(0, coinbaseTx.nVersion, CTxInFlags::None);

    if (bSegSigIsEnabled)
    {
        std::string coinbaseSignature = GetArg("-coinbasesignature", "");
        coinbaseTx.vin[0].segregatedSignatureData.stack.clear();
        coinbaseTx.vin[0].segregatedSignatureData.stack.push_back(std::vector<unsigned char>());
        CVectorWriter(0, 0, coinbaseTx.vin[0].segregatedSignatureData.stack[0], 0) << VARINT(nWitnessHeight);
        coinbaseTx.vin[0].segregatedSignatureData.stack.push_back(std::vector<unsigned char>(coinbaseSignature.begin(), coinbaseSignature.end()));
    }
    else
    {
        // Phase 3 - we restrict the coinbase signature to only the block height.
        // This helps simplify the logic for the PoW mining (which has to stuff all this info into it's own coinbase signature).
        coinbaseTx.vin[0].scriptSig = CScript() << nWitnessHeight;
    }

    // Sign witness coinbase.
    {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        if (!pactiveWallet->SignTransaction(selectedWitnessAccount, coinbaseTx, SignType::Witness, &selectedWitnessOutput))
        {
            std::string strErrorMessage = strprintf("Failed to sign witness coinbase: height[%d] chain-tip-height[%d]", nWitnessHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0);
            CAlert::Notify(strErrorMessage, true, true);
            LogPrintf("%s\n", strErrorMessage.c_str());
            return std::tuple(false, coinbaseTx, coinbaseWitnessBundles);
        }
    }

    CWitnessRewardTemplate rewardTemplate;
    // If an explicit template has been set then use that, otherwise create a default template
    if (selectedWitnessAccount->hasRewardTemplate())
    {
        LOCK(pactiveWallet->cs_wallet);
        rewardTemplate = selectedWitnessAccount->getRewardTemplate();
    }
    else
    {
        if (selectedWitnessAccount->getCompounding() > 0)
        {
            auto compoundAmount = selectedWitnessAccount->getCompounding();
            if (compoundAmount == MAX_MONEY)
            {
                compoundAmount = witnessBlockSubsidy;
                // Subsidy and any overflow fees to compound
                rewardTemplate.destinations.push_back(CWitnessRewardDestination(CWitnessRewardDestination::DestType::Compound, CNativeAddress(), compoundAmount, 0.0, true, false));
                // Any compound overflow to script
                rewardTemplate.destinations.push_back(CWitnessRewardDestination(CWitnessRewardDestination::DestType::Account, CNativeAddress(), 0, 0.0, false, true));
            }
            else
            {
                // Pay up until requested amount to compound
                rewardTemplate.destinations.push_back(CWitnessRewardDestination(CWitnessRewardDestination::DestType::Compound, CNativeAddress(), compoundAmount, 0.0, false, false));
                // Any remaining fees/overflow to script
                rewardTemplate.destinations.push_back(CWitnessRewardDestination(CWitnessRewardDestination::DestType::Account, CNativeAddress(), 0, 0.0, true, true));
            }
        }
        else
        {
            // Compound nothing, all money into 'reward script'
            rewardTemplate.destinations.push_back(CWitnessRewardDestination(CWitnessRewardDestination::DestType::Compound, CNativeAddress(), 0, 0.0, false, false));
            rewardTemplate.destinations.push_back(CWitnessRewardDestination(CWitnessRewardDestination::DestType::Account, CNativeAddress(), 0, 0.0, true, true));
        }
    }

    // Output for subsidy and refresh witness address.
    if (!CreateWitnessSubsidyOutputs(witnessInput, coinbaseTx, rewardTemplate, coinbaseScript, witnessBlockSubsidy, witnessFeeSubsidy, selectedWitnessOutput, selectedWitnessOutPoint, bSegSigIsEnabled, nSelectedWitnessBlockHeight))
    {
        // Error message already handled inside function
        return std::tuple(false, coinbaseTx, coinbaseWitnessBundles);
    }
    
    CWitnessTxBundle bundle;
    if (!selectedWitnessOutPoint.isHash)
    {
        bundle.bundleType = CWitnessTxBundle::WitnessTxType::WitnessType;
        bundle.outputs.push_back(std::tuple(coinbaseTx.vout[0], coinbaseTx.vout[0].output.witnessDetails, COutPoint(nWitnessHeight, nTransactionIndex, 0)));
        bundle.inputs.push_back(std::tuple(selectedWitnessOutput, std::move(witnessInput), COutPoint(nSelectedWitnessBlockHeight, selectedWitnessOutPoint.getTransactionIndex(), selectedWitnessOutPoint.n)));
        coinbaseWitnessBundles.push_back(bundle);
    }
    return std::tuple(true, coinbaseTx, coinbaseWitnessBundles);
}


void TryPopulateAndSignWitnessBlock(CBlockIndex* candidateIter, CChainParams& chainparams, Consensus::Params& consensusParams, CGetWitnessInfo witnessInfo, std::shared_ptr<CBlock> pWitnessBlock, std::map<boost::uuids::uuid, std::shared_ptr<CReserveKeyOrScript>>& reserveKeys, bool& encounteredError, bool& signedBlock)
{
    signedBlock = false;
    encounteredError = false;

    CAmount witnessBlockSubsidy = GetBlockSubsidy(candidateIter->nHeight).witness;
    CAmount witnessFeesSubsidy = 0;

    bool isMineAny = (pactiveWallet->IsMineWitness(witnessInfo.selectedWitnessTransaction) == ISMINE_WITNESS);
    bool isMineTestnetGenesis = false;
    CAccount* selectedWitnessAccount = nullptr;
    std::shared_ptr<CAccount> deleteAccount = nullptr;
    if (isMineAny)
    {
        selectedWitnessAccount = pactiveWallet->FindBestWitnessAccountForTransaction(witnessInfo.selectedWitnessTransaction);
    }
    else if (chainparams.genesisWitnessPrivKey.IsValid())
    {
        if (witnessInfo.selectedWitnessTransaction.output.witnessDetails.witnessKeyID == chainparams.genesisWitnessPrivKey.GetPubKey().GetID())
        {
            isMineTestnetGenesis = true;
            selectedWitnessAccount = new CAccount();
            selectedWitnessAccount->m_Type = AccountType::ImportedPrivateKeyAccount;
            selectedWitnessAccount->AddKeyPubKey(chainparams.genesisWitnessPrivKey, chainparams.genesisWitnessPrivKey.GetPubKey(), KEYCHAIN_WITNESS);
            deleteAccount = std::shared_ptr<CAccount>(selectedWitnessAccount);
        }
    }   
    if (selectedWitnessAccount)
    {
        //We must do this before we add the blank coinbase otherwise GetBlockWeight crashes on a NULL pointer dereference.
        int nStartingBlockWeight = GetBlockWeight(*pWitnessBlock);

        /** First we add the new witness coinbase to the block, this acts as a seperator between transactions from the initial mined block and the witness block **/
        /** We add a placeholder for now as we don't know the fees we will generate **/
        pWitnessBlock->vtx.emplace_back();
        unsigned int nWitnessCoinbaseIndex = pWitnessBlock->vtx.size()-1;
        nStartingBlockWeight += 200;

        std::shared_ptr<CReserveKeyOrScript> coinbaseScript = nullptr;
        if (!isMineTestnetGenesis)
        {
            if (witnessScriptsAreDirty)
            {
                reserveKeys.clear();
            }
            auto findIter = reserveKeys.find(selectedWitnessAccount->getUUID());
            if (findIter != reserveKeys.end())
            {
                coinbaseScript = findIter->second;
            }
            else
            {
                GetMainSignals().ScriptForWitnessing(coinbaseScript, selectedWitnessAccount);
                // Don't attempt to witness if we have nowhere to pay the rewards.
                // ScriptForWitnessing will have alerted the user.
                if (coinbaseScript == nullptr)
                {
                    std::string strErrorMessage = strprintf("Failed to create payout for witness block [%d] current chain tip [%d].\n", candidateIter->nHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0);
                    CAlert::Notify(strErrorMessage, true, true);
                    LogPrintf("GuldenWitness: [Error] %s\n", strErrorMessage.c_str());
                    encounteredError=true;
                    return;
                }
                reserveKeys[selectedWitnessAccount->getUUID()] = coinbaseScript;
            }
        }
        else
        {
            CScript script;
            coinbaseScript = std::make_shared<CReserveKeyOrScript>(script);
        }

        int nPoW2PhaseParent = GetPoW2Phase(candidateIter->pprev);
        
        /** Now add any additional transactions if there is space left **/
        //fixme: (FUT): In an attempt to work around a potential rare issue that causes chain stalls we temporarily avoid adding transactions if witnessing a non tip node
        //In future we should add the transactions back again
        if (nPoW2PhaseParent >= 4 && candidateIter == chainActive.Tip())
        {
            // Piggy back off existing block assembler code to grab the transactions we want to include.
            // Setup maximum size for assembler so that size of existing (PoW) block transactions are subtracted from overall maximum.
            BlockAssembler::Options assemblerOptions;
            assemblerOptions.nBlockMaxWeight = GetArg("-blockmaxweight", DEFAULT_BLOCK_MAX_WEIGHT) - nStartingBlockWeight;
            assemblerOptions.nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE) - nStartingBlockWeight;

            std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params(), assemblerOptions).CreateNewBlock(candidateIter, coinbaseScript, true, nullptr, true));
            if (!pblocktemplate.get())
            {
                std::string strErrorMessage = strprintf("Failed to get block template [%d] current chain tip [%d].\n", candidateIter->nHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0);
                CAlert::Notify(strErrorMessage, true, true);
                LogPrintf("GuldenWitness: [Error] %s\n", strErrorMessage.c_str());
                encounteredError=true;
                return;
            }

            // Skip the coinbase as we obviously don't want this included again, it is already in the PoW part of the block.
            size_t nSkipCoinbase = 1;
            //fixme: (FUT) (CBSU)? pre-allocate for vtx.size().
            for (size_t i=nSkipCoinbase; i < pblocktemplate->block.vtx.size(); i++)
            {
                bool bSkip = false;
                //fixme: (PHASE5) Check why we were getting duplicates - something to do with mempool not being updated for latest block or something?
                // Exclude any duplicates that somehow creep in.
                for(size_t j=0; j < nWitnessCoinbaseIndex; j++)
                {
                    if (pWitnessBlock->vtx[j]->GetHash() == pblocktemplate->block.vtx[i]->GetHash())
                    {
                        bSkip = true;
                        break;
                    }
                }
                
                //Forbid any transactions that would affect the witness set, as these would throw simplified witness utxo tracking off
                //fixme: (WITNESS_SYNC); Ensure witnesses are still adding transactions here
                //fixme: (WITNESS_SYNC); We should rather do this inside the block assembler
                if (!bSkip)
                {
                    const auto& tx = pblocktemplate->block.vtx[i];
                    if (!tx->witnessBundles || tx->witnessBundles->size()>0)
                    {
                        bSkip = true;
                    }
                }
                if (!bSkip)
                {
                    //fixme: (FUT) emplace_back for better performace?
                    pWitnessBlock->vtx.push_back(pblocktemplate->block.vtx[i]);
                    witnessFeesSubsidy += (pblocktemplate->vTxFees[i]);
                }
            }
        }

        /** Populate witness coinbase placeholder with real information now that we have it **/
        const auto& [result, coinbaseTx, witnessBundles] = CreateWitnessCoinbase(candidateIter->nHeight, candidateIter->pprev, nPoW2PhaseParent, coinbaseScript, witnessBlockSubsidy, witnessFeesSubsidy, witnessInfo.selectedWitnessTransaction, witnessInfo.selectedWitnessOutpoint, witnessInfo.selectedWitnessBlockHeight, selectedWitnessAccount, nWitnessCoinbaseIndex);
        if (result)
        {
            pWitnessBlock->vtx[nWitnessCoinbaseIndex] = MakeTransactionRef(std::move(coinbaseTx));
            pWitnessBlock->vtx[nWitnessCoinbaseIndex]->witnessBundles = std::make_shared<CWitnessBundles>(witnessBundles);

            /** Set witness specific block header information **/
            {
                // ComputeBlockVersion returns the right version flag to signal for phase 4 activation here, assuming we are already in phase 3 and 95 percent of peers are upgraded.
                pWitnessBlock->nVersionPoW2Witness = ComputeBlockVersion(candidateIter->pprev, consensusParams);

                // Second witness timestamp gets added to the block as an additional time source to the miner timestamp.
                // Witness timestamp must exceed median of previous mined timestamps.
                pWitnessBlock->nTimePoW2Witness = std::max(candidateIter->GetMedianTimePastPoW()+1, GetAdjustedTime());

                // Set witness merkle hash.
                pWitnessBlock->hashMerkleRootPoW2Witness = BlockMerkleRoot(pWitnessBlock->vtx.begin()+nWitnessCoinbaseIndex, pWitnessBlock->vtx.end());
                
                // Set the simplified witness UTXO change delta
                if ((uint64_t)candidateIter->nHeight  > consensusParams.pow2WitnessSyncHeight)
                {
                    std::shared_ptr<SimplifiedWitnessUTXOSet> pow2SimplifiedWitnessUTXOForPrevBlock = std::make_shared<SimplifiedWitnessUTXOSet>();
                    if (!GetSimplifiedWitnessUTXOSetForIndex(candidateIter->pprev, *pow2SimplifiedWitnessUTXOForPrevBlock))
                    {
                        std::string strErrorMessage = strprintf("Failed to compute UTXO for prev block [%d] current chain tip [%d].\n", candidateIter->nHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0);
                        CAlert::Notify(strErrorMessage, true, true);
                        LogPrintf("GuldenWitness: [Error] %s\n", strErrorMessage.c_str());
                        encounteredError=true;
                        return;
                    }
                    if (!GetSimplifiedWitnessUTXODeltaForBlock(candidateIter, *pWitnessBlock, pow2SimplifiedWitnessUTXOForPrevBlock, pWitnessBlock->witnessUTXODelta, nullptr))
                    {
                        std::string strErrorMessage = strprintf("Failed to compute UTXO delta for block [%d] current chain tip [%d].\n", candidateIter->nHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0);
                        CAlert::Notify(strErrorMessage, true, true);
                        LogPrintf("GuldenWitness: [Error] %s\n", strErrorMessage.c_str());
                        encounteredError=true;
                        return;
                    }
                }
            }

            /** Do the witness operation (Sign the block using our witness key) and broadcast the final product to the network. **/
            if (SignBlockAsWitness(pWitnessBlock, witnessInfo.selectedWitnessTransaction, selectedWitnessAccount))
            {
                ProcessBlockFound(pWitnessBlock, chainparams);
                coinbaseScript->keepScriptOnDestroy();
                signedBlock=true;
                return;
            }
            else
            {
                std::string strErrorMessage = strprintf("Signature error, failed to witness block [%d] current chain tip [%d].\n", candidateIter->nHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0);
                CAlert::Notify(strErrorMessage, true, true);
                LogPrintf("GuldenWitness: [Error] %s\n", strErrorMessage.c_str());
                encounteredError=true;
                return;
            }
        }
        else
        {
            std::string strErrorMessage = strprintf("Coinbase error, failed to create coinbase for witness block [%d] current chain tip [%d] address [%s].\n", candidateIter->nHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0, CNativeAddress(CPoW2WitnessDestination(witnessInfo.selectedWitnessTransaction.output.witnessDetails.spendingKeyID, witnessInfo.selectedWitnessTransaction.output.witnessDetails.witnessKeyID)).ToString());
            CAlert::Notify(strErrorMessage, true, true);
            LogPrintf("GuldenWitness: [Error] %s\n", strErrorMessage.c_str());
            encounteredError=true;
            return;
        }
    }
}

struct CBlockIndexCacheComparator
{
    bool operator()(CBlockIndex *pa, CBlockIndex *pb) const {
        if (pa->nHeight > pb->nHeight) return false;
        if (pa->nHeight < pb->nHeight) return true;

        if (pa < pb) return false;
        if (pa > pb) return true;

        return false;
    }
};
//fixme: (POST-PHASE5) We should also check for already signed block coming from ourselves (from e.g. a different machine - think witness devices for instance) - Don't sign it if we already have a signed copy of the block lurking around...
std::set<CBlockIndex*, CBlockIndexCacheComparator> cacheAlreadySeenWitnessCandidates;

bool witnessScriptsAreDirty = false;
bool witnessingEnabled = true;

void static GuldenWitness()
{
    LogPrintf("Witness thread started\n");
    RenameThread(GLOBAL_APPNAME"-witness");
    
    // Don't even try witness if we have no wallet (-disablewallet)
    if (!pactiveWallet)
        return;

    static bool regTest = Params().IsRegtest();
    static bool regTestLegacy = Params().IsRegtestLegacy();
    // Don't use witness loop on regtest
    if (regTest || regTestLegacy)
        return;
    
    static bool testNet = Params().IsTestnet();

    CChainParams chainparams = Params();
    try
    {
        std::map<boost::uuids::uuid, std::shared_ptr<CReserveKeyOrScript>> reserveKeys;
        while (true)
        {
            if (!testNet)
            {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                do
                {
                    if (pactiveWallet && g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) > 0)
                    {
                        LOCK(cs_main);
                        if(!IsInitialBlockDownload())
                            break;
                    }
                    MilliSleep(5000);
                } while (true);
            }
            while (!witnessingEnabled)
            {
                MilliSleep(200);
            }
            DO_BENCHMARK("WIT: GuldenWitness", BCLog::BENCH|BCLog::WITNESS);

            CBlockIndex* pindexTip = nullptr;
            {
                LOCK(cs_main);
                pindexTip = chainActive.Tip();
            }
            Consensus::Params consensusParams = chainparams.GetConsensus();

            //We can only start witnessing from phase 3 onward.
            if (!pindexTip || !pindexTip->pprev || !IsPow2WitnessingActive(pindexTip->nHeight))
            {
                MilliSleep(5000);
                continue;
            }
            int nPoW2PhasePrev = GetPoW2Phase(pindexTip->pprev);

            //fixme: (POST-PHASE5)
            //Ideally instead of just sleeping/busy polling rather wait on a signal that gets triggered only when new blocks come in??
            MilliSleep(100);

            // Check for stop or if block needs to be rebuilt
            boost::this_thread::interruption_point();

            static uint256 hashLastAbsentWitnessTip;
            static uint64_t timeLastAbsentWitnessTip = 0;
            static uint64_t secondsLastAbsentWitnessTip = 0;

            // If we already have a witnessed block at the tip don't bother looking at any orphans, just patiently wait for next unsigned tip.
            if (nPoW2PhasePrev < 3 || pindexTip->nVersionPoW2Witness != 0)
            {
                timeLastAbsentWitnessTip = 0;
                secondsLastAbsentWitnessTip = 0;
                hashLastAbsentWitnessTip.SetNull();
                continue;
            }

            // Log absent witness if witness logging enabled.
            if (LogAcceptCategory(BCLog::WITNESS) || IsArgSet("-zmqpubstalledwitness"))
            {
                if (hashLastAbsentWitnessTip == pindexTip->GetBlockHashPoW2())
                {
                    uint64_t nSecondsAbsent = (GetTimeMillis() - timeLastAbsentWitnessTip) / 1000;
                    if (((nSecondsAbsent % 5) == 0) && nSecondsAbsent != secondsLastAbsentWitnessTip)
                    {
                        secondsLastAbsentWitnessTip = nSecondsAbsent;
                        GetMainSignals().StalledWitness(pindexTip, nSecondsAbsent);
                        LogPrint(BCLog::WITNESS, "GuldenWitness: absent witness at tip [%s] [%d] %d seconds\n", hashLastAbsentWitnessTip.ToString(), pindexTip->nHeight, nSecondsAbsent);
                    }
                }
                else
                {
                    hashLastAbsentWitnessTip = pindexTip->GetBlockHashPoW2();
                    timeLastAbsentWitnessTip = GetTimeMillis();
                    secondsLastAbsentWitnessTip = 0;
                }
            }

            // Use a cache to prevent trying the same blocks over and over.
            // Look for all potential signable blocks at same height as the index tip - don't limit ourselves to just the tip
            // This is important because otherwise the chain can stall if there is an absent signer for the current tip.
            std::vector<CBlockIndex*> candidateOrphans;
            if (cacheAlreadySeenWitnessCandidates.find(pindexTip) == cacheAlreadySeenWitnessCandidates.end())
            {
                LogPrint(BCLog::WITNESS, "GuldenWitness: Add witness candidate from chain tip [%s]\n", pindexTip->GetBlockHashPoW2().ToString());
                candidateOrphans.push_back(pindexTip);
            }
            if (candidateOrphans.size() == 0)
            {
                for (const auto candidateIter : GetTopLevelPoWOrphans(pindexTip->nHeight, *(pindexTip->pprev->phashBlock)))
                {
                    if (cacheAlreadySeenWitnessCandidates.find(candidateIter) == cacheAlreadySeenWitnessCandidates.end())
                    {
                        LogPrint(BCLog::WITNESS, "GuldenWitness: Add witness candidate from top level pow orphans [%s]\n", candidateIter->GetBlockHashPoW2().ToString());
                        candidateOrphans.push_back(candidateIter);
                    }
                }
                if (cacheAlreadySeenWitnessCandidates.size() > 100000)
                {
                    auto eraseEnd = cacheAlreadySeenWitnessCandidates.begin();
                    std::advance(eraseEnd, cacheAlreadySeenWitnessCandidates.size() - 10);
                    cacheAlreadySeenWitnessCandidates.erase(cacheAlreadySeenWitnessCandidates.begin(), eraseEnd);
                }
            }
            boost::this_thread::interruption_point();

            if (candidateOrphans.size() > 0)
            {
                LOCK2(processBlockCS,cs_main); //cs_main lock for ReadBlockFromDisk and chainActive.Tip()
                
                if (chainActive.Tip() != pindexTip)
                {
                    continue;
                }

                
                for (const auto candidateIter : candidateOrphans)
                {
                    boost::this_thread::interruption_point();

                    cacheAlreadySeenWitnessCandidates.insert(candidateIter);

                    //Create new block
                    std::shared_ptr<CBlock> pWitnessBlock(new CBlock);
                    if (ReadBlockFromDisk(*pWitnessBlock, candidateIter, chainparams))
                    {
                        boost::this_thread::interruption_point();
                        CGetWitnessInfo witnessInfo;

                        if (!GetWitness(chainActive, chainparams, nullptr, candidateIter->pprev, *pWitnessBlock, witnessInfo))
                        {
                            LogPrintf("GuldenWitness: Invalid candidate witness [%s]\n", candidateIter->GetBlockHashPoW2().ToString());
                            static int64_t nLastErrorHeight = -1;

                            if (nLastErrorHeight == -1 || candidateIter->nHeight - nLastErrorHeight > 10)
                            {
                                nLastErrorHeight = candidateIter->nHeight;
                                continue;
                            }
                            nLastErrorHeight = candidateIter->nHeight;
                            std::string strErrorMessage = strprintf("Failed to calculate witness info for candidate block.\n If this occurs frequently please contact a developer for assistance.\n height [%d] chain-tip-height [%d]", candidateIter->nHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0);
                            CAlert::Notify(strErrorMessage, true, true);
                            LogPrintf("%s\n", strErrorMessage.c_str());
                            continue;
                        }

                        // Create all the witness inputs/outputs and additional metadata, add any additional transactions, sign block etc.
                        boost::this_thread::interruption_point();
                        bool encounteredError=false;
                        bool signedBlock=false;
                        TryPopulateAndSignWitnessBlock(candidateIter, chainparams, consensusParams, witnessInfo, pWitnessBlock, reserveKeys, encounteredError, signedBlock);
                        
                        //fixme: (HIGH) If we signed the block consider terminating the loop and purging all other candidates of same height at this point.
                    }
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        cacheAlreadySeenWitnessCandidates.clear();
        LogPrintf("Witness thread terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        cacheAlreadySeenWitnessCandidates.clear();
        LogPrintf("Witness thread runtime error: %s\n", e.what());
        return;
    }
}

#endif


void StartPoW2WitnessThread(boost::thread_group& threadGroup)
{
    #ifdef ENABLE_WALLET
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "pow2_witness", &GuldenWitness));
    #endif
}
