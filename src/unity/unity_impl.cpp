// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

// Unity specific includes
#include "unity_impl.h"
#include "libinit.h"

// Standard gulden headers
#include "util.h"
#include "Gulden/util.h"
#include "ui_interface.h"
#include "wallet/wallet.h"
#include "unity/appmanager.h"
#include "utilmoneystr.h"
#include <chain.h>
#include "consensus/validation.h"
#include "net.h"
#include "Gulden/mnemonic.h"

// Djinni generated files
#include "gulden_unified_backend.hpp"
#include "gulden_unified_frontend.hpp"
#include "qrcode_record.hpp"
#include "balance_record.hpp"
#include "uri_record.hpp"
#include "uri_recipient.hpp"
#include "transaction_record.hpp"
#include "transaction_type.hpp"
#include "address_record.hpp"
#include "djinni_support.hpp"

// External libraries
#include <boost/algorithm/string.hpp>
#include <qrencode.h>

void calculateTransactionRecordsForWalletTransaction(const CWalletTx& wtx, std::vector<TransactionRecord>& transactionRecords)
{
    std::list<COutputEntry> listReceived;
    std::list<COutputEntry> listSent;
    CAmount nFee;

    wtx.GetAmounts(listReceived, listSent, nFee, ISMINE_SPENDABLE, nullptr);
    if ((!listSent.empty() || nFee != 0) )
    {
        for(const COutputEntry& s : listSent)
        {
            std::string address;
            CGuldenAddress addr;
            if (addr.Set(s.destination))
                address = addr.ToString();
            std::string label;
            if (pactiveWallet->mapAddressBook.count(CGuldenAddress(s.destination).ToString())) {
                label = pactiveWallet->mapAddressBook[CGuldenAddress(s.destination).ToString()].name;
            }
            transactionRecords.push_back(TransactionRecord(TransactionType::SEND, s.amount, address, label, wtx.nTimeSmart));
        }
    }
    if (listReceived.size() > 0)
    {
        for(const COutputEntry& r : listReceived)
        {
            std::string address;
            CGuldenAddress addr;
            if (addr.Set(r.destination))
                address = addr.ToString();
            std::string label;
            if (pactiveWallet->mapAddressBook.count(CGuldenAddress(r.destination).ToString())) {
                label = pactiveWallet->mapAddressBook[CGuldenAddress(r.destination).ToString()].name;
            }
            transactionRecords.push_back(TransactionRecord(TransactionType::RECEIVE, r.amount, address, label, wtx.nTimeSmart));
        }
    }
}

static std::shared_ptr<GuldenUnifiedFrontend> signalHandler;

static void notifyBalanceChanged(CWallet* pwallet)
{
    if (pwallet && signalHandler)
    {
        WalletBalances balances;
        pwallet->GetBalances(balances, nullptr, false);
        signalHandler->notifyBalanceChange(BalanceRecord(balances.availableIncludingLocked, balances.availableExcludingLocked, balances.availableLocked, balances.unconfirmedIncludingLocked, balances.unconfirmedExcludingLocked, balances.unconfirmedLocked, balances.immatureIncludingLocked, balances.immatureExcludingLocked, balances.immatureLocked, balances.totalLocked));
    }
}

void terminateUnityFrontend()
{
    if (signalHandler)
    {
        signalHandler->notifyShutdown();
    }
}

void handlePostInitMain()
{
    if (signalHandler)
    {
        signalHandler->notifyCoreReady();
    }
    // Update sync progress as we receive headers/blocks.
    uiInterface.NotifySPVProgress.connect(
        [=](int startHeight, int processedHeight, int expectedHeight)
        {
            if (signalHandler)
            {
                signalHandler->notifySPVProgress(startHeight, processedHeight, expectedHeight);
            }
        }
    );

    // Update transaction/balance changes
    if (pactiveWallet)
    {
        pactiveWallet->NotifyTransactionChanged.connect( [&](CWallet* pwallet, const uint256& hash, ChangeType status) 
        {
            {
                DS_LOCK2(cs_main, pwallet->cs_wallet);
                if (pwallet->mapWallet.find(hash) != pwallet->mapWallet.end())
                {
                    const CWalletTx& wtx = pwallet->mapWallet[hash];
                    if (status == CT_NEW)
                    {
                        std::vector<TransactionRecord> walletTransactions;
                        calculateTransactionRecordsForWalletTransaction(wtx, walletTransactions);
                        for (const auto& tx: walletTransactions)
                        {
                            LogPrintf("unity: notify transaction changed [2] %s",hash.ToString().c_str());
                            if (signalHandler)
                            {
                                signalHandler->notifyNewTransaction(tx);
                            }
                        }
                    }
                    else if (status == CT_DELETED)
                    {
                        //fixme: (UNITY) - Consider implementing f.e.x if a 0 conf transaction gets deleted...
                    }
                }
            }
            notifyBalanceChanged(pwallet);
        } );

        // Fire once immediately to update with latest on load.
        //notifyBalanceChanged(pactiveWallet);
    }
}

