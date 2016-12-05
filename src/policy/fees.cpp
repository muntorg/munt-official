// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "policy/fees.h"
#include "policy/policy.h"

#include "amount.h"
#include "primitives/transaction.h"
#include "random.h"
#include "streams.h"
#include "txmempool.h"
#include "util.h"

void TxConfirmStats::Initialize(std::vector<double>& defaultBuckets,
                                unsigned int maxConfirms, double _decay, std::string _dataTypeString)
{
    decay = _decay;
    dataTypeString = _dataTypeString;
    for (unsigned int i = 0; i < defaultBuckets.size(); i++) {
        buckets.push_back(defaultBuckets[i]);
        bucketMap[defaultBuckets[i]] = i;
    }
    confAvg.resize(maxConfirms);
    curBlockConf.resize(maxConfirms);
    unconfTxs.resize(maxConfirms);
    for (unsigned int i = 0; i < maxConfirms; i++) {
        confAvg[i].resize(buckets.size());
        curBlockConf[i].resize(buckets.size());
        unconfTxs[i].resize(buckets.size());
    }

    oldUnconfTxs.resize(buckets.size());
    curBlockTxCt.resize(buckets.size());
    txCtAvg.resize(buckets.size());
    curBlockVal.resize(buckets.size());
    avg.resize(buckets.size());
}

void TxConfirmStats::ClearCurrent(unsigned int nBlockHeight)
{
    for (unsigned int j = 0; j < buckets.size(); j++) {
        oldUnconfTxs[j] += unconfTxs[nBlockHeight % unconfTxs.size()][j];
        unconfTxs[nBlockHeight % unconfTxs.size()][j] = 0;
        for (unsigned int i = 0; i < curBlockConf.size(); i++)
            curBlockConf[i][j] = 0;
        curBlockTxCt[j] = 0;
        curBlockVal[j] = 0;
    }
}

void TxConfirmStats::Record(int blocksToConfirm, double val)
{

    if (blocksToConfirm < 1)
        return;
    unsigned int bucketindex = bucketMap.lower_bound(val)->second;
    for (size_t i = blocksToConfirm; i <= curBlockConf.size(); i++) {
        curBlockConf[i - 1][bucketindex]++;
    }
    curBlockTxCt[bucketindex]++;
    curBlockVal[bucketindex] += val;
}

void TxConfirmStats::UpdateMovingAverages()
{
    for (unsigned int j = 0; j < buckets.size(); j++) {
        for (unsigned int i = 0; i < confAvg.size(); i++)
            confAvg[i][j] = confAvg[i][j] * decay + curBlockConf[i][j];
        avg[j] = avg[j] * decay + curBlockVal[j];
        txCtAvg[j] = txCtAvg[j] * decay + curBlockTxCt[j];
    }
}

double TxConfirmStats::EstimateMedianVal(int confTarget, double sufficientTxVal,
                                         double successBreakPoint, bool requireGreater,
                                         unsigned int nBlockHeight)
{

    double nConf = 0; // Number of tx's confirmed within the confTarget
    double totalNum = 0; // Total number of tx's that were ever confirmed
    int extraNum = 0; // Number of tx's still in mempool for confTarget or longer

    int maxbucketindex = buckets.size() - 1;

    unsigned int startbucket = requireGreater ? maxbucketindex : 0;
    int step = requireGreater ? -1 : 1;

    unsigned int curNearBucket = startbucket;
    unsigned int bestNearBucket = startbucket;
    unsigned int curFarBucket = startbucket;
    unsigned int bestFarBucket = startbucket;

    bool foundAnswer = false;
    unsigned int bins = unconfTxs.size();

    for (int bucket = startbucket; bucket >= 0 && bucket <= maxbucketindex; bucket += step) {
        curFarBucket = bucket;
        nConf += confAvg[confTarget - 1][bucket];
        totalNum += txCtAvg[bucket];
        for (unsigned int confct = confTarget; confct < GetMaxConfirms(); confct++)
            extraNum += unconfTxs[(nBlockHeight - confct) % bins][bucket];
        extraNum += oldUnconfTxs[bucket];

        if (totalNum >= sufficientTxVal / (1 - decay)) {
            double curPct = nConf / (totalNum + extraNum);

            if (requireGreater && curPct < successBreakPoint)
                break;
            if (!requireGreater && curPct > successBreakPoint)
                break;

            else {
                foundAnswer = true;
                nConf = 0;
                totalNum = 0;
                extraNum = 0;
                bestNearBucket = curNearBucket;
                bestFarBucket = curFarBucket;
                curNearBucket = bucket + step;
            }
        }
    }

    double median = -1;
    double txSum = 0;

    unsigned int minBucket = bestNearBucket < bestFarBucket ? bestNearBucket : bestFarBucket;
    unsigned int maxBucket = bestNearBucket > bestFarBucket ? bestNearBucket : bestFarBucket;
    for (unsigned int j = minBucket; j <= maxBucket; j++) {
        txSum += txCtAvg[j];
    }
    if (foundAnswer && txSum != 0) {
        txSum = txSum / 2;
        for (unsigned int j = minBucket; j <= maxBucket; j++) {
            if (txCtAvg[j] < txSum)
                txSum -= txCtAvg[j];
            else { // we're in the right bucket
                median = avg[j] / txCtAvg[j];
                break;
            }
        }
    }

    LogPrint("estimatefee", "%3d: For conf success %s %4.2f need %s %s: %12.5g from buckets %8g - %8g  Cur Bucket stats %6.2f%%  %8.1f/(%.1f+%d mempool)\n",
             confTarget, requireGreater ? ">" : "<", successBreakPoint, dataTypeString,
             requireGreater ? ">" : "<", median, buckets[minBucket], buckets[maxBucket],
             100 * nConf / (totalNum + extraNum), nConf, totalNum, extraNum);

    return median;
}

