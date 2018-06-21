// Copyright (c) 2015-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "witness.h"
#include "generation.h"
#include "miner.h"

#include "net.h"

#include "alert.h"
#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "consensus/tx_verify.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "Gulden/auto_checkpoints.h"
#include "hash.h"
#include "validation/validation.h"
#include "validation/witnessvalidation.h"
#include "net.h"
#include "policy/feerate.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "utiltime.h"
#include "utilmoneystr.h"
#include "validation/validationinterface.h"

#include <algorithm>
#include <queue>
#include <utility>

#include <Gulden/Common/diff.h>
#include <Gulden/Common/hash/hash.h>
#include <Gulden/util.h>
#include <openssl/sha.h>

#include <boost/thread.hpp>

#include "txdb.h"

//Gulden includes
#include "streams.h"

CCriticalSection processBlockCS;

static bool SignBlockAsWitness(std::shared_ptr<CBlock> pBlock, CTxOut fittestWitnessOutput)
{
    assert(pBlock->nVersionPoW2Witness != 0);

    CKeyID witnessKeyID;


    if (fittestWitnessOutput.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        witnessKeyID = fittestWitnessOutput.output.witnessDetails.witnessKeyID;
    }
    else if ( (fittestWitnessOutput.GetType() <= CTxOutType::ScriptLegacyOutput && fittestWitnessOutput.output.scriptPubKey.IsPoW2Witness()) ) //fixme: (2.1) We can remove this.
    {
        std::vector<unsigned char> hashWitnessBytes = fittestWitnessOutput.output.scriptPubKey.GetPow2WitnessHash();
        witnessKeyID = CKeyID(uint160(hashWitnessBytes));
    }
    else
    {
        assert(0);
    }

    CKey key;
    if (!pactiveWallet->GetKey(witnessKeyID, key))
    {
        std::string strErrorMessage = strprintf("Failed to obtain key to sign as witness: height[%d] chain-tip-height[%d]", nWitnessHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0);
        CAlert::Notify(strErrorMessage, true, true);
        LogPrintf(strErrorMessage);
        return false;
    }

    // Do not allow uncompressed keys.
    if (!key.IsCompressed())
    {
        std::string strErrorMessage = strprintf("Invalid witness key - uncompressed keys not allowed: height[%d] chain-tip-height[%d]", nWitnessHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0);
        CAlert::Notify(strErrorMessage, true, true);
        LogPrintf(strErrorMessage);
        return false;
    }

    //fixme: (2.0) - Anything else to serialise here? Add the block height maybe? probably overkill.
    uint256 hash = pBlock->GetHashPoW2();
    if (!key.SignCompact(hash, pBlock->witnessHeaderPoW2Sig))
        return false;

    //LogPrintf(">>>[WitFound] witness pubkey [%s]\n", key.GetPubKey().GetID().GetHex());

    //fixme: (2.0) (RELEASE) - Remove this, it is here for testing purposes only.
    if (fittestWitnessOutput.GetType() == CTxOutType::PoW2WitnessOutput)
    {
        if (fittestWitnessOutput.output.witnessDetails.witnessKeyID != key.GetPubKey().GetID())
        {
            std::string strErrorMessage = strprintf("Fatal witness error - segsig key mismatch: height[%d] chain-tip-height[%d]", nWitnessHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0);
            CAlert::Notify(strErrorMessage, true, true);
            LogPrintf(strErrorMessage);
            return false;
        }
    }
    else
    {
        if (CKeyID(uint160(fittestWitnessOutput.output.scriptPubKey.GetPow2WitnessHash())) != key.GetPubKey().GetID())
        {
            std::string strErrorMessage = strprintf("Fatal witness error - legacy key mismatch: height[%d] chain-tip-height[%d]", nWitnessHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0);
            CAlert::Notify(strErrorMessage, true, true);
            LogPrintf(strErrorMessage);
            return false;
        }
    }

    return true;
}


