// Copyright (c) 2020 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

//Workaround braindamaged 'hack' in libtool.m4 that defines DLL_EXPORT when building a dll via libtool (this in turn imports unwanted symbols from e.g. pthread that breaks static pthread linkage)
#ifdef DLL_EXPORT
#undef DLL_EXPORT
#endif

// Unity specific includes
#include "unity_impl.h"
#include "libinit.h"

// Standard gulden headers
#include "util.h"
#include "witnessutil.h"
#include "ui_interface.h"
#include "wallet/wallet.h"
#include "unity/appmanager.h"
#include "utilmoneystr.h"
#include <chain.h>
#include "consensus/validation.h"
#include "net.h"
#include "wallet/mnemonic.h"
#include "net_processing.h"
#include "wallet/spvscanner.h"
#include "sync.h"

// Djinni generated files
#include "unified_backend.hpp"
#include "unified_frontend.hpp"
#include "qr_code_record.hpp"
#include "balance_record.hpp"
#include "uri_record.hpp"
#include "uri_recipient.hpp"
#include "mutation_record.hpp"
#include "transaction_record.hpp"
#include "input_record.hpp"
#include "output_record.hpp"
#include "address_record.hpp"
#include "peer_record.hpp"
#include "block_info_record.hpp"
#include "monitor_record.hpp"
#include "monitor_listener.hpp"
#include "payment_result_status.hpp"
#ifdef __ANDROID__
#include "djinni_support.hpp"
#endif

// External libraries
#include <boost/algorithm/string.hpp>
#include <boost/program_options/parsers.hpp>
#include <qrencode.h>
#include <memory>

#include "pow/pow.h"
#include <crypto/hash/sigma/sigma.h>
#include <algorithm>

std::shared_ptr<UnifiedFrontend> signalHandler;

CCriticalSection cs_monitoringListeners;
std::set<std::shared_ptr<MonitorListener> > monitoringListeners;

const int RECOMMENDED_CONFIRMATIONS = 3;
const int BALANCE_NOTIFY_THRESHOLD_MS = 4000;
const int NEW_MUTATIONS_NOTIFY_THRESHOLD_MS = 10000;

boost::asio::io_context ioctx;
boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work = boost::asio::make_work_guard(ioctx);
boost::thread run_thread(boost::bind(&boost::asio::io_context::run, boost::ref(ioctx)));

/* Helper for rate limiting notifications.
 * This helper is agnostic of state like chain sync etc.
 * It is limited by a minimum time that has to pass between notifications. Any updates that come in too fast will
 * be absorbed. Finally the last update is guaranteed to be delivered and will take the latest data.
 * So for example with a rate limit of 3 seconds we get this scenario:
 * trigger(1)
 * trigger(2)
 * wait of 1 seconds
 * trigger(3)
 * trigger(4)
 * wait of 4 seconds
 * trigger(5)
 * trigger(6)
 * will see trigger 1 (or 2 depending on thread scheduling) delivered at t=0s, 4 at t=3s and finally 6 at t=6s.
 */
template <typename EventData>
class CRateLimit
{
public:

    CRateLimit(std::function<void (const EventData&)> _notifier, const std::chrono::milliseconds& _timeLimit):
        notifier(_notifier),
        timeLimit(_timeLimit),
        pending(false),
        lastTimeHandled(std::chrono::steady_clock::now() - timeLimit),
        timer(ioctx) {}

    virtual ~CRateLimit()
    {
        timer.cancel();
    }

    void trigger(const EventData& data)
    {
        std::scoped_lock lock(mtx);
        mostRecentData = data;
        if (!pending) {
            pending = true;
            timer.expires_at(lastTimeHandled + timeLimit);
            timer.async_wait(boost::bind(&CRateLimit::handler, this));
        }
    }

    void handler() {
        EventData data;
        {
            std::scoped_lock lock(mtx);
            data = mostRecentData;
            pending = false;
            lastTimeHandled = std::chrono::steady_clock::now();

        }
        notifier(data); // notify using a copy of event data such that the notifier will not block new triggers
    }

private:
    std::function<void (const EventData&)> notifier;
    boost::asio::steady_timer::duration timeLimit;

    std::mutex mtx;
    bool pending;
    EventData mostRecentData;
    std::chrono::time_point<std::chrono::steady_clock> lastTimeHandled;
    boost::asio::steady_timer timer;
};