void TxConfirmStats::Write(CAutoFile& fileout)
{
    fileout << decay;
    fileout << buckets;
    fileout << avg;
    fileout << txCtAvg;
    fileout << confAvg;
}

void TxConfirmStats::Read(CAutoFile& filein)
{

    std::vector<double> fileBuckets;
    std::vector<double> fileAvg;
    std::vector<std::vector<double> > fileConfAvg;
    std::vector<double> fileTxCtAvg;
    double fileDecay;
    size_t maxConfirms;
    size_t numBuckets;

    filein >> fileDecay;
    if (fileDecay <= 0 || fileDecay >= 1)
        throw std::runtime_error("Corrupt estimates file. Decay must be between 0 and 1 (non-inclusive)");
    filein >> fileBuckets;
    numBuckets = fileBuckets.size();
    if (numBuckets <= 1 || numBuckets > 1000)
        throw std::runtime_error("Corrupt estimates file. Must have between 2 and 1000 fee/pri buckets");
    filein >> fileAvg;
    if (fileAvg.size() != numBuckets)
        throw std::runtime_error("Corrupt estimates file. Mismatch in fee/pri average bucket count");
    filein >> fileTxCtAvg;
    if (fileTxCtAvg.size() != numBuckets)
        throw std::runtime_error("Corrupt estimates file. Mismatch in tx count bucket count");
    filein >> fileConfAvg;
    maxConfirms = fileConfAvg.size();
    if (maxConfirms <= 0 || maxConfirms > 6 * 24 * 7) // one week
        throw std::runtime_error("Corrupt estimates file.  Must maintain estimates for between 1 and 1008 (one week) confirms");
    for (unsigned int i = 0; i < maxConfirms; i++) {
        if (fileConfAvg[i].size() != numBuckets)
            throw std::runtime_error("Corrupt estimates file. Mismatch in fee/pri conf average bucket count");
    }

    decay = fileDecay;
    buckets = fileBuckets;
    avg = fileAvg;
    confAvg = fileConfAvg;
    txCtAvg = fileTxCtAvg;
    bucketMap.clear();

    curBlockConf.resize(maxConfirms);
    for (unsigned int i = 0; i < maxConfirms; i++) {
        curBlockConf[i].resize(buckets.size());
    }
    curBlockTxCt.resize(buckets.size());
    curBlockVal.resize(buckets.size());

    unconfTxs.resize(maxConfirms);
    for (unsigned int i = 0; i < maxConfirms; i++) {
        unconfTxs[i].resize(buckets.size());
    }
    oldUnconfTxs.resize(buckets.size());

    for (unsigned int i = 0; i < buckets.size(); i++)
        bucketMap[buckets[i]] = i;

    LogPrint("estimatefee", "Reading estimates: %u %s buckets counting confirms up to %u blocks\n",
             numBuckets, dataTypeString, maxConfirms);
}