static bool CreateWitnessSubsidyOutputs(CMutableTransaction& coinbaseTx, std::shared_ptr<CReserveKeyOrScript> coinbaseReservedKey, const CAmount witnessBlockSubsidy, const CAmount witnessFeeSubsidy, CTxOut& selectedWitnessOutput, COutPoint& selectedWitnessOutPoint, CAmount compoundTargetAmount, bool bSegSigIsEnabled, unsigned int nSelectedWitnessBlockHeight)
{
    // Forbid compound earnings for phase 3, as we can't handle this in a backwards compatible way.
    if (!bSegSigIsEnabled)
        compoundTargetAmount = 0;

    // First obtain the details of the signing witness transaction which must be consumed as an input and recreated as an output.
    CTxOutPoW2Witness witnessInput; GetPow2WitnessOutput(selectedWitnessOutput, witnessInput);

    // Now ammend some details of the input that must change in the new output.
    CPoW2WitnessDestination witnessDestination;
    witnessDestination.spendingKey = witnessInput.spendingKeyID;
    witnessDestination.witnessKey = witnessInput.witnessKeyID;
    witnessDestination.lockFromBlock = witnessInput.lockFromBlock;
    witnessDestination.lockUntilBlock = witnessInput.lockUntilBlock;
    witnessDestination.failCount = witnessInput.failCount;

    // If this is the first time witnessing the lockFromBlock won't yet be filled in so fill it in now.
    if (witnessDestination.lockFromBlock == 0)
        witnessDestination.lockFromBlock = nSelectedWitnessBlockHeight;

    // If fail count is non-zero then we are allowed to decrement it by one every time we witness.
    if (witnessDestination.failCount > 0)
        witnessDestination.failCount = witnessDestination.failCount - 1;

    // If we are compounding then we need 2 outputs.
    // If we are compounding then we want only 1 output.
    // However if we are compounding and there are more fees than compouding rules allow; then the overflow needs to go in a second output so we need 2 outputs again.
    CAmount noncompoundWitnessBlockSubsidy = 0;
    CAmount compoundWitnessBlockSubsidy = witnessBlockSubsidy + witnessFeeSubsidy;
    if (compoundTargetAmount != 0)
    {
        // Compound target is negative
        // Pay the first '-compoundTargetAmount' of reward to non-compound output and compound the remainder.
        if (compoundTargetAmount < 0)
        {
            noncompoundWitnessBlockSubsidy = -compoundTargetAmount;
            if (noncompoundWitnessBlockSubsidy > compoundWitnessBlockSubsidy)
            {
                noncompoundWitnessBlockSubsidy = compoundWitnessBlockSubsidy;
                compoundWitnessBlockSubsidy = 0;
            }
            else
            {
                compoundWitnessBlockSubsidy -= noncompoundWitnessBlockSubsidy;
            }
        }
        // Compound target is positive
        // Compound the first '-compoundTargetAmount' of reward and pay the remainder to non-compound.
        else if (compoundWitnessBlockSubsidy > compoundTargetAmount)
        {
            noncompoundWitnessBlockSubsidy = compoundWitnessBlockSubsidy - compoundTargetAmount;
            compoundWitnessBlockSubsidy = compoundTargetAmount;
        }

        // If we are compounding more than the network allows then reduce accordingly.
        if (compoundWitnessBlockSubsidy > witnessBlockSubsidy*2)
        {
            noncompoundWitnessBlockSubsidy += compoundWitnessBlockSubsidy - witnessBlockSubsidy*2;
            compoundWitnessBlockSubsidy = witnessBlockSubsidy*2;
        }

        //fixme: (DUST)
        //Rudimentary dust prevention - in future possibly improve this.
        if ((noncompoundWitnessBlockSubsidy > 0) && (noncompoundWitnessBlockSubsidy < 1 * COIN / 10))
        {
            noncompoundWitnessBlockSubsidy += 1 * COIN / 10;
            compoundWitnessBlockSubsidy -= 1 * COIN / 10;
        }
    }
    else
    {
        noncompoundWitnessBlockSubsidy = compoundWitnessBlockSubsidy;
        compoundWitnessBlockSubsidy = 0;
    }
    coinbaseTx.vout.resize((noncompoundWitnessBlockSubsidy == 0) ? 1 : 2);

    // Finally create the output(s).
    if (bSegSigIsEnabled)
    {
        coinbaseTx.vout[0].SetType(CTxOutType::PoW2WitnessOutput);
        coinbaseTx.vout[0].output.witnessDetails.spendingKeyID = witnessDestination.spendingKey;
        coinbaseTx.vout[0].output.witnessDetails.witnessKeyID = witnessDestination.witnessKey;
        coinbaseTx.vout[0].output.witnessDetails.lockFromBlock = witnessDestination.lockFromBlock;
        coinbaseTx.vout[0].output.witnessDetails.lockUntilBlock = witnessDestination.lockUntilBlock;
        coinbaseTx.vout[0].output.witnessDetails.failCount = witnessDestination.failCount;
    }
    else
    {
        coinbaseTx.vout[0].SetType(CTxOutType::ScriptLegacyOutput);
        coinbaseTx.vout[0].output.scriptPubKey = GetScriptForDestination(witnessDestination);
    }
    coinbaseTx.vout[0].nValue = selectedWitnessOutput.nValue + compoundWitnessBlockSubsidy;

    if (noncompoundWitnessBlockSubsidy > 0)
    {
        if (bSegSigIsEnabled && !coinbaseReservedKey->scriptOnly())
        {
            coinbaseTx.vout[1].SetType(CTxOutType::StandardKeyHashOutput);
            CPubKey addressPubKey;
            if (!coinbaseReservedKey->GetReservedKey(addressPubKey))
            {
                CAlert::Notify(strprintf("CreateWitnessSubsidyOutputs, failed to get reserved key with which to sign as witness: height[%d] chain-tip-height[%d]", nWitnessHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0), true, true);
                LogPrintf("CreateWitnessSubsidyOutputs, failed to get reserved key with which to sign as witness");
                return false;
            }
            coinbaseTx.vout[1].output.standardKeyHash = CTxOutStandardKeyHash(addressPubKey.GetID());
        }
        else
        {
            coinbaseTx.vout[1].SetType(CTxOutType::ScriptLegacyOutput);
            coinbaseTx.vout[1].output.scriptPubKey = coinbaseReservedKey->reserveScript;
        }
        coinbaseTx.vout[1].nValue = noncompoundWitnessBlockSubsidy;
    }

    return true;
}