TransactionStatus getStatusForTransaction(const CWalletTx* wtx)
{
    TransactionStatus status;
    int depth = wtx->GetDepthInMainChain();
    if (depth < 0)
        status = TransactionStatus::CONFLICTED;
    else if (depth == 0) {
        if (wtx->isAbandoned())
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
    return status;
}

void addMutationsForTransaction(const CWalletTx* wtx, std::vector<MutationRecord>& mutations)
{
    // exclude generated that are orphaned
    if (wtx->IsCoinBase() && wtx->GetDepthInMainChain() < 1)
        return;

    int64_t subtracted = wtx->GetDebit(ISMINE_SPENDABLE, pactiveWallet->activeAccount, true);
    int64_t added = wtx->GetCredit(ISMINE_SPENDABLE, pactiveWallet->activeAccount, true) +
                    wtx->GetImmatureCredit(false, pactiveWallet->activeAccount, true);

    uint64_t time = wtx->nTimeSmart;
    std::string hash = wtx->GetHash().ToString();

    TransactionStatus status = getStatusForTransaction(wtx);
    int depth = wtx->GetDepthInMainChain();

    // if any funds were subtracted the transaction was sent by us
    if (subtracted > 0)
    {
        int64_t fee = subtracted - wtx->tx->GetValueOut();
        int64_t change = wtx->GetChange();

        // detect internal transfer and split it
        if (subtracted - fee == added)
        {
            // amount received
            mutations.push_back(MutationRecord(added - change, time, hash, status, depth));

            // amount send including fee
            mutations.push_back(MutationRecord(change - subtracted, time, hash, status, depth));
        }
        else
        {
            mutations.push_back(MutationRecord(added - subtracted, time, hash, status, depth));
        }
    }
    else if (added != 0) // nothing subtracted so we received funds
    {
        mutations.push_back(MutationRecord(added, time, hash, status, depth));
    }
}

TransactionRecord calculateTransactionRecordForWalletTransaction(const CWalletTx& wtx)
{
    CWallet* pwallet = pactiveWallet;

    std::vector<InputRecord> inputs;
    std::vector<OutputRecord> outputs;


    //fixme: (UNITY) - rather calculate this once and pass it in instead of for every call..
    std::vector<CAccount*> accountsToTry;
    accountsToTry.push_back(pactiveWallet->activeAccount);
    for (const auto& [accountUUID, account] : pactiveWallet->mapAccounts)
    {
        (unused) accountUUID;
        if (account->getParentUUID() == pactiveWallet->activeAccount->getUUID())
        {
            accountsToTry.push_back(account);
        }
    }

    int64_t subtracted=0;
    int64_t added=0;
    for (const auto& account : accountsToTry)
    {
        subtracted += wtx.GetDebit(ISMINE_SPENDABLE, account, true);
        added += wtx.GetCredit(ISMINE_SPENDABLE, account, true);
    }

    CAmount fee = 0;
    // if any funds were subtracted the transaction was sent by us
    if (subtracted > 0)
        fee = subtracted - wtx.tx->GetValueOut();

    const CTransaction& tx = *wtx.tx;

    for (const CTxIn& txin: tx.vin)
    {
        std::string address;
        CNativeAddress addr;
        CTxDestination dest = CNoDestination();

        // Try to extract destination, this is not possible in general. Only if the previous
        // ouput of our input happens to be in our wallet. Which will usually only be the case for
        // our own transactions.

        uint256 txHash;
        if (txin.prevout.isHash)
        {
            txHash = txin.prevout.getTransactionHash();
        }
        else
        {
            if (!pwallet->GetTxHash(txin.prevout, txHash))
            {
                LogPrintf("Transaction with no corresponding hash found, txid [%d] [%d]\n", txin.prevout.getTransactionBlockNumber(), txin.prevout.getTransactionIndex());
                continue;
            }
        }
        std::map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(txHash);
        if (mi != pwallet->mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
            {
                const auto& prevOut =  prev.tx->vout[txin.prevout.n];
                if (!ExtractDestination(prevOut, dest) && !prevOut.IsUnspendable())
                {
                    LogPrintf("Unknown transaction type found, txid %s\n", wtx.GetHash().ToString());
                    dest = CNoDestination();
                }
            }
        }
        if (addr.Set(dest))
        {
            address = addr.ToString();
        }
        std::string label;
        std::string description;
        if (pwallet->mapAddressBook.count(address))
        {
            const auto& data = pwallet->mapAddressBook[address];
            label = data.name;
            description = data.description;
        }
        bool isMine = false;
        for (const auto& account : accountsToTry)
        {
            if (static_cast<const CExtWallet*>(pwallet)->IsMine(*account, txin))
            {
                isMine = true;
            }
        }
        inputs.push_back(InputRecord(address, label, description, isMine));
    }

    for (const CTxOut& txout: tx.vout)
    {
        std::string address;
        CNativeAddress addr;
        CTxDestination dest;
        if (!ExtractDestination(txout, dest) && !txout.IsUnspendable())
        {
            LogPrintf("Unknown transaction type found, txid %s\n", tx.GetHash().ToString());
            dest = CNoDestination();
        }

        if (addr.Set(dest))
        {
            address = addr.ToString();
        }
        std::string label;
        std::string description;
        if (pwallet->mapAddressBook.count(address))
        {
            const auto& data = pwallet->mapAddressBook[address];
            label = data.name;
            description = data.description;
        }
        bool isMine = false;
        for (const auto& account : accountsToTry)
        {
            if (IsMine(*account, txout))
            {
                isMine = true;
            }
        }
        outputs.push_back(OutputRecord(txout.nValue, address, label, description, isMine));
    }

    TransactionStatus status = getStatusForTransaction(&wtx);

    return TransactionRecord(wtx.GetHash().ToString(), wtx.nTimeSmart,
                             added - subtracted,
                             fee, status, wtx.nHeight, wtx.nBlockTime, wtx.GetDepthInMainChain(),
                             inputs, outputs);
}

// rate limited balance change notifier
static CRateLimit<int>* balanceChangeNotifier=nullptr;

// rate limited new mutations notifier
static CRateLimit<std::pair<uint256, bool>>* newMutationsNotifier=nullptr;


void terminateUnityFrontend()
{
    if (signalHandler)
    {
        signalHandler->notifyShutdown();
    }

    // Allow frontend time to clean up and free any references to objects before unloading the library
    // Otherwise we get a free after close (on macOS at least)
    while (signalHandler.use_count() > 1)
    {
        MilliSleep(50);
    }
    signalHandler=nullptr;
}

#include <boost/chrono/thread_clock.hpp>

void handlePostInitMain()
{
    //fixme: (SIGMA) (PHASE4) Remove this once we have witness-header-sync
    // Select appropriate verification factor based on devices performance.
    std::thread([=]
    {
// When available measure thread relative cpu time to avoid effects of thread suspension
// which occur when observing system time.
#if false && defined(BOOST_CHRONO_HAS_THREAD_CLOCK) && BOOST_CHRONO_THREAD_CLOCK_IS_STEADY
        boost::chrono::time_point tpStart = boost::chrono::thread_clock::now();
#else
        uint64_t nStart = GetTimeMicros();
#endif

        // note that measurement is on single thread, which makes the measurement more stable
        // actual verification might use more threads which helps overall app performance
        sigma_verify_context verify(defaultSigmaSettings, 1);
        CBlockHeader header;
        verify.verifyHeader<1>(header);

        // We want at least 1000 blocks per second
#if false && defined(BOOST_CHRONO_HAS_THREAD_CLOCK) && BOOST_CHRONO_THREAD_CLOCK_IS_STEADY
        boost::chrono::microseconds ms  = boost::chrono::duration_cast<boost::chrono::microseconds>(boost::chrono::thread_clock::now() - tpStart);
        uint64_t nTotal = ms.count();
#else
        uint64_t nTotal = GetTimeMicros() - nStart;
#endif
        uint64_t nPerSec = 1000000/nTotal;
        if (nPerSec > 1000) // Fast enough to do most the blocks
        {
            verifyFactor = 5;
        }
        else if(nPerSec > 0) // Slower so reduce the number of blocks
        {
            // 2 in verifyFactor chance of verifying.
            // We verify 2 in verifyFactor blocks - or target_speed/(num_per_sec/2)
            verifyFactor = 1000/(nPerSec/2.0);
            verifyFactor = std::max((uint64_t)5, verifyFactor);
            verifyFactor = std::min((uint64_t)200, verifyFactor);
        }
        LogPrintf("unity: selected verification factor %d", verifyFactor);
    }).detach();

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
        // Fire events for transaction depth changes (up to depth 10 only)
        pactiveWallet->NotifyTransactionDepthChanged.connect( [&](CWallet* pwallet, const uint256& hash)
        {
            DS_LOCK2(cs_main, pwallet->cs_wallet);
            if (pwallet->mapWallet.find(hash) != pwallet->mapWallet.end())
            {
                const CWalletTx& wtx = pwallet->mapWallet[hash];
                LogPrintf("unity: notify transaction depth changed %s",hash.ToString().c_str());
                if (signalHandler)
                {
                    TransactionRecord walletTransaction = calculateTransactionRecordForWalletTransaction(wtx);
                    signalHandler->notifyUpdatedTransaction(walletTransaction);
                }
            }
        } );
        // Fire events for transaction status changes, or new transactions (this won't fire for simple depth changes)
        pactiveWallet->NotifyTransactionChanged.connect( [&](CWallet* pwallet, const uint256& hash, ChangeType status, bool fSelfComitted) {
            DS_LOCK2(cs_main, pwallet->cs_wallet);
            if (pwallet->mapWallet.find(hash) != pwallet->mapWallet.end())
            {
                if (status == CT_NEW) {
                    newMutationsNotifier->trigger(std::make_pair(hash, fSelfComitted));
                }
                else if (status == CT_UPDATED && signalHandler)
                {
                    LogPrintf("unity: notify tx updated %s",hash.ToString().c_str());
                    const CWalletTx& wtx = pwallet->mapWallet[hash];
                    TransactionRecord walletTransaction = calculateTransactionRecordForWalletTransaction(wtx);
                    signalHandler->notifyUpdatedTransaction(walletTransaction);
                }
                //fixme: (UNITY) - Consider implementing f.e.x if a 0 conf transaction gets deleted...
                // else if (status == CT_DELETED)
            }
            balanceChangeNotifier->trigger(0);
        });

        // Fire once immediately to update with latest on load.
        balanceChangeNotifier->trigger(0);
    }
}

