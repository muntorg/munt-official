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
#include "net_processing.h"
#include "wallet/spvscanner.h"
#include "sync.h"

// Djinni generated files
#include "gulden_unified_backend.hpp"
#include "gulden_unified_frontend.hpp"
#include "qrcode_record.hpp"
#include "balance_record.hpp"
#include "uri_record.hpp"
#include "uri_recipient.hpp"
#include "mutation_record.hpp"
#include "transaction_record.hpp"
#include "output_record.hpp"
#include "address_record.hpp"
#include "peer_record.hpp"
#include "blockinfo_record.hpp"
#include "monitor_record.hpp"
#include "gulden_monitor_listener.hpp"
#ifdef __ANDROID__
#include "djinni_support.hpp"
#endif

// External libraries
#include <boost/algorithm/string.hpp>
#include <qrencode.h>
#include <memory>

std::shared_ptr<GuldenUnifiedFrontend> signalHandler;

CCriticalSection cs_monitoringListeners;
std::set<std::shared_ptr<GuldenMonitorListener> > monitoringListeners;

TransactionRecord calculateTransactionRecordForWalletTransaction(const CWalletTx& wtx)
{
    std::vector<OutputRecord> sentOutputs;
    std::vector<OutputRecord> receivedOutputs;

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
            sentOutputs.push_back(OutputRecord(s.amount, address, label));
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
            receivedOutputs.push_back(OutputRecord(r.amount, address, label));
        }
    }

    const int RECOMMENDED_CONFIRMATIONS = 3;

    int depth = wtx.GetDepthInMainChain();

    TransactionStatus status;
    if (depth < 0)
        status = TransactionStatus::CONFLICTED;
    else if (depth == 0) {
        if (wtx.isAbandoned())
            status = TransactionStatus::ABANDONED;
        else
            status = TransactionStatus::UNCONFIRMED;
    }
    else if (depth < RECOMMENDED_CONFIRMATIONS) {
        status = TransactionStatus::CONFIRMING;
    }
    else {
        status = TransactionStatus::CONFIRMED;
    }

    return TransactionRecord(wtx.GetHash().ToString(), wtx.nTimeSmart,
                             wtx.GetCredit(ISMINE_SPENDABLE) - wtx.GetDebit(ISMINE_SPENDABLE),
                             nFee, status, wtx.nHeight, wtx.nBlockTime, wtx.GetDepthInMainChain(),
                             receivedOutputs, sentOutputs);
}

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

    // unified progress notification
    uiInterface.NotifyUnifiedProgress.connect([=](float progress) {
        if (signalHandler)
            signalHandler->notifyUnifiedProgress(progress);
    });

    // monitoring listeners notifications
    uiInterface.NotifyHeaderProgress.connect([=](int, int, int, int64_t) {
        int32_t height, probable_height, offset;
        {
            LOCK(cs_main);
            height = partialChain.Height();
            probable_height = GetProbableHeight();
            offset = partialChain.HeightOffset();
        }
        LOCK(cs_monitoringListeners);
        for (const auto &listener: monitoringListeners) {
            listener->onPartialChain(height, probable_height, offset);
        }
    });

    uiInterface.NotifySPVPrune.connect([=](int height) {
        LOCK(cs_monitoringListeners);
        for (const auto &listener: monitoringListeners) {
            listener->onPruned(height);
        }
    });

    uiInterface.NotifySPVProgress.connect([=](int /*start_height*/, int processed_height, int /*probable_height*/) {
        LOCK(cs_monitoringListeners);
        for (const auto &listener: monitoringListeners) {
            listener->onProcessedSPVBlocks(processed_height);
        }
    });

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
                    if (status == CT_NEW || status == CT_UPDATED)
                    {
                        LogPrintf("unity: notify transaction changed [2] %s",hash.ToString().c_str());
                        if (signalHandler)
                        {
                            TransactionRecord walletTransactions = calculateTransactionRecordForWalletTransaction(wtx);
                            if (status == CT_NEW)
                                signalHandler->notifyNewTransaction(walletTransactions);
                            else // status == CT_UPDATED
                                signalHandler->notifyUpdatedTransaction(walletTransactions);
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
        notifyBalanceChanged(pactiveWallet);
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
    if (!checkMnemonic(phraseOnly))
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
    return checkMnemonic(phraseOnly) && (phraseBirthNumber == 0 || Base10ChecksumDecode(phraseBirthNumber, nullptr));
}

std::string GuldenUnifiedBackend::GenerateRecoveryMnemonic()
{
    std::vector<unsigned char> entropy(16);
    GetStrongRandBytes(&entropy[0], 16);
    return GuldenAppManager::gApp->composeRecoveryPhrase(mnemonicFromEntropy(entropy, entropy.size()*8), GetAdjustedTime()).c_str();
}

bool GuldenUnifiedBackend::InitWalletLinkedFromURI(const std::string& linked_uri)
{
    CGuldenSecretExt<CExtKey> linkedKey;
    if (!linkedKey.fromURIString(linked_uri))
    {
        return false;
    }
    GuldenAppManager::gApp->setLinkKey(linkedKey);
    GuldenAppManager::gApp->isLink = true;
    GuldenAppManager::gApp->initialize();

    return true;
}


bool GuldenUnifiedBackend::ReplaceWalletLinkedFromURI(const std::string& linked_uri)
{
    LOCK2(cs_main, pactiveWallet->cs_wallet);

    if (!pactiveWallet || !pactiveWallet->activeAccount)
    {
        LogPrintf("ReplaceWalletLinkedFromURI: no active wallet");
        return false;
    }

    // Create ext key for new linked account from parsed data
    CGuldenSecretExt<CExtKey> linkedKey;
    if (!linkedKey.fromURIString(linked_uri))
    {
        LogPrintf("ReplaceWalletLinkedFromURI: failed to parse link URI");
        return false;
    }

    // Ensure we have a valid location to send all the funds
    CGuldenAddress address(linkedKey.getPayAccount());
    if (!address.IsValid())
    {
        LogPrintf("ReplaceWalletLinkedFromURI: invalid address %s", linkedKey.getPayAccount().c_str());
        return false;
    }

    // Empty wallet to target address
    bool fSubtractFeeFromAmount = true;
    for (const auto& [accountUUID, pAccount] : pactiveWallet->mapAccounts)
    {
        CAmount nBalance = pactiveWallet->GetLegacyBalance(ISMINE_SPENDABLE, 0, &accountUUID);
        if (nBalance > 0)
        {
            std::vector<CRecipient> vecSend;
            CRecipient recipient = GetRecipientForDestination(address.Get(), nBalance, fSubtractFeeFromAmount, GetPoW2Phase(chainActive.Tip(), Params(), chainActive));
            vecSend.push_back(recipient);

            CWalletTx wtx;
            CAmount nFeeRequired;
            int nChangePosRet = -1;
            std::string strError;
            CReserveKeyOrScript reservekey(pactiveWallet, pAccount, KEYCHAIN_CHANGE);
            if (!pactiveWallet->CreateTransaction(pAccount, vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError))
            {
                LogPrintf("ReplaceWalletLinkedFromURI: failed to create transaction %s",strError.c_str());
                return false;
            }
        }
    }

    // Purge all current accounts/seeds from the system
    LogPrintf("ReplaceWalletLinkedFromURI: Begin purge seeds");
    while (!pactiveWallet->mapSeeds.empty())
    {
        LogPrintf("ReplaceWalletLinkedFromURI: purge seeds");
        pactiveWallet->DeleteSeed(pactiveWallet->mapSeeds.begin()->second, true);
    }
    LogPrintf("ReplaceWalletLinkedFromURI: End purge seeds");

    // Purge all transactions from the wallet
    std::vector<uint256> hashesToErase;
    std::vector<uint256> hashesErased;
    hashesToErase.reserve(pactiveWallet->mapWallet.size());
    for (const auto& [txHash, tx] : pactiveWallet->mapWallet)
    {
        hashesToErase.push_back(txHash);
    }
    pactiveWallet->ZapSelectTx(hashesToErase, hashesErased);

    // Create a new linked account as the primary account
    pactiveWallet->nTimeFirstKey = linkedKey.getCreationTime();
    LogPrintf("Creating new linked primary account, birth time [%d]\n", pactiveWallet->nTimeFirstKey);
    pactiveWallet->activeAccount = pactiveWallet->CreateSeedlessHDAccount("My account", linkedKey, AccountState::Normal, AccountType::Mobi);

    // Write the primary account into wallet file
    {
        CWalletDB walletdb(*pactiveWallet->dbw);
        if (!walletdb.WriteAccount(getUUIDAsString(pactiveWallet->activeAccount->getUUID()), pactiveWallet->activeAccount))
        {
            LogPrintf("ReplaceWalletLinkedFromURI: failed to write new linked account");
            return false;
        }
        walletdb.WritePrimaryAccount(pactiveWallet->activeAccount);
    }

    // Allow update of balance for deleted accounts/transactions
    notifyBalanceChanged(pactiveWallet);

    // Rescan for transactions on the linked account
    DoRescan();

    return true;
}

bool GuldenUnifiedBackend::IsValidLinkURI(const std::string& linked_uri)
{
    CGuldenSecretExt<CExtKey> linkedKey;
    if (!linkedKey.fromURIString(linked_uri))
        return false;
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
    GuldenAppManager::gApp->shutdown();
    GuldenAppManager::gApp->waitForShutDown();
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

    LOCK2(cs_main, pactiveWallet->cs_wallet);
    //WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    //if (ctx.isValid())
    {
        int64_t birthTime = pactiveWallet->birthTime();

        std::set<SecureString> allPhrases;
        for (const auto& seedIter : pactiveWallet->mapSeeds)
        {
            return GuldenAppManager::composeRecoveryPhrase(seedIter.second->getMnemonic(), birthTime).c_str();
        }
    }
    return "";
}

bool GuldenUnifiedBackend::HaveUnconfirmedFunds()
{
    if (!pactiveWallet)
        return true;

    WalletBalances balances;
    pactiveWallet->GetBalances(balances, nullptr, false);

    if (balances.unconfirmedIncludingLocked > 0 || balances.immatureIncludingLocked > 0)
    {
        return true;
    }
    return false;
}

int64_t GuldenUnifiedBackend::GetBalance()
{
    if (!pactiveWallet)
        return 0;

    WalletBalances balances;
    pactiveWallet->GetBalances(balances, nullptr, false);
    return balances.availableIncludingLocked + balances.unconfirmedIncludingLocked + balances.immatureIncludingLocked;
}

void GuldenUnifiedBackend::DoRescan()
{
    if (pactiveWallet)
    {
        boost::thread t(rescanThread); // thread runs free
    }
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

void GuldenUnifiedBackend::performPaymentToRecipient(const UriRecipient & request)
{
    if (!pactiveWallet)
        throw std::runtime_error(_("No active internal wallet."));

    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);

    CGuldenAddress address(request.address);
    if (!address.IsValid())
    {
        LogPrintf("performPaymentToRecipient: invalid address %s", request.address.c_str());
        throw std::runtime_error(_("Invalid address"));
    }

    CAmount nAmount;
    if (!ParseMoney(request.amount, nAmount))
    {
        LogPrintf("performPaymentToRecipient: invalid amount %s", request.amount.c_str());
        throw std::runtime_error(_("Invalid amount"));
    }

    bool fSubtractFeeFromAmount = false;

    CRecipient recipient = GetRecipientForDestination(address.Get(), nAmount, fSubtractFeeFromAmount, GetPoW2Phase(chainActive.Tip(), Params(), chainActive));
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
        throw std::runtime_error(strprintf(_("Failed to create transaction\n%s"), strError));
    }

    CValidationState state;
    if (!pactiveWallet->CommitTransaction(wtx, reservekey, g_connman.get(), state))
    {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", state.GetRejectReason());
        LogPrintf("performPaymentToRecipient: failed to commit transaction %s",strError.c_str());
        throw std::runtime_error(strprintf(_("Transaction rejected, reason: %s"), state.GetRejectReason()));
    }
}