unsigned int TxConfirmStats::NewTx(unsigned int nBlockHeight, double val)
{
    unsigned int bucketindex = bucketMap.lower_bound(val)->second;
    unsigned int blockIndex = nBlockHeight % unconfTxs.size();
    unconfTxs[blockIndex][bucketindex]++;
    LogPrint("estimatefee", "adding to %s", dataTypeString);
    return bucketindex;
}

void TxConfirmStats::removeTx(unsigned int entryHeight, unsigned int nBestSeenHeight, unsigned int bucketindex)
{

    int blocksAgo = nBestSeenHeight - entryHeight;
    if (nBestSeenHeight == 0) // the BlockPolicyEstimator hasn't seen any blocks yet
        blocksAgo = 0;
    if (blocksAgo < 0) {
        LogPrint("estimatefee", "Blockpolicy error, blocks ago is negative for mempool tx\n");
        return; //This can't happen because we call this with our best seen height, no entries can have higher
    }

    if (blocksAgo >= (int)unconfTxs.size()) {
        if (oldUnconfTxs[bucketindex] > 0)
            oldUnconfTxs[bucketindex]--;
        else
            LogPrint("estimatefee", "Blockpolicy error, mempool tx removed from >25 blocks,bucketIndex=%u already\n",
                     bucketindex);
    } else {
        unsigned int blockIndex = entryHeight % unconfTxs.size();
        if (unconfTxs[blockIndex][bucketindex] > 0)
            unconfTxs[blockIndex][bucketindex]--;
        else
            LogPrint("estimatefee", "Blockpolicy error, mempool tx removed from blockIndex=%u,bucketIndex=%u already\n",
                     blockIndex, bucketindex);
    }
}

void CBlockPolicyEstimator::removeTx(uint256 hash)
{
    std::map<uint256, TxStatsInfo>::iterator pos = mapMemPoolTxs.find(hash);
    if (pos == mapMemPoolTxs.end()) {
        LogPrint("estimatefee", "Blockpolicy error mempool tx %s not found for removeTx\n",
                 hash.ToString().c_str());
        return;
    }
    TxConfirmStats* stats = pos->second.stats;
    unsigned int entryHeight = pos->second.blockHeight;
    unsigned int bucketIndex = pos->second.bucketIndex;

    if (stats != NULL)
        stats->removeTx(entryHeight, nBestSeenHeight, bucketIndex);
    mapMemPoolTxs.erase(hash);
}

CBlockPolicyEstimator::CBlockPolicyEstimator(const CFeeRate& _minRelayFee)
    : nBestSeenHeight(0)
{
    minTrackedFee = _minRelayFee < CFeeRate(MIN_FEERATE) ? CFeeRate(MIN_FEERATE) : _minRelayFee;
    std::vector<double> vfeelist;
    for (double bucketBoundary = minTrackedFee.GetFeePerK(); bucketBoundary <= MAX_FEERATE; bucketBoundary *= FEE_SPACING) {
        vfeelist.push_back(bucketBoundary);
    }
    vfeelist.push_back(INF_FEERATE);
    feeStats.Initialize(vfeelist, MAX_BLOCK_CONFIRMS, DEFAULT_DECAY, "FeeRate");

    minTrackedPriority = AllowFreeThreshold() < MIN_PRIORITY ? MIN_PRIORITY : AllowFreeThreshold();
    std::vector<double> vprilist;
    for (double bucketBoundary = minTrackedPriority; bucketBoundary <= MAX_PRIORITY; bucketBoundary *= PRI_SPACING) {
        vprilist.push_back(bucketBoundary);
    }
    vprilist.push_back(INF_PRIORITY);
    priStats.Initialize(vprilist, MAX_BLOCK_CONFIRMS, DEFAULT_DECAY, "Priority");

    feeUnlikely = CFeeRate(0);
    feeLikely = CFeeRate(INF_FEERATE);
    priUnlikely = 0;
    priLikely = INF_PRIORITY;
}

bool CBlockPolicyEstimator::isFeeDataPoint(const CFeeRate& fee, double pri)
{
    if ((pri < minTrackedPriority && fee >= minTrackedFee) || (pri < priUnlikely && fee > feeLikely)) {
        return true;
    }
    return false;
}