void handleInitWithExistingWallet()
{
    if (signalHandler)
    {
        signalHandler->notifyInitWithExistingWallet();
    }
    AppLifecycleManager::gApp->initialize();
}

void handleInitWithoutExistingWallet()
{
    signalHandler->notifyInitWithoutExistingWallet();
}

std::string UnifiedBackend::BuildInfo()
{
    std::string info = FormatFullVersion();

#if defined(__aarch64__)
    info += " aarch64";
#elif defined(__arm__)
    info += " arm (32bit)";
#elif defined(__x86_64__)
    info += " x86_64";
#elif defined(__i386__)
    info += " x86";
#endif
    return info;
}

bool UnifiedBackend::InitWalletFromRecoveryPhrase(const std::string& phrase, const std::string& password)
{
    // Refuse to acknowledge an empty recovery phrase, or one that doesn't pass even the most obvious requirement
    if (phrase.length() < 16)
    {
        return false;
    }

    //fixme: (UNITY) (SPV) - Handle all the various birth date (or lack of birthdate) cases here instead of just the one.
    SecureString phraseOnly;
    int phraseBirthNumber = 0;
    AppLifecycleManager::gApp->splitRecoveryPhraseAndBirth(phrase.c_str(), phraseOnly, phraseBirthNumber);
    if (!checkMnemonic(phraseOnly))
    {
        return false;
    }

    // ensure that wallet is initialized with a starting time (else it will start from now and old tx will not be scanned)
    // Use the hardcoded timestamp 1441212522 of block 250000, we didn't have any recovery phrase style wallets (using current phrase system) before that.
    if (phraseBirthNumber == 0)
        phraseBirthNumber = timeToBirthNumber(1441212522L);

    //fixme: (UNITY) (SPV) - Handle all the various birth date (or lack of birthdate) cases here instead of just the one.
    AppLifecycleManager::gApp->setRecoveryPhrase(phraseOnly);
    AppLifecycleManager::gApp->setRecoveryBirthNumber(phraseBirthNumber);
    AppLifecycleManager::gApp->setRecoveryPassword(password.c_str());
    AppLifecycleManager::gApp->isRecovery = true;
    AppLifecycleManager::gApp->initialize();

    return true;
}

