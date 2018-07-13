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

#include "wallet/wallet.h"
#include "wallet/wallettx.h"

#include "validation/validation.h"
#include "net.h"
#include "scheduler.h"
#include "timedata.h"
#include "utilmoneystr.h"
#include "init.h"
#include <unity/appmanager.h>
#include <script/ismine.h>

isminetype CWallet::IsMine(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.getHash());
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
                return IsMine(prev.tx->vout[txin.prevout.n]);
        }
    }
    return ISMINE_NO;
}

isminetype CWallet::IsMine(const CTxOut& txout) const
{
    return ::IsMine(*this, txout);
}

bool CWallet::IsMine(const CTransaction& tx) const
{
    for(const CTxOut& txout : tx.vout)
        if (IsMine(txout)  && txout.nValue >= nMinimumInputValue)
            return true;
    return false;
}

bool CWallet::IsFromMe(const CTransaction& tx) const
{
    return (GetDebit(tx, ISMINE_ALL) > 0);
}

bool CWallet::IsAllFromMe(const CTransaction& tx, const isminefilter& filter) const
{
    LOCK(cs_wallet);

    for(const CTxIn& txin : tx.vin)
    {
        auto mi = mapWallet.find(txin.prevout.getHash());
        if (mi == mapWallet.end())
            return false; // any unknown inputs can't be from us

        const CWalletTx& prev = (*mi).second;

        if (txin.prevout.n >= prev.tx->vout.size())
            return false; // invalid input!

        if (!(IsMine(prev.tx->vout[txin.prevout.n]) & filter))
            return false;
    }
    return true;
}

int CWallet::GetTransactionScanProgressPercent()
{
    return nTransactionScanProgressPercent;
}

/**
 * Scan active chain for relevant transactions after importing keys. This should
 * be called whenever new keys are added to the wallet, with the oldest key
 * creation time.
 *
 * @return Earliest timestamp that could be successfully scanned from. Timestamp
 * returned will be higher than startTime if relevant blocks could not be read.
 */
int64_t CWallet::RescanFromTime(int64_t startTime, bool update)
{
    // Find starting block. May be null if nCreateTime is greater than the
    // highest blockchain timestamp, in which case there is nothing that needs
    // to be scanned.
    CBlockIndex* startBlock = nullptr;
    {
        LOCK(cs_main);
        startBlock = chainActive.FindEarliestAtLeast(startTime - TIMESTAMP_WINDOW);
        LogPrintf("%s: Rescanning last %i blocks\n", __func__, startBlock ? chainActive.Height() - startBlock->nHeight + 1 : 0);
    }

    if (startBlock)
    {
        const CBlockIndex* const failedBlock = ScanForWalletTransactions(startBlock, update);
        if (failedBlock)
        {
            return failedBlock->GetBlockTimeMax() + TIMESTAMP_WINDOW + 1;
        }
    }
    return startTime;
}

/**
 * Scan the block chain (starting in pindexStart) for transactions
 * from or to us. If fUpdate is true, found transactions that already
 * exist in the wallet will be updated.
 *
 * Returns null if scan was successful. Otherwise, if a complete rescan was not
 * possible (due to pruning or corruption), returns pointer to the most recent
 * block that could not be scanned.
 */
CBlockIndex* CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
    int64_t nNow = GetTime();
    const CChainParams& chainParams = Params();

    CBlockIndex* pindex = pindexStart;
    CBlockIndex* ret = nullptr;
    {
        LOCK2(cs_main, cs_wallet); // Required for ReadBlockFromDisk.
        fAbortRescan = false;
        fScanningWallet = true;

        // no need to read and scan block, if block was created before
        // our wallet birthday (as adjusted for block time variability)
        // NB! nTimeFirstKey > TIMESTAMP_WINDOW check is important otherwise we overflow nTimeFirstKey
        while (pindex && (nTimeFirstKey > TIMESTAMP_WINDOW) && (pindex->GetBlockTime() < (int64_t)(nTimeFirstKey - TIMESTAMP_WINDOW)))
            pindex = chainActive.Next(pindex);

        nTransactionScanProgressPercent = 0;
        ShowProgress(_("Rescanning..."), nTransactionScanProgressPercent); // show rescan progress in GUI, if -rescan on startup
        if (!pindex)
        {
            LogPrintf("Nothing to do for rescan, chain empty.\n");
            return ret;
        }
        LogPrintf("Rescanning...\n");
        uint64_t nProgressStart = pindex->nHeight;
        uint64_t nProgressTip = chainActive.Tip()->nHeight;
        uint64_t nWorkQuantity = nProgressTip - nProgressStart;
        while (pindex && !fAbortRescan && nWorkQuantity > 0)
        {
            // Temporarily release lock to allow shadow key allocation a chance to do it's thing
            LEAVE_CRITICAL_SECTION(cs_main)
            LEAVE_CRITICAL_SECTION(cs_wallet)
            nTransactionScanProgressPercent = ((pindex->nHeight-nProgressStart) / (nWorkQuantity)) * 100;
            nTransactionScanProgressPercent = std::max(1, std::min(99, nTransactionScanProgressPercent));
            if (pindex->nHeight % 100 == 0 && nProgressTip - nProgressStart > 0.0)
            {
                ShowProgress(_("Rescanning..."), nTransactionScanProgressPercent);
            }
            if (GetTime() >= nNow + 60) {
                nNow = GetTime();
                LogPrintf("Still rescanning. At block %d. Progress=%d%%\n", pindex->nHeight, nTransactionScanProgressPercent);
            }
            ENTER_CRITICAL_SECTION(cs_main)
            ENTER_CRITICAL_SECTION(cs_wallet)

            if (ShutdownRequested())
                return ret;

            CBlock block;
            if (ReadBlockFromDisk(block, pindex, Params())) {
                for (size_t posInBlock = 0; posInBlock < block.vtx.size(); ++posInBlock) {
                    AddToWalletIfInvolvingMe(block.vtx[posInBlock], pindex, posInBlock, fUpdate);
                }
            } else {
                ret = pindex;
            }
            pindex = chainActive.Next(pindex);
        }
        if (pindex && fAbortRescan) {
            LogPrintf("Rescan aborted at block %d. Progress=%f\n", pindex->nHeight, GuessVerificationProgress(chainParams.TxData(), pindex));
        }
        nTransactionScanProgressPercent = 100;
        ShowProgress(_("Rescanning..."), nTransactionScanProgressPercent); // hide progress dialog in GUI

        fScanningWallet = false;
    }
    LogPrintf("Rescan done.\n");
    return ret;
}