bool CBlockPolicyEstimator::isPriDataPoint(const CFeeRate& fee, double pri)
{
    if ((fee < minTrackedFee && pri >= minTrackedPriority) || (fee < feeUnlikely && pri > priLikely)) {
        return true;
    }
    return false;
}

void CBlockPolicyEstimator::processTransaction(const CTxMemPoolEntry& entry, bool fCurrentEstimate)
{
    unsigned int txHeight = entry.GetHeight();
    uint256 hash = entry.GetTx().GetHash();
    if (mapMemPoolTxs[hash].stats != NULL) {
        LogPrint("estimatefee", "Blockpolicy error mempool tx %s already being tracked\n",
                 hash.ToString().c_str());
        return;
    }

    if (txHeight < nBestSeenHeight) {

        return;
    }

    if (!fCurrentEstimate)
        return;

    if (!entry.WasClearAtEntry()) {

        return;
    }

    CFeeRate feeRate(entry.GetFee(), entry.GetTxSize());

    double curPri = entry.GetPriority(txHeight);
    mapMemPoolTxs[hash].blockHeight = txHeight;

    LogPrint("estimatefee", "Blockpolicy mempool tx %s ", hash.ToString().substr(0, 10));

    if (entry.GetFee() == 0 || isPriDataPoint(feeRate, curPri)) {
        mapMemPoolTxs[hash].stats = &priStats;
        mapMemPoolTxs[hash].bucketIndex = priStats.NewTx(txHeight, curPri);
    }

    else if (isFeeDataPoint(feeRate, curPri)) {
        mapMemPoolTxs[hash].stats = &feeStats;
        mapMemPoolTxs[hash].bucketIndex = feeStats.NewTx(txHeight, (double)feeRate.GetFeePerK());
    } else {
        LogPrint("estimatefee", "not adding");
    }
    LogPrint("estimatefee", "\n");
}

void CBlockPolicyEstimator::processBlockTx(unsigned int nBlockHeight, const CTxMemPoolEntry& entry)
{
    if (!entry.WasClearAtEntry()) {

        return;
    }

    int blocksToConfirm = nBlockHeight - entry.GetHeight();
    if (blocksToConfirm <= 0) {

        LogPrint("estimatefee", "Blockpolicy error Transaction had negative blocksToConfirm\n");
        return;
    }

    CFeeRate feeRate(entry.GetFee(), entry.GetTxSize());

    double curPri = entry.GetPriority(nBlockHeight);

    if (entry.GetFee() == 0 || isPriDataPoint(feeRate, curPri)) {
        priStats.Record(blocksToConfirm, curPri);
    }

    else if (isFeeDataPoint(feeRate, curPri)) {
        feeStats.Record(blocksToConfirm, (double)feeRate.GetFeePerK());
    }
}

void CBlockPolicyEstimator::processBlock(unsigned int nBlockHeight,
                                         std::vector<CTxMemPoolEntry>& entries, bool fCurrentEstimate)
{
    if (nBlockHeight <= nBestSeenHeight) {

        return;
    }
    nBestSeenHeight = nBlockHeight;

    if (!fCurrentEstimate)
        return;

    LogPrint("estimatefee", "Blockpolicy recalculating dynamic cutoffs:\n");
    priLikely = priStats.EstimateMedianVal(2, SUFFICIENT_PRITXS, MIN_SUCCESS_PCT, true, nBlockHeight);
    if (priLikely == -1)
        priLikely = INF_PRIORITY;

    double feeLikelyEst = feeStats.EstimateMedianVal(2, SUFFICIENT_FEETXS, MIN_SUCCESS_PCT, true, nBlockHeight);
    if (feeLikelyEst == -1)
        feeLikely = CFeeRate(INF_FEERATE);
    else
        feeLikely = CFeeRate(feeLikelyEst);

    priUnlikely = priStats.EstimateMedianVal(10, SUFFICIENT_PRITXS, UNLIKELY_PCT, false, nBlockHeight);
    if (priUnlikely == -1)
        priUnlikely = 0;

    double feeUnlikelyEst = feeStats.EstimateMedianVal(10, SUFFICIENT_FEETXS, UNLIKELY_PCT, false, nBlockHeight);
    if (feeUnlikelyEst == -1)
        feeUnlikely = CFeeRate(0);
    else
        feeUnlikely = CFeeRate(feeUnlikelyEst);

    feeStats.ClearCurrent(nBlockHeight);
    priStats.ClearCurrent(nBlockHeight);

    for (unsigned int i = 0; i < entries.size(); i++)
        processBlockTx(nBlockHeight, entries[i]);

    feeStats.UpdateMovingAverages();
    priStats.UpdateMovingAverages();

    LogPrint("estimatefee", "Blockpolicy after updating estimates for %u confirmed entries, new mempool map size %u\n",
             entries.size(), mapMemPoolTxs.size());
}