bool ValidateAndSplitRecoveryPhrase(const std::string & phrase, SecureString& mnemonic, int& birthNumber)
{
    if (phrase.length() < 16)
        return false;

    AppLifecycleManager::gApp->splitRecoveryPhraseAndBirth(phrase.c_str(), mnemonic, birthNumber);
    return checkMnemonic(mnemonic) && (birthNumber == 0 || Base10ChecksumDecode(birthNumber, nullptr));
}

bool UnifiedBackend::ContinueWalletFromRecoveryPhrase(const std::string& phrase, const std::string& password)
{
    SecureString phraseOnly;
    int phraseBirthNumber;

    if (!ValidateAndSplitRecoveryPhrase(phrase, phraseOnly, phraseBirthNumber))
        return false;

    // ensure that wallet is initialized with a starting time (else it will start from now and old tx will not be scanned)
    // Use the hardcoded timestamp 1441212522 of block 250000, we didn't have any recovery phrase style wallets (using current phrase system) before that.
    if (phraseBirthNumber == 0)
        phraseBirthNumber = timeToBirthNumber(1441212522L);

    if (!pactiveWallet)
    {
        LogPrintf("ContineWalletFromRecoveryPhrase: No active wallet");
        return false;
    }

    LOCK2(cs_main, pactiveWallet->cs_wallet);
    AppLifecycleManager::gApp->setRecoveryPhrase(phraseOnly);
    AppLifecycleManager::gApp->setRecoveryBirthNumber(phraseBirthNumber);
    AppLifecycleManager::gApp->setRecoveryPassword(password.c_str());
    AppLifecycleManager::gApp->isRecovery = true;

    CWallet::CreateSeedAndAccountFromPhrase(pactiveWallet);

    // Allow update of balance for deleted accounts/transactions
    LogPrintf("%s: Update balance and rescan", __func__);
    balanceChangeNotifier->trigger(0);

    // Rescan for transactions on the linked account
    DoRescan();

    return true;
}

bool UnifiedBackend::IsValidRecoveryPhrase(const std::string & phrase)
{
    SecureString dummyMnemonic;
    int dummyNumber;
    return ValidateAndSplitRecoveryPhrase(phrase, dummyMnemonic, dummyNumber);
}

#include "base58.h"
std::string UnifiedBackend::GenerateGenesisKeys()
{
    std::string address = GetReceiveAddress();
    CNativeAddress addr(address);
    CTxDestination dest = addr.Get();
    CPubKey vchPubKeyDevSubsidy;
    pactiveWallet->GetPubKey(boost::get<CKeyID>(dest), vchPubKeyDevSubsidy);
    std::string devSubsidyPubKey = HexStr(vchPubKeyDevSubsidy);

    CKey key;
    key.MakeNewKey(true);    
    CPrivKey vchPrivKey = key.GetPrivKey();
    CPubKey vchPubKey = key.GetPubKey();
    std::string privkey = HexStr<CPrivKey::iterator>(vchPrivKey.begin(), vchPrivKey.end()).c_str();
    std::string pubKey = vchPubKey.GetID().GetHex();
    std::string witnessKeys = GLOBAL_APP_URIPREFIX"://witnesskeys?keys=" + CEncodedSecretKey(key).ToString() + strprintf("#%s", GetAdjustedTime());
    
    return "privkey: "+privkey+"\n"+"pubkeyID: "+pubKey+"\n"+"witness: "+witnessKeys+"\n"+"dev subsidy addr: "+address+"\n"+"dev subsidy pubkey: "+devSubsidyPubKey+"\n";
}

std::string UnifiedBackend::GenerateRecoveryMnemonic()
{
    std::vector<unsigned char> entropy(16);
    GetStrongRandBytes(&entropy[0], 16);
    #if 0
    int64_t birthTime = GetAdjustedTime();
    #else
    int64_t birthTime = 0;
    #endif
    return AppLifecycleManager::gApp->composeRecoveryPhrase(mnemonicFromEntropy(entropy, entropy.size()*8), birthTime).c_str();
}

std::string UnifiedBackend::ComposeRecoveryPhrase(const std::string & mnemonic, int64_t birthTime)
{
    return std::string(AppLifecycleManager::composeRecoveryPhrase(SecureString(mnemonic), birthTime));
}

bool UnifiedBackend::InitWalletLinkedFromURI(const std::string& linked_uri, const std::string& password)
{
    CEncodedSecretKeyExt<CExtKey> linkedKey;
    if (!linkedKey.fromURIString(linked_uri))
    {
        return false;
    }
    AppLifecycleManager::gApp->setLinkKey(linkedKey);
    AppLifecycleManager::gApp->isLink = true;
    AppLifecycleManager::gApp->setRecoveryPassword(password.c_str());
    AppLifecycleManager::gApp->initialize();

    return true;
}

bool UnifiedBackend::ContinueWalletLinkedFromURI(const std::string & linked_uri, const std::string& password)
{
    if (!pactiveWallet)
    {
        LogPrintf("%s: No active wallet", __func__);
        return false;
    }

    LOCK2(cs_main, pactiveWallet->cs_wallet);

    CEncodedSecretKeyExt<CExtKey> linkedKey;
    if (!linkedKey.fromURIString(linked_uri))
    {
        LogPrintf("%s: Failed to parse link URI", __func__);
        return false;
    }

    AppLifecycleManager::gApp->setLinkKey(linkedKey);
    AppLifecycleManager::gApp->setRecoveryPassword(password.c_str());
    AppLifecycleManager::gApp->isLink = true;

    CWallet::CreateSeedAndAccountFromLink(pactiveWallet);

    // Allow update of balance for deleted accounts/transactions
    LogPrintf("%s: Update balance and rescan", __func__);
    balanceChangeNotifier->trigger(0);

    // Rescan for transactions on the linked account
    DoRescan();

    return true;
}