std::vector<TransactionRecord> GuldenUnifiedBackend::getTransactionHistory()
{
    std::vector<TransactionRecord> ret;

    if (!pactiveWallet)
        return ret;

    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);

    for (const auto& [hash, wtx] : pactiveWallet->mapWallet)
    {
        TransactionRecord tx = calculateTransactionRecordForWalletTransaction(wtx);
        ret.push_back(tx);
    }
    std::sort(ret.begin(), ret.end(), [&](TransactionRecord& x, TransactionRecord& y){ return (x.timestamp > y.timestamp); });
    return ret;
}

TransactionRecord GuldenUnifiedBackend::getTransaction(const std::string & txHash)
{
    if (!pactiveWallet)
        throw std::runtime_error(strprintf("No active wallet to query tx hash [%s]", txHash));

    uint256 hash = uint256S(txHash);

    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);

    if (pactiveWallet->mapWallet.find(hash) == pactiveWallet->mapWallet.end())
        throw std::runtime_error(strprintf("No transaction found for hash [%s]", txHash));

    const CWalletTx& wtx = pactiveWallet->mapWallet[hash];
    return calculateTransactionRecordForWalletTransaction(wtx);
}

std::vector<MutationRecord> GuldenUnifiedBackend::getMutationHistory()
{
    std::vector<MutationRecord> ret;

    if (!pactiveWallet)
        return ret;

    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);

    // wallet transactions in reverse chronological ordering
    std::vector<const CWalletTx*> vWtx;
    for (const auto& [hash, wtx] : pactiveWallet->mapWallet)
        vWtx.push_back(&wtx);
    std::sort(vWtx.begin(), vWtx.end(), [&](const CWalletTx* x, const CWalletTx* y){ return (x->nTimeSmart > y->nTimeSmart); });

    // build mutation list based on transactions
    for (const CWalletTx* wtx : vWtx)
    {
        int64_t substracted = wtx->GetDebit(ISMINE_SPENDABLE);
        int64_t added = wtx->GetCredit(ISMINE_SPENDABLE);

        uint64_t time = wtx->nTimeSmart;
        std::string hash = wtx->GetHash().ToString();

        // if any funds were substracted the transaction was sent by us
        if (substracted > 0) {
            int64_t fee = substracted - wtx->tx->GetValueOut();
            int64_t change = wtx->GetChange();

            // detect internal transfer and split it
            if (substracted - fee == added)
            {
                // amount received
                ret.push_back(MutationRecord(added - change, time, hash));

                // amount send including fee
                ret.push_back(MutationRecord(change - substracted, time, hash));
            }
            else
                ret.push_back(MutationRecord(added - substracted, time, hash));
        }
        else // nothing substracted so we received funds
            ret.push_back(MutationRecord(added, time, hash));
    }

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

