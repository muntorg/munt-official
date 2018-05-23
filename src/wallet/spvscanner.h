#ifndef SPVSCANNER_H
#define SPVSCANNER_H

#include "../validationinterface.h"

class CWallet;

class CSPVScanner : public CValidationInterface
{
public:
    CSPVScanner(CWallet& _wallet);
    ~CSPVScanner();

    void StartScan();

    CSPVScanner(const CSPVScanner&) = delete;
    CSPVScanner& operator=(const CSPVScanner&) = delete;

protected:
    void ProcessPriorityRequest(const std::shared_ptr<const CBlock> &block, const CBlockIndex *pindex) override;
    void HeaderTipChanged(const CBlockIndex* pTip) override;

private:
    CWallet& wallet;

    // SPV scan processed up to this block
    const CBlockIndex* lastProcessed;

    // Blocks (lastProcessed .. requstTip] have been requested and are pending
    const CBlockIndex* requestTip;

    void RequestBlocks();
};

#endif // SPVSCANNER_H