bool UnifiedBackend::ReplaceWalletLinkedFromURI(const std::string& linked_uri, const std::string& password)
{
    LOCK2(cs_main, pactiveWallet->cs_wallet);

    if (!pactiveWallet || !pactiveWallet->activeAccount)
    {
        LogPrintf("ReplaceWalletLinkedFromURI: No active wallet");
        return false;
    }

    // Create ext key for new linked account from parsed data
    CEncodedSecretKeyExt<CExtKey> linkedKey;
    if (!linkedKey.fromURIString(linked_uri))
    {
        LogPrintf("ReplaceWalletLinkedFromURI: Failed to parse link URI");
        return false;
    }

    // Ensure we have a valid location to send all the funds
    CNativeAddress address(linkedKey.getPayAccount());
    if (!address.IsValid())
    {
        LogPrintf("ReplaceWalletLinkedFromURI: invalid address %s", linkedKey.getPayAccount().c_str());
        return false;
    }

    // Empty wallet to target address
    LogPrintf("ReplaceWalletLinkedFromURI: Empty accounts into linked address");
    bool fSubtractFeeFromAmount = true;
    std::vector<std::tuple<CWalletTx*, CReserveKeyOrScript*>> transactionsToCommit;
    for (const auto& [accountUUID, pAccount] : pactiveWallet->mapAccounts)
    {
        CAmount nBalance = pactiveWallet->GetBalance(pAccount, false, true, true);
        if (nBalance > 0)
        {
            LogPrintf("ReplaceWalletLinkedFromURI: Empty account into linked address [%s]", getUUIDAsString(accountUUID).c_str());
            std::vector<CRecipient> vecSend;
            CRecipient recipient = GetRecipientForDestination(address.Get(), nBalance, fSubtractFeeFromAmount, GetPoW2Phase(chainTip()));
            vecSend.push_back(recipient);

            CWalletTx* pWallettx = new CWalletTx();
            CAmount nFeeRequired;
            int nChangePosRet = -1;
            std::string strError;
            CReserveKeyOrScript* pReserveKey = new CReserveKeyOrScript(pactiveWallet, pAccount, KEYCHAIN_CHANGE);
            std::vector<CKeyStore*> accountsToTry;
            for ( const auto& accountPair : pactiveWallet->mapAccounts )
            {
                if(accountPair.second->getParentUUID() == pAccount->getUUID())
                {
                    accountsToTry.push_back(accountPair.second);
                }
                accountsToTry.push_back(pAccount);
            }
            if (!pactiveWallet->CreateTransaction(accountsToTry, vecSend, *pWallettx, *pReserveKey, nFeeRequired, nChangePosRet, strError))
            {
                LogPrintf("ReplaceWalletLinkedFromURI: Failed to create transaction %s [%d]",strError.c_str(), nBalance);
                return false;
            }
            transactionsToCommit.push_back(std::tuple(pWallettx, pReserveKey));
        }
        else
        {
            LogPrintf("ReplaceWalletLinkedFromURI: Account already empty [%s]", getUUIDAsString(accountUUID).c_str());
        }
    }

    if (!EraseWalletSeedsAndAccounts())
    {
        LogPrintf("ReplaceWalletLinkedFromURI: Failed to erase seed and accounts");
        return false;
    }

    AppLifecycleManager::gApp->setLinkKey(linkedKey);
    AppLifecycleManager::gApp->setRecoveryPassword(password.c_str());
    AppLifecycleManager::gApp->isLink = true;

    CWallet::CreateSeedAndAccountFromLink(pactiveWallet);

    for (auto& [pWalletTx, pReserveKey] : transactionsToCommit)
    {
        CValidationState state;
        //NB! We delibritely pass nullptr for connman here to prevent transaction from relaying
        //We allow the relaying to occur inside DoRescan instead
        if (!pactiveWallet->CommitTransaction(*pWalletTx, *pReserveKey, nullptr, state))
        {
            LogPrintf("ReplaceWalletLinkedFromURI: Failed to commit transaction");
            return false;
        }
        delete pWalletTx;
        delete pReserveKey;
    }

    // Allow update of balance for deleted accounts/transactions
    LogPrintf("ReplaceWalletLinkedFromURI: Update balance and rescan");
    balanceChangeNotifier->trigger(0);

    // Rescan for transactions on the linked account
    DoRescan();

    return true;
}

bool UnifiedBackend::EraseWalletSeedsAndAccounts()
{
    pactiveWallet->EraseWalletSeedsAndAccounts();
    return true;
}

bool UnifiedBackend::IsValidLinkURI(const std::string& linked_uri)
{
    CEncodedSecretKeyExt<CExtKey> linkedKey;
    if (!linkedKey.fromURIString(linked_uri))
        return false;
    return true;
}