void handleInitWithExistingWallet()
{
    if (signalHandler)
    {
        signalHandler->notifyInitWithExistingWallet();
    }
    GuldenAppManager::gApp->initialize();
}

void handleInitWithoutExistingWallet()
{
    signalHandler->notifyInitWithoutExistingWallet();
}

bool GuldenUnifiedBackend::InitWalletFromRecoveryPhrase(const std::string& phrase)
{
    // Refuse to acknowledge an empty recovery phrase, or one that doesn't pass even the most obvious requirement
    if (phrase.length() < 16)
    {
        return false;
    }

    //fixme: (SPV) - Handle all the various birth date (or lack of birthdate) cases here instead of just the one.
    SecureString phraseOnly;
    int phraseBirthNumber = 0;
    GuldenAppManager::gApp->splitRecoveryPhraseAndBirth(phrase.c_str(), phraseOnly, phraseBirthNumber);
    if (phraseBirthNumber == 0 || !checkMnemonic(phraseOnly))
    {
        return false;
    }

    //fixme: (SPV) - Handle all the various birth date (or lack of birthdate) cases here instead of just the one.
    GuldenAppManager::gApp->setRecoveryPhrase(phraseOnly);
    GuldenAppManager::gApp->setRecoveryBirthNumber(phraseBirthNumber);
    GuldenAppManager::gApp->isRecovery = true;
    GuldenAppManager::gApp->initialize();

    return true;
}

bool GuldenUnifiedBackend::IsValidRecoveryPhrase(const std::string & phrase)
{
    if (phrase.length() < 16)
        return false;

    SecureString phraseOnly;
    int phraseBirthNumber = 0;
    GuldenAppManager::gApp->splitRecoveryPhraseAndBirth(phrase.c_str(), phraseOnly, phraseBirthNumber);
    if (phraseBirthNumber == 0 || !checkMnemonic(phraseOnly))
        return false;

    return true;
}

std::string GuldenUnifiedBackend::GenerateRecoveryMnemonic()
{
    std::vector<unsigned char> entropy(16);
    GetStrongRandBytes(&entropy[0], 16);
    return GuldenAppManager::gApp->composeRecoveryPhrase(mnemonicFromEntropy(entropy, entropy.size()*8), GetAdjustedTime()).c_str();
}

bool GuldenUnifiedBackend::InitWalletLinkedFromURI(const std::string& linked_uri)
{
    //fixme: (SPV) - Implement
    GuldenAppManager::gApp->initialize();
    return true;
}

int32_t GuldenUnifiedBackend::InitUnityLib(const std::string& dataDir, bool testnet, const std::shared_ptr<GuldenUnifiedFrontend>& signals)
{
    // Force the datadir to specific place on e.g. android devices
    if (!dataDir.empty())
        SoftSetArg("-datadir", dataDir);

    // SPV wallets definitely shouldn't be listening for incoming connections at all
    SoftSetArg("-listen", "0");

    // Mininmise logging for performance reasons
    SoftSetArg("-debug", "0");

    // Turn SPV mode on
    SoftSetArg("-fullsync", "0");
    SoftSetArg("-spv", "1");

    // Minimise lookahead size for performance reasons
    SoftSetArg("-accountpool", "1");

    // Minimise background threads and memory consumption
    SoftSetArg("-par", "-100");
    SoftSetArg("-maxsigcachesize", "0");
    SoftSetArg("-dbcache", "4");
    SoftSetArg("-maxmempool", "5");
    SoftSetArg("-maxconnections", "8");

    // Testnet
    if (testnet)
    {
        SoftSetArg("-testnet", "C1534687770:60");
        SoftSetArg("-addnode", "devbak.net");
    }

    //fixme: (2.1) Reverse headers
    // Temporarily disable reverse headers for mobile until memory requirements can be reduced.
    SoftSetArg("-reverseheaders", "false");

    signalHandler = signals;

    return InitUnity();
}