static std::pair<bool, CMutableTransaction> CreateWitnessCoinbase(int nWitnessHeight, CBlockIndex* pIndexPrev, int nPoW2PhaseParent, std::shared_ptr<CReserveKeyOrScript> coinbaseScript, const CAmount witnessBlockSubsidy, const CAmount witnessFeeSubsidy, CTxOut& selectedWitnessOutput, COutPoint& selectedWitnessOutPoint, unsigned int nSelectedWitnessBlockHeight, CAccount* selectedWitnessAccount)
{
    bool bSegSigIsEnabled = IsSegSigEnabled(pIndexPrev);

    CMutableTransaction coinbaseTx(bSegSigIsEnabled ? CTransaction::SEGSIG_ACTIVATION_VERSION : CTransaction::CURRENT_VERSION);
    coinbaseTx.vin.resize(2);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vin[0].SetSequence(0, coinbaseTx.nVersion, CTxInFlags::None);
    coinbaseTx.vin[1].prevout = selectedWitnessOutPoint;
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
        LOCK(pactiveWallet->cs_wallet);
        if (!pactiveWallet->SignTransaction(selectedWitnessAccount, coinbaseTx, Witness))
        {
            CAlert::Notify(strprintf("Failed to sign witness coinbase: height[%d] chain-tip-height[%d]", nWitnessHeight, chainActive.Tip()? chainActive.Tip()->nHeight : 0), true, true);
            return std::pair(false, coinbaseTx);
        }
    }

    // Output for subsidy and refresh witness address.
    if (!CreateWitnessSubsidyOutputs(coinbaseTx, coinbaseScript, witnessBlockSubsidy, witnessFeeSubsidy, selectedWitnessOutput, selectedWitnessOutPoint, selectedWitnessAccount->getCompounding(), bSegSigIsEnabled, nSelectedWitnessBlockHeight))
    {
        // Error message already handled inside function
        return std::pair(false, coinbaseTx);
    }

    return std::pair(true, coinbaseTx);
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
//fixme: (2.1) We should also check for already signed block coming from ourselves (from e.g. a different machine - think witness devices for instance) - Don't sign it if we already have a signed copy of the block lurking around...
std::set<CBlockIndex*, CBlockIndexCacheComparator> cacheAlreadySeenWitnessCandidates;

bool witnessScriptsAreDirty = false;