int32_t UnifiedBackend::InitUnityLib(const std::string& dataDir, const std::string& staticFilterPath, int64_t staticFilterOffset, int64_t staticFilterLength, bool testnet, bool spvMode, const std::shared_ptr<UnifiedFrontend>& signalHandler_, const std::string& extraArgs)
{
    balanceChangeNotifier = new CRateLimit<int>([](int)
    {
        if (pactiveWallet && signalHandler)
        {
            WalletBalances balances;
            pactiveWallet->GetBalances(balances, pactiveWallet->activeAccount, true);
            signalHandler->notifyBalanceChange(BalanceRecord(balances.availableIncludingLocked, balances.availableExcludingLocked, balances.availableLocked, balances.unconfirmedIncludingLocked, balances.unconfirmedExcludingLocked, balances.unconfirmedLocked, balances.immatureIncludingLocked, balances.immatureExcludingLocked, balances.immatureLocked, balances.totalLocked));
        }
    }, std::chrono::milliseconds(BALANCE_NOTIFY_THRESHOLD_MS));

    newMutationsNotifier = new CRateLimit<std::pair<uint256, bool>>([](const std::pair<uint256, bool>& txInfo)
    {
        if (pactiveWallet && signalHandler)
        {
            const uint256& txHash = txInfo.first;
            const bool fSelfComitted = txInfo.second;

            LOCK2(cs_main, pactiveWallet->cs_wallet);
            if (pactiveWallet->mapWallet.find(txHash) != pactiveWallet->mapWallet.end())
            {
                const CWalletTx& wtx = pactiveWallet->mapWallet[txHash];
                std::vector<MutationRecord> mutations;
                addMutationsForTransaction(&wtx, mutations);
                for (auto& m: mutations)
                {
                    LogPrintf("unity: notify new mutation for tx %s", txHash.ToString().c_str());
                    signalHandler->notifyNewMutation(m, fSelfComitted);
                }
            }
        }
    }, std::chrono::milliseconds(NEW_MUTATIONS_NOTIFY_THRESHOLD_MS));

    // Force the datadir to specific place on e.g. android devices
    if (!dataDir.empty())
        SoftSetArg("-datadir", dataDir);

    if (spvMode)
    {
        // SPV wallets definitely shouldn't be listening for incoming connections at all
        SoftSetArg("-listen", "0");

        // Minimise logging for performance reasons
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
        
        //fixme: (FUT) (UNITY) Reverse headers
        // Temporarily disable reverse headers for mobile until memory requirements can be reduced.
        SoftSetArg("-reverseheaders", "false");
    }
    
    SoftSetArg("-spvstaticfilterfile", staticFilterPath);
    SoftSetArg("-spvstaticfilterfileoffset", i64tostr(staticFilterOffset));
    SoftSetArg("-spvstaticfilterfilelength", i64tostr(staticFilterLength));

    // Change client name
#if defined(__APPLE__) && TARGET_OS_IPHONE == 1
    SoftSetArg("-clientname", GLOBAL_APPNAME" ios");
#elif defined(__ANDROID__)
    SoftSetArg("-clientname", GLOBAL_APPNAME" android");
#else
    SoftSetArg("-clientname", GLOBAL_APPNAME" desktop");
#endif

    SoftSetArg("-addnode", "devbak.net");
    SoftSetArg("-addnode", "devbak.net");
    
    // Testnet
    if (testnet)
    {
        SoftSetArg("-testnet", "C1534687770:60");
        SoftSetArg("-addnode", "devbak.net");
    }

    signalHandler = signalHandler_;

    if (!extraArgs.empty()) {
        std::vector<const char*> args;
        auto splitted = boost::program_options::split_unix(extraArgs);
        for(const auto& part: splitted)
            args.push_back(part.c_str());
        gArgs.ParseExtraParameters(int(args.size()), args.data());
    }

    return InitUnity();
}

void UnifiedBackend::InitUnityLibThreaded(const std::string& dataDir, const std::string& staticFilterPath, int64_t staticFilterOffset, int64_t staticFilterLength, bool testnet, bool spvMode, const std::shared_ptr<UnifiedFrontend>& signalHandler_, const std::string& extraArgs)
{
    std::thread([=]
    {
        InitUnityLib(dataDir, staticFilterPath, staticFilterOffset, staticFilterLength, testnet, spvMode, signalHandler_, extraArgs);
    }).detach();
}

void UnifiedBackend::TerminateUnityLib()
{
    // Terminate in thread so we don't block interprocess communication
    std::thread([=]
    {
        work.reset();
        ioctx.stop();
        AppLifecycleManager::gApp->shutdown();
        AppLifecycleManager::gApp->waitForShutDown();
        run_thread.join();
    }).detach();
}

QrCodeRecord UnifiedBackend::QRImageFromString(const std::string& qr_string, int32_t width_hint)
{
    QRcode* code = QRcode_encodeString(qr_string.c_str(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!code)
    {
        return QrCodeRecord(0, std::vector<uint8_t>());
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
        return QrCodeRecord(finalWidth, dataVector);
    }
}

std::string UnifiedBackend::GetReceiveAddress()
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
        return CNativeAddress(keyID).ToString();
    }
    else
    {
        return "";
    }
}

//fixme: (UNITY) - find a way to use char[] here as well as on the java side.
std::string UnifiedBackend::GetRecoveryPhrase()
{
    if (!pactiveWallet || !pactiveWallet->activeAccount)
        return "";

    LOCK2(cs_main, pactiveWallet->cs_wallet);
    //WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    //if (ctx.isValid())
    {
        #if 0
        int64_t birthTime = pactiveWallet->birthTime();
        #else
        int64_t birthTime = 0;
        #endif

        std::set<SecureString> allPhrases;
        for (const auto& seedIter : pactiveWallet->mapSeeds)
        {
            return AppLifecycleManager::composeRecoveryPhrase(seedIter.second->getMnemonic(), birthTime).c_str();
        }
    }
    return "";
}

bool UnifiedBackend::IsMnemonicWallet()
{
    if (!pactiveWallet || !pactiveWallet->activeAccount)
        throw std::runtime_error(_("No active internal wallet."));

    LOCK2(cs_main, pactiveWallet->cs_wallet);

    return pactiveWallet->activeSeed != nullptr;
}