CFeeRate CBlockPolicyEstimator::estimateFee(int confTarget)
{

    if (confTarget <= 0 || (unsigned int)confTarget > feeStats.GetMaxConfirms())
        return CFeeRate(0);

    double median = feeStats.EstimateMedianVal(confTarget, SUFFICIENT_FEETXS, MIN_SUCCESS_PCT, true, nBestSeenHeight);

    if (median < 0)
        return CFeeRate(0);

    return CFeeRate(median);
}

CFeeRate CBlockPolicyEstimator::estimateSmartFee(int confTarget, int* answerFoundAtTarget, const CTxMemPool& pool)
{
    if (answerFoundAtTarget)
        *answerFoundAtTarget = confTarget;

    if (confTarget <= 0 || (unsigned int)confTarget > feeStats.GetMaxConfirms())
        return CFeeRate(0);

    double median = -1;
    while (median < 0 && (unsigned int)confTarget <= feeStats.GetMaxConfirms()) {
        median = feeStats.EstimateMedianVal(confTarget++, SUFFICIENT_FEETXS, MIN_SUCCESS_PCT, true, nBestSeenHeight);
    }

    if (answerFoundAtTarget)
        *answerFoundAtTarget = confTarget - 1;

    CAmount minPoolFee = pool.GetMinFee(GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000).GetFeePerK();
    if (minPoolFee > 0 && minPoolFee > median)
        return CFeeRate(minPoolFee);

    if (median < 0)
        return CFeeRate(0);

    return CFeeRate(median);
}

double CBlockPolicyEstimator::estimatePriority(int confTarget)
{

    if (confTarget <= 0 || (unsigned int)confTarget > priStats.GetMaxConfirms())
        return -1;

    return priStats.EstimateMedianVal(confTarget, SUFFICIENT_PRITXS, MIN_SUCCESS_PCT, true, nBestSeenHeight);
}

double CBlockPolicyEstimator::estimateSmartPriority(int confTarget, int* answerFoundAtTarget, const CTxMemPool& pool)
{
    if (answerFoundAtTarget)
        *answerFoundAtTarget = confTarget;

    if (confTarget <= 0 || (unsigned int)confTarget > priStats.GetMaxConfirms())
        return -1;

    CAmount minPoolFee = pool.GetMinFee(GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000).GetFeePerK();
    if (minPoolFee > 0)
        return INF_PRIORITY;

    double median = -1;
    while (median < 0 && (unsigned int)confTarget <= priStats.GetMaxConfirms()) {
        median = priStats.EstimateMedianVal(confTarget++, SUFFICIENT_PRITXS, MIN_SUCCESS_PCT, true, nBestSeenHeight);
    }

    if (answerFoundAtTarget)
        *answerFoundAtTarget = confTarget - 1;

    return median;
}

void CBlockPolicyEstimator::Write(CAutoFile& fileout)
{
    fileout << nBestSeenHeight;
    feeStats.Write(fileout);
    priStats.Write(fileout);
}

void CBlockPolicyEstimator::Read(CAutoFile& filein)
{
    int nFileBestSeenHeight;
    filein >> nFileBestSeenHeight;
    feeStats.Read(filein);
    priStats.Read(filein);
    nBestSeenHeight = nFileBestSeenHeight;
}

FeeFilterRounder::FeeFilterRounder(const CFeeRate& minIncrementalFee)
{
    CAmount minFeeLimit = minIncrementalFee.GetFeePerK() / 2;
    feeset.insert(0);
    for (double bucketBoundary = minFeeLimit; bucketBoundary <= MAX_FEERATE; bucketBoundary *= FEE_SPACING) {
        feeset.insert(bucketBoundary);
    }
}

CAmount FeeFilterRounder::round(CAmount currentMinFee)
{
    std::set<double>::iterator it = feeset.lower_bound(currentMinFee);
    if ((it != feeset.begin() && insecure_rand() % 3 != 0) || it == feeset.end()) {
        it--;
    }
    return *it;
}