void static GuldenWitness()
{
    LogPrintf("GuldenWitness started\n");
    RenameThread("gulden-witness");

    static bool hashCity = IsArgSet("-testnet") ? ( GetArg("-testnet", "")[0] == 'C' ? true : false ) : false;
    static bool regTest = GetBoolArg("-regtest", false);

    CChainParams chainparams = Params();
    try
    {
        std::map<boost::uuids::uuid, std::shared_ptr<CReserveKeyOrScript>> reserveKeys;
        while (true)
        {
            if (!regTest)
            {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                do
                {
                    if (hashCity || g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) > 0)
                    {
                        if(!IsInitialBlockDownload())
                            break;
                    }
                    MilliSleep(5000);
                } while (true);
            }
            DO_BENCHMARK("WIT: GuldenWitness", BCLog::BENCH|BCLog::WITNESS);

            CBlockIndex* pindexTip = chainActive.Tip();
            Consensus::Params pParams = chainparams.GetConsensus();

            //We can only start witnessing from phase 3 onward.
            if ( !pindexTip || !pindexTip->pprev || !IsPow2WitnessingActive(pindexTip, chainparams, chainActive)  )
            {
                MilliSleep(5000);
                continue;
            }
            int nPoW2PhasePrev = GetPoW2Phase(pindexTip->pprev, chainparams, chainActive);

            //fixme: (2.0) Shorter sleep here?
            //Or ideally instead of just sleeping/busy polling rather wait on a signal that gets triggered only when new blocks come in??
            MilliSleep(100);

            // Check for stop or if block needs to be rebuilt
            boost::this_thread::interruption_point();

            // If we already have a witnessed block at the tip don't bother looking at any orphans, just patiently wait for next unsigned tip.
            if (nPoW2PhasePrev < 3 || pindexTip->nVersionPoW2Witness != 0)
                continue;

            // Use a cache to prevent trying the same blocks over and over.
            // Look for all potential signable blocks at same height as the index tip - don't limit ourselves to just the tip
            // This is important because otherwise the chain can stall if there is an absent signer for the current tip.
            std::vector<CBlockIndex*> candidateOrphans;
            if (cacheAlreadySeenWitnessCandidates.find(pindexTip) == cacheAlreadySeenWitnessCandidates.end())
            {
                candidateOrphans.push_back(pindexTip);
            }
            if (candidateOrphans.size() == 0)
            {
                for (const auto candidateIter : GetTopLevelPoWOrphans(pindexTip->nHeight, *(pindexTip->pprev->phashBlock)))
                {
                    if (cacheAlreadySeenWitnessCandidates.find(candidateIter) == cacheAlreadySeenWitnessCandidates.end())
                    {
                        candidateOrphans.push_back(candidateIter);
                    }
                }
                if (cacheAlreadySeenWitnessCandidates.size() > 50)
                {
                    auto eraseEnd = cacheAlreadySeenWitnessCandidates.begin();
                    std::advance(eraseEnd, cacheAlreadySeenWitnessCandidates.size() - 50);
                    cacheAlreadySeenWitnessCandidates.erase(cacheAlreadySeenWitnessCandidates.begin(), eraseEnd);
                }
            }
            boost::this_thread::interruption_point();

            if (candidateOrphans.size() > 0)
            {
                LOCK(processBlockCS);

                if (chainActive.Tip() != pindexTip)
                {
                    continue;
                }

                LOCK(cs_main); // For ReadBlockFromDisk
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

                        //fixme: (2.0) Error handling
                        if (!GetWitness(chainActive, chainparams, nullptr, candidateIter->pprev, *pWitnessBlock, witnessInfo))
                            continue;

                        boost::this_thread::interruption_point();
                        CAmount witnessBlockSubsidy = GetBlockSubsidyWitness(candidateIter->nHeight, pParams);
                        CAmount witnessFeesSubsidy = 0;

                        //fixme: (2.0) (POW2) (ISMINE_WITNESS)
                        if (pactiveWallet->IsMine(witnessInfo.selectedWitnessTransaction) == ISMINE_SPENDABLE)
                        {
                            CAccount* selectedWitnessAccount = pactiveWallet->FindAccountForTransaction(witnessInfo.selectedWitnessTransaction);
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
                                    reserveKeys[selectedWitnessAccount->getUUID()] = coinbaseScript;
                                }

                                int nPoW2PhaseParent = GetPoW2Phase(candidateIter->pprev, chainparams, chainActive);

                                /** Now add any additional transactions if there is space left **/
                                if (nPoW2PhaseParent >= 4)
                                {
                                    // Piggy back off existing block assembler code to grab the transactions we want to include.
                                    // Setup maximum size for assembler so that size of existing (PoW) block transactions are subtracted from overall maximum.
                                    BlockAssembler::Options assemblerOptions;
                                    assemblerOptions.nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT - nStartingBlockWeight;
                                    assemblerOptions.nBlockMaxSize = assemblerOptions.nBlockMaxWeight;

                                    std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params(), assemblerOptions).CreateNewBlock(candidateIter, coinbaseScript, true, nullptr, true));
                                    if (!pblocktemplate.get())
                                    {
                                        LogPrintf("Error in GuldenWitness: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                                        continue;
                                    }

                                    // Skip the coinbase as we obviously don't want this included again, it is already in the PoW part of the block.
                                    size_t nSkipCoinbase = 1;
                                    //fixme: (2.1) (CBSU)? pre-allocate for vtx.size().
                                    for (size_t i=nSkipCoinbase; i < pblocktemplate->block.vtx.size(); i++)
                                    {
                                        bool bSkip = false;
                                        // fixme: (2.1) Check why we were getting duplicates - something to do with mempool not being updated for latest block or something?
                                        // Exclude any duplicates that somehow creep in.
                                        for(size_t j=0; j < nWitnessCoinbaseIndex; j++)
                                        {
                                            if (pWitnessBlock->vtx[j]->GetHash() == pblocktemplate->block.vtx[i]->GetHash())
                                            {
                                                bSkip = true;
                                                break;
                                            }
                                        }
                                        if (!bSkip)
                                        {
                                            //fixme: (2.1) emplace_back for better performace?
                                            pWitnessBlock->vtx.push_back(pblocktemplate->block.vtx[i]);
                                            witnessFeesSubsidy += (pblocktemplate->vTxFees[i]);
                                        }
                                    }
                                }


                                /** Populate witness coinbase placeholder with real information now that we have it **/
                                const auto& [result, coinbaseTx] = CreateWitnessCoinbase(candidateIter->nHeight, candidateIter->pprev, nPoW2PhaseParent, coinbaseScript, witnessBlockSubsidy, witnessFeesSubsidy, witnessInfo.selectedWitnessTransaction, witnessInfo.selectedWitnessOutpoint, witnessInfo.selectedWitnessBlockHeight, selectedWitnessAccount);
                                if (result)
                                {
                                    pWitnessBlock->vtx[nWitnessCoinbaseIndex] = MakeTransactionRef(std::move(coinbaseTx));


                                    /** Set witness specific block header information **/
                                    //testme: (GULDEN) (POW2) (2.0)
                                    {
                                        // ComputeBlockVersion returns the right version flag to signal for phase 4 activation here, assuming we are already in phase 3 and 95 percent of peers are upgraded.
                                        pWitnessBlock->nVersionPoW2Witness = ComputeBlockVersion(candidateIter->pprev, pParams);

                                        // Second witness timestamp gets added to the block as an additional time source to the miner timestamp.
                                        // Witness timestamp must exceed median of previous mined timestamps.
                                        pWitnessBlock->nTimePoW2Witness = std::max(candidateIter->GetMedianTimePastPoW()+1, GetAdjustedTime());

                                        // Set witness merkle hash.
                                        pWitnessBlock->hashMerkleRootPoW2Witness = BlockMerkleRoot(pWitnessBlock->vtx.begin()+nWitnessCoinbaseIndex, pWitnessBlock->vtx.end());
                                    }


                                    /** Do the witness operation (Sign the block using our witness key) and broadcast the final product to the network. **/
                                    if (SignBlockAsWitness(pWitnessBlock, witnessInfo.selectedWitnessTransaction))
                                    {
                                        LogPrint(BCLog::WITNESS, "GuldenWitness: witness found %s", pWitnessBlock->GetHashPoW2().ToString());
                                        ProcessBlockFound(pWitnessBlock, chainparams);
                                        coinbaseScript->keepScriptOnDestroy();
                                        continue;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        cacheAlreadySeenWitnessCandidates.clear();
        LogPrintf("GuldenWitness terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        cacheAlreadySeenWitnessCandidates.clear();
        LogPrintf("GuldenWitness runtime error: %s\n", e.what());
        return;
    }
}


void StartPoW2WitnessThread(boost::thread_group& threadGroup)
{
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "pow2_witness", &GuldenWitness));
}