void GuldenUnifiedBackend::PersistAndPruneForSPV()
{
    PersistAndPruneForPartialSync();
}

void GuldenUnifiedBackend::ResetUnifiedProgress()
{
    CWallet::ResetUnifiedSPVProgressNotification();
}

std::vector<PeerRecord> GuldenUnifiedBackend::getPeers()
{
    std::vector<PeerRecord> ret;

    if (g_connman) {
        std::vector<CNodeStats> vstats;
        g_connman->GetNodeStats(vstats);
        for (CNodeStats& nstat: vstats) {
            ret.push_back(PeerRecord(nstat.addr.ToString(), nstat.addr.HostnameLookup(), nstat.nStartingHeight,
                                 int32_t(nstat.dPingTime * 1000), nstat.cleanSubVer, nstat.nVersion));
        }
    }

    return ret;
}

std::vector<BlockinfoRecord> GuldenUnifiedBackend::getLastSPVBlockinfos()
{
    std::vector<BlockinfoRecord> ret;

    LOCK(cs_main);

    int height = partialChain.Height();
    while (ret.size() < 32 && height > partialChain.HeightOffset()) {
        const CBlockIndex* pindex = partialChain[height];
        ret.push_back(BlockinfoRecord(pindex->nHeight, pindex->GetBlockTime(), pindex->GetBlockHashPoW2().ToString()));
        height--;
    }

    return ret;
}

MonitorRecord GuldenUnifiedBackend::getMonitoringStats()
{
    LOCK(cs_main);
    int32_t partialHeight_ = partialChain.Height();
    int32_t partialOffset_ = partialChain.HeightOffset();
    int32_t prunedHeight_ = nPartialPruneHeightDone;
    int32_t processedSPVHeight_ = CSPVScanner::getProcessedHeight();
    int32_t probableHeight_ = GetProbableHeight();

    return MonitorRecord(partialHeight_,
                         partialOffset_,
                         prunedHeight_,
                         processedSPVHeight_,
                         probableHeight_);
}

void GuldenUnifiedBackend::RegisterMonitorListener(const std::shared_ptr<GuldenMonitorListener> & listener)
{
    LOCK(cs_monitoringListeners);
    monitoringListeners.insert(listener);
}

void GuldenUnifiedBackend::UnregisterMonitorListener(const std::shared_ptr<GuldenMonitorListener> & listener)
{
    LOCK(cs_monitoringListeners);
    monitoringListeners.erase(listener);
}