bool UnifiedBackend::IsMnemonicCorrect(const std::string & phrase)
{
    if (!pactiveWallet || !pactiveWallet->activeAccount)
        throw std::runtime_error(_("No active internal wallet."));

    SecureString mnemonicPhrase;
    int birthNumber;

    AppLifecycleManager::splitRecoveryPhraseAndBirth(SecureString(phrase), mnemonicPhrase, birthNumber);

    LOCK2(cs_main, pactiveWallet->cs_wallet);

    for (const auto& seedIter : pactiveWallet->mapSeeds)
    {
        if (mnemonicPhrase == seedIter.second->getMnemonic())
            return true;
    }
    return false;
}


//fixme: (UNITY) HIGH - take a timeout value and always lock again after timeout
bool UnifiedBackend::UnlockWallet(const std::string& password)
{
    if (!pactiveWallet)
    {
        LogPrintf("UnlockWallet: No active wallet");
        return false;
    }

    if (!dynamic_cast<CExtWallet*>(pactiveWallet)->IsCrypted())
    {
        LogPrintf("UnlockWallet: Wallet not encrypted");
        return false;
    }

    return pactiveWallet->Unlock(password.c_str());
}

bool UnifiedBackend::LockWallet()
{
    if (!pactiveWallet)
    {
        LogPrintf("LockWallet: No active wallet");
        return false;
    }

    if (dynamic_cast<CExtWallet*>(pactiveWallet)->IsLocked())
        return true;

    return dynamic_cast<CExtWallet*>(pactiveWallet)->Lock();
}

bool UnifiedBackend::ChangePassword(const std::string& oldPassword, const std::string& newPassword)
{
    if (!pactiveWallet)
    {
        LogPrintf("ChangePassword: No active wallet");
        return false;
    }

    if (newPassword.length() == 0)
    {
        LogPrintf("ChangePassword: Refusing invalid password of length 0");
        return false;
    }

    return pactiveWallet->ChangeWalletPassphrase(oldPassword.c_str(), newPassword.c_str());
}

bool UnifiedBackend::HaveUnconfirmedFunds()
{
    if (!pactiveWallet)
        return true;

    WalletBalances balances;
    pactiveWallet->GetBalances(balances, pactiveWallet->activeAccount, true);

    if (balances.unconfirmedIncludingLocked > 0 || balances.immatureIncludingLocked > 0)
    {
        return true;
    }
    return false;
}

int64_t UnifiedBackend::GetBalance()
{
    if (!pactiveWallet)
        return 0;

    WalletBalances balances;
    pactiveWallet->GetBalances(balances, pactiveWallet->activeAccount, true);
    return balances.availableIncludingLocked + balances.unconfirmedIncludingLocked + balances.immatureIncludingLocked;
}

void UnifiedBackend::DoRescan()
{
    if (pactiveWallet)
    {
        ResetSPVStartRescanThread();
    }
}

UriRecipient UnifiedBackend::IsValidRecipient(const UriRecord & request)
{
     // return if URI is not valid or is no Gulden: URI
    std::string lowerCaseScheme = boost::algorithm::to_lower_copy(request.scheme);
    if (lowerCaseScheme != "guldencoin" && lowerCaseScheme != "gulden")
        return UriRecipient(false, "", "", "", 0);

    if (!CNativeAddress(request.path).IsValid())
        return UriRecipient(false, "", "", "", 0);

    std::string address = request.path;
    std::string label = "";
    std::string description = "";
    CAmount amount = 0;
    if (request.items.find("amount") != request.items.end())
    {
        ParseMoney(request.items.find("amount")->second, amount);
    }

    if (pactiveWallet)
    {
        DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
        if (pactiveWallet->mapAddressBook.find(address) != pactiveWallet->mapAddressBook.end())
        {
            const auto& data = pactiveWallet->mapAddressBook[address];
            label = data.name;
            description = data.description;
        }
    }

    return UriRecipient(true, address, label, description, amount);
}


int64_t UnifiedBackend::feeForRecipient(const UriRecipient & request)
{
    if (!pactiveWallet)
        throw std::runtime_error(_("No active internal wallet."));

    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);

    CNativeAddress address(request.address);
    if (!address.IsValid())
    {
        LogPrintf("feeForRecipient: invalid address %s", request.address.c_str());
        throw std::runtime_error(_("Invalid address"));
    }

    CRecipient recipient = GetRecipientForDestination(address.Get(), std::min(GetBalance(), request.amount), true, GetPoW2Phase(chainTip()));
    std::vector<CRecipient> vecSend;
    vecSend.push_back(recipient);

    CWalletTx wtx;
    CAmount nFeeRequired;
    int nChangePosRet = -1;
    std::string strError;
    CReserveKeyOrScript reservekey(pactiveWallet, pactiveWallet->activeAccount, KEYCHAIN_CHANGE);
    std::vector<CKeyStore*> accountsToTry;
    for ( const auto& accountPair : pactiveWallet->mapAccounts )
    {
        if(accountPair.second->getParentUUID() == pactiveWallet->activeAccount->getUUID())
        {
            accountsToTry.push_back(accountPair.second);
        }
        accountsToTry.push_back(pactiveWallet->activeAccount);
    }
    if (!pactiveWallet->CreateTransaction(accountsToTry, vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError, NULL, false))
    {
        LogPrintf("feeForRecipient: failed to create transaction %s",strError.c_str());
        throw std::runtime_error(strprintf(_("Failed to calculate fee\n%s"), strError));
    }

    return nFeeRequired;
}

