// Unity specific includes
#include "unity_impl.h"
#include "libinit.h"

// Standard gulden headers
#include "util.h"
#include "ui_interface.h"
#include "wallet/wallet.h"
#include "unity/appmanager.h"

// Djinni generated files
#include "gulden_unified_backend.hpp"
#include "gulden_unified_frontend.hpp"
#include "qrcode_record.hpp"
#include "balance_record.hpp"
#include "djinni_support.hpp"

// External libraries
#include <qrencode.h>

static std::shared_ptr<GuldenUnifiedFrontend> signalHandler;

static void notifyBalanceChanged(CWallet* pwallet)
{
    WalletBalances balances;
    pwallet->GetBalances(balances, nullptr, false);
    signalHandler->notifyBalanceChange(BalanceRecord(balances.availableIncludingLocked, balances.availableExcludingLocked, balances.availableLocked, balances.unconfirmedIncludingLocked, balances.unconfirmedExcludingLocked, balances.unconfirmedLocked, balances.immatureIncludingLocked, balances.immatureExcludingLocked, balances.immatureLocked, balances.totalLocked));
}

void handlePostInitMain()
{
    // Update sync progress as we receive headers/blocks.
    uiInterface.NotifySPVProgress.connect(
        [=](int startHeight, int processedHeight, int expectedHeight)
        {
            signalHandler->notifySPVProgress(startHeight, processedHeight, expectedHeight);
        }
    );

    // Update transaction/balance changes
    if (pactiveWallet)
    {
        pactiveWallet->NotifyTransactionChanged.connect( [&](CWallet* pwallet, const uint256& hash, ChangeType status) 
        {
            notifyBalanceChanged(pwallet);
        } );

        // Fire once immediately to update with latest on load.
        //notifyBalanceChanged(pactiveWallet);
    }
}

int32_t GuldenUnifiedBackend::InitUnityLib(const std::string& dataDir, const std::shared_ptr<GuldenUnifiedFrontend>& signals)
{
    if (!dataDir.empty())
        SoftSetArg("-datadir", dataDir);
    SoftSetArg("-listen", "0");
    SoftSetArg("-debug", "0");
    SoftSetArg("-fullsync", "0");
    SoftSetArg("-spv", "1");
    SoftSetArg("-accountpool", "1");

    signalHandler = signals;

    return InitUnity();
}

QrcodeRecord GuldenUnifiedBackend::QRImageFromString(const std::string& qr_string, int32_t width_hint)
{
    QRcode* code = QRcode_encodeString(qr_string.c_str(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!code)
    {
        return QrcodeRecord(0, std::vector<uint8_t>());
    }
    else
    {
        const int32_t generatedWidth = code->width;
        const int32_t finalWidth = (width_hint / generatedWidth) * generatedWidth;
        const int32_t scaleMultiplier = finalWidth / generatedWidth;
        std::vector<uint8_t> dataVector;
        dataVector.reserve(finalWidth*finalWidth);
        int nIndex=0;
        for (int nRow=0; nRow<generatedWidth; ++nRow)
        {
            for (int nCol=0; nCol<generatedWidth; ++nCol)
            {
                dataVector.insert(dataVector.end(), scaleMultiplier, (code->data[nIndex++] & 1) * 255);
            }
            for (int i=1; i<scaleMultiplier; ++i)
            {
                dataVector.insert(dataVector.end(), dataVector.end()-finalWidth, dataVector.end());
            }
        }
        QRcode_free(code);
        return QrcodeRecord(finalWidth, dataVector);
    }
}

std::string GuldenUnifiedBackend::GetReceiveAddress()
{
    LOCK2(cs_main, pactiveWallet->cs_wallet);

    if (!pactiveWallet || !pactiveWallet->activeAccount)
        return "";

    CReserveKeyOrScript* receiveAddress = new CReserveKeyOrScript(pactiveWallet, pactiveWallet->activeAccount, KEYCHAIN_EXTERNAL);
    CPubKey pubKey;
    if (receiveAddress->GetReservedKey(pubKey))
    {
        CKeyID keyID = pubKey.GetID();
        receiveAddress->ReturnKey();
        delete receiveAddress;
        return CGuldenAddress(keyID).ToString();
    }
    else
    {
        return "";
    }
}

//fixme: (Unity) - find a way to use char[] here as well as on the java side.
std::string GuldenUnifiedBackend::GetRecoveryPhrase()
{
    if (!pactiveWallet || !pactiveWallet->activeAccount)
        return "";

    //fixme: (Unity) - dedup; this shares common code with backupdialog.cpp
    LOCK2(cs_main, pactiveWallet->cs_wallet);
    //WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    //if (ctx.isValid())
    {
        int64_t birthTime = 0;

        // determine block time of earliest transaction (if any)
        // if this cannot be determined for every transaction a phrase without birth time acceleration will be used
        int64_t firstTransactionTime = std::numeric_limits<int64_t>::max();
        for (CWallet::TxItems::const_iterator it = pactiveWallet->wtxOrdered.begin(); it != pactiveWallet->wtxOrdered.end(); ++it)
        {
            CWalletTx* wtx = it->second.first;
            if (!wtx->hashUnset())
            {
                CBlockIndex* index = mapBlockIndex[wtx->hashBlock];
                if (index && index->IsValid(BLOCK_VALID_HEADER))
                    firstTransactionTime = std::min(firstTransactionTime, std::max(int64_t(0), index->GetBlockTime()));
                else
                {
                    firstTransactionTime = 0;
                    break;
                }
            }
        }

        int64_t tipTime;
        const CBlockIndex* lastSPVBlock = pactiveWallet->LastSPVBlockProcessed();
        if (lastSPVBlock)
            tipTime = lastSPVBlock->GetBlockTime();
        else
            tipTime = chainActive.Tip()->GetBlockTime();

        // never use a time beyond our processed tip either spv or full sync
        birthTime = std::min(tipTime, firstTransactionTime);

        std::set<SecureString> allPhrases;
        for (const auto& seedIter : pactiveWallet->mapSeeds)
        {
            return GuldenAppManager::composeRecoveryPhrase(seedIter.second->getMnemonic(), birthTime).c_str();
        }
    }
    return "";
}