void GuldenUnifiedBackend::TerminateUnityLib()
{
    return GuldenAppManager::gApp->shutdown();
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


UriRecipient GuldenUnifiedBackend::IsValidRecipient(const UriRecord & request)
{
     // return if URI is not valid or is no Gulden: URI
    std::string lowerCaseScheme = boost::algorithm::to_lower_copy(request.scheme);
    if (lowerCaseScheme != "guldencoin" && lowerCaseScheme != "gulden")
        return UriRecipient(false, "", "", "");

    if (!CGuldenAddress(request.path).IsValid())
        return UriRecipient(false, "", "", "");

    std::string address = request.path;
    std::string label = "";
    std::string amount = "";
    if (request.items.find("amount") != request.items.end())
        amount = request.items.find("amount")->second;
    if (pactiveWallet)
    {
        DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
        if (pactiveWallet->mapAddressBook.find(address) != pactiveWallet->mapAddressBook.end())
        {
            label = pactiveWallet->mapAddressBook[address].name;
        }
    }

    return UriRecipient(true, address, label, amount);
}

bool GuldenUnifiedBackend::performPaymentToRecipient(const UriRecipient & request)
{
    if (!pactiveWallet)
        return false;

    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);

    CGuldenAddress address(request.address);
    if (!address.IsValid())
    {
        LogPrintf("performPaymentToRecipient: invalid address %s", request.address.c_str());
        return false;
    }

    CAmount nAmount;
    if (!ParseMoney(request.amount, nAmount))
    {
        LogPrintf("performPaymentToRecipient: invalid amount %s", request.amount.c_str());
        return false;
    }

    bool fSubtractFeeFromAmount = false;

    CRecipient recipient = GetRecipientForDestination(address.Get(), nAmount, false, GetPoW2Phase(chainActive.Tip(), Params(), chainActive));
    std::vector<CRecipient> vecSend;
    vecSend.push_back(recipient);

    CWalletTx wtx;
    CAmount nFeeRequired;
    int nChangePosRet = -1;
    std::string strError;
    CReserveKeyOrScript reservekey(pactiveWallet, pactiveWallet->activeAccount, KEYCHAIN_CHANGE);
    if (!pactiveWallet->CreateTransaction(pactiveWallet->activeAccount, vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError))
    {
        LogPrintf("performPaymentToRecipient: failed to create transaction %s",strError.c_str());
        return false;
    }

    CValidationState state;
    if (!pactiveWallet->CommitTransaction(wtx, reservekey, g_connman.get(), state))
    {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", state.GetRejectReason());
        LogPrintf("performPaymentToRecipient: failed to commit transaction %s",strError.c_str());
        return false;
    }

    return true;
}

std::vector<TransactionRecord> GuldenUnifiedBackend::getTransactionHistory()
{
    std::vector<TransactionRecord> ret;

    if (!pactiveWallet)
        return ret;

    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);

    for (const auto& [hash, wtx] : pactiveWallet->mapWallet)
    {
        calculateTransactionRecordsForWalletTransaction(wtx, ret);
    }
    std::sort(ret.begin(), ret.end(), [&](TransactionRecord& x, TransactionRecord& y){ return (x.timestamp > y.timestamp); });
    return ret;
}

std::vector<AddressRecord> GuldenUnifiedBackend::getAddressBookRecords()
{
    std::vector<AddressRecord> ret;
    if (pactiveWallet)
    {
        DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
        for(const auto& [address, addressData] : pactiveWallet->mapAddressBook)
        {
            ret.emplace_back(AddressRecord(address, addressData.purpose, addressData.name));
        }
    }

    return ret;
}

void GuldenUnifiedBackend::addAddressBookRecord(const AddressRecord& address)
{
    if (pactiveWallet)
    {
        pactiveWallet->SetAddressBook(address.address, address.name, address.purpose);
    }
}

void GuldenUnifiedBackend::deleteAddressBookRecord(const AddressRecord& address)
{
    if (pactiveWallet)
    {
        pactiveWallet->DelAddressBook(address.address);
    }
}