PaymentResultStatus UnifiedBackend::performPaymentToRecipient(const UriRecipient & request, bool substract_fee)
{
    if (!pactiveWallet)
        throw std::runtime_error(_("No active internal wallet."));

    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);

    CNativeAddress address(request.address);
    if (!address.IsValid())
    {
        LogPrintf("performPaymentToRecipient: invalid address %s", request.address.c_str());
        throw std::runtime_error(_("Invalid address"));
    }

    CRecipient recipient = GetRecipientForDestination(address.Get(), request.amount, substract_fee, GetPoW2Phase(chainTip()));
    std::vector<CRecipient> vecSend;
    vecSend.push_back(recipient);

    CWalletTx wtx;
    CAmount nFeeRequired;
    int nChangePosRet = -1;
    std::string strError;
    CReserveKeyOrScript reservekey(pactiveWallet, pactiveWallet->activeAccount, KEYCHAIN_CHANGE);
    std::vector<CKeyStore*> accountsToTry;
    for ( const auto& accountPair : pactiveWallet->mapAccounts )
    {
        if(accountPair.second->getParentUUID() == pactiveWallet->activeAccount->getUUID())
        {
            accountsToTry.push_back(accountPair.second);
        }
        accountsToTry.push_back(pactiveWallet->activeAccount);
    }
    if (!pactiveWallet->CreateTransaction(accountsToTry, vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError))
    {
        if (!substract_fee && request.amount + nFeeRequired > GetBalance()) {
            return PaymentResultStatus::INSUFFICIENT_FUNDS;
        }
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

    return PaymentResultStatus::SUCCESS;
}

std::vector<TransactionRecord> UnifiedBackend::getTransactionHistory()
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
    std::sort(ret.begin(), ret.end(), [&](TransactionRecord& x, TransactionRecord& y){ return (x.timeStamp > y.timeStamp); });
    return ret;
}

TransactionRecord UnifiedBackend::getTransaction(const std::string & txHash)
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

std::vector<MutationRecord> UnifiedBackend::getMutationHistory()
{
    std::vector<MutationRecord> ret;

    if (!pactiveWallet)
        return ret;

    DS_LOCK2(cs_main, pactiveWallet->cs_wallet);

    // wallet transactions in reverse chronological ordering
    std::vector<const CWalletTx*> vWtx;
    for (const auto& [hash, wtx] : pactiveWallet->mapWallet)
    {
        vWtx.push_back(&wtx);
    }
    std::sort(vWtx.begin(), vWtx.end(), [&](const CWalletTx* x, const CWalletTx* y){ return (x->nTimeSmart > y->nTimeSmart); });

    // build mutation list based on transactions
    for (const CWalletTx* wtx : vWtx)
    {
        addMutationsForTransaction(wtx, ret);
    }

    return ret;
}

std::vector<AddressRecord> UnifiedBackend::getAddressBookRecords()
{
    std::vector<AddressRecord> ret;
    if (pactiveWallet)
    {
        DS_LOCK2(cs_main, pactiveWallet->cs_wallet);
        for(const auto& [address, addressData] : pactiveWallet->mapAddressBook)
        {
            ret.emplace_back(AddressRecord(address, addressData.name, addressData.description, addressData.purpose));
        }
    }

    return ret;
}

void UnifiedBackend::addAddressBookRecord(const AddressRecord& address)
{
    if (pactiveWallet)
    {
        pactiveWallet->SetAddressBook(address.address, address.name, address.desc, address.purpose);
    }
}

void UnifiedBackend::deleteAddressBookRecord(const AddressRecord& address)
{
    if (pactiveWallet)
    {
        pactiveWallet->DelAddressBook(address.address);
    }
}

void UnifiedBackend::PersistAndPruneForSPV()
{
    PersistAndPruneForPartialSync();
}

void UnifiedBackend::ResetUnifiedProgress()
{
    CWallet::ResetUnifiedSPVProgressNotification();
}

float UnifiedBackend::getUnifiedProgress()
{
    return CSPVScanner::lastProgressReported;
}

std::vector<PeerRecord> UnifiedBackend::getPeers()
{
    std::vector<PeerRecord> ret;

    if (g_connman) {
        std::vector<CNodeStats> vstats;
        g_connman->GetNodeStats(vstats);
        for (CNodeStats& nstat: vstats)
        {
            int64_t nSyncedHeight = 0;
            int64_t nCommonHeight = 0;
            CNodeStateStats stateStats;
            TRY_LOCK(cs_main, lockMain);
            if (lockMain && GetNodeStateStats(nstat.nodeid, stateStats))
            {
                nSyncedHeight = stateStats.nSyncHeight;
                nCommonHeight = stateStats.nCommonHeight;
            }

            PeerRecord rec(nstat.nodeid,
                           nstat.addr.ToString(),
                           nstat.addr.HostnameLookup(),
                           nstat.nStartingHeight,
                           nSyncedHeight,
                           nCommonHeight,
                           int32_t(nstat.dPingTime * 1000),
                           nstat.cleanSubVer,
                           nstat.nVersion);
            ret.emplace_back(rec);
        }
    }

    return ret;
}

std::vector<BlockInfoRecord> UnifiedBackend::getLastSPVBlockInfos()
{
    std::vector<BlockInfoRecord> ret;

    LOCK(cs_main);

    int height = partialChain.Height();
    while (ret.size() < 32 && height > partialChain.HeightOffset()) {
        const CBlockIndex* pindex = partialChain[height];
        ret.push_back(BlockInfoRecord(pindex->nHeight, pindex->GetBlockTime(), pindex->GetBlockHashPoW2().ToString()));
        height--;
    }

    return ret;
}

MonitorRecord UnifiedBackend::getMonitoringStats()
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

void UnifiedBackend::RegisterMonitorListener(const std::shared_ptr<MonitorListener> & listener)
{
    LOCK(cs_monitoringListeners);
    monitoringListeners.insert(listener);
}

void UnifiedBackend::UnregisterMonitorListener(const std::shared_ptr<MonitorListener> & listener)
{
    LOCK(cs_monitoringListeners);
    monitoringListeners.erase(listener);
}
