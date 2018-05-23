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

#include "validation.h"
#include "net.h"
#include "scheduler.h"
#include "timedata.h"
#include "utilmoneystr.h"
#include "init.h"
#include <unity/appmanager.h>
#include <Gulden/mnemonic.h>


DBErrors CWallet::LoadWallet(WalletLoadState& nExtraLoadState)
{
    nExtraLoadState = NEW_WALLET;
    DBErrors nLoadWalletRet = CWalletDB(*dbw,"cr+").LoadWallet(this, nExtraLoadState);
    if (nLoadWalletRet == DB_NEED_REWRITE)
    {
        if (dbw->Rewrite("\x04pool"))
        {
            LOCK(cs_wallet);
            //fixme: (Post-2.1)
            for (auto accountPair : mapAccounts)
            {
                accountPair.second->setKeyPoolInternal.clear();
                accountPair.second->setKeyPoolExternal.clear();
            }
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;

    uiInterface.LoadWallet(this);

    return DB_LOAD_OK;
}

std::string CWallet::GetWalletHelpString(bool showDebug)
{
    std::string strUsage = HelpMessageGroup(_("Wallet options:"));
    strUsage += HelpMessageOpt("-disablewallet", _("Do not load the wallet and disable wallet RPC calls"));
    strUsage += HelpMessageOpt("-disableui", _("Load the wallet in a special console only mode"));
    strUsage += HelpMessageOpt("-keypool=<n>", strprintf(_("Set key pool size to <n> (default: %u)"), DEFAULT_KEYPOOL_SIZE));
    strUsage += HelpMessageOpt("-accountpool=<n>", strprintf(_("Set account pool size to <n> (default: %u)"), 10));
    strUsage += HelpMessageOpt("-fallbackfee=<amt>", strprintf(_("A fee rate (in %s/kB) that will be used when fee estimation has insufficient data (default: %s)"),
                                                               CURRENCY_UNIT, FormatMoney(DEFAULT_FALLBACK_FEE)));
    strUsage += HelpMessageOpt("-mintxfee=<amt>", strprintf(_("Fees (in %s/kB) smaller than this are considered zero fee for transaction creation (default: %s)"),
                                                            CURRENCY_UNIT, FormatMoney(DEFAULT_TRANSACTION_MINFEE)));
    strUsage += HelpMessageOpt("-paytxfee=<amt>", strprintf(_("Fee (in %s/kB) to add to transactions you send (default: %s)"),
                                                            CURRENCY_UNIT, FormatMoney(payTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-rescan", _("Rescan the block chain for missing wallet transactions on startup"));
    strUsage += HelpMessageOpt("-salvagewallet", _("Attempt to recover private keys from a corrupt wallet on startup"));
    strUsage += HelpMessageOpt("-spendzeroconfchange", strprintf(_("Spend unconfirmed change when sending transactions (default: %u)"), DEFAULT_SPEND_ZEROCONF_CHANGE));
    strUsage += HelpMessageOpt("-txconfirmtarget=<n>", strprintf(_("If paytxfee is not set, include enough fee so transactions begin confirmation on average within n blocks (default: %u)"), DEFAULT_TX_CONFIRM_TARGET));
    strUsage += HelpMessageOpt("-usehd", _("Use hierarchical deterministic key generation (HD) after BIP32. Only has effect during wallet creation/first start") + " " + strprintf(_("(default: %u)"), DEFAULT_USE_HD_WALLET));
    strUsage += HelpMessageOpt("-walletrbf", strprintf(_("Send transactions with full-RBF opt-in enabled (default: %u)"), DEFAULT_WALLET_RBF));
    strUsage += HelpMessageOpt("-upgradewallet", _("Upgrade wallet to latest format on startup"));
    strUsage += HelpMessageOpt("-wallet=<file>", _("Specify wallet file (within data directory)") + " " + strprintf(_("(default: %s)"), DEFAULT_WALLET_DAT));
    strUsage += HelpMessageOpt("-walletbroadcast", _("Make the wallet broadcast transactions") + " " + strprintf(_("(default: %u)"), DEFAULT_WALLETBROADCAST));
    strUsage += HelpMessageOpt("-walletnotify=<cmd>", _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)"));
    strUsage += HelpMessageOpt("-zapwallettxes=<mode>", _("Delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup") +
                               " " + _("(1 = keep tx meta data e.g. account owner and payment request information, 2 = drop tx meta data)"));

    if (showDebug)
    {
        strUsage += HelpMessageGroup(_("Wallet debugging/testing options:"));

        strUsage += HelpMessageOpt("-dblogsize=<n>", strprintf("Flush wallet database activity from memory to disk log every <n> megabytes (default: %u)", DEFAULT_WALLET_DBLOGSIZE));
        strUsage += HelpMessageOpt("-flushwallet", strprintf("Run a thread to flush wallet periodically (default: %u)", DEFAULT_FLUSHWALLET));
        strUsage += HelpMessageOpt("-privdb", strprintf("Sets the DB_PRIVATE flag in the wallet db environment (default: %u)", DEFAULT_WALLET_PRIVDB));
        strUsage += HelpMessageOpt("-walletrejectlongchains", strprintf(_("Wallet will not create transactions that violate mempool chain limits (default: %u)"), DEFAULT_WALLET_REJECT_LONG_CHAINS));
    }

    return strUsage;
}

CWallet* CWallet::CreateWalletFromFile(const std::string walletFile)
{
    // needed to restore wallet transaction meta data after -zapwallettxes
    std::vector<CWalletTx> vWtx;

    if (GetBoolArg("-zapwallettxes", false)) {
        uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

        std::unique_ptr<CWalletDBWrapper> dbw(new CWalletDBWrapper(&bitdb, walletFile));
        CWallet *tempWallet = new CWallet(std::move(dbw));
        DBErrors nZapWalletRet = tempWallet->ZapWalletTx(vWtx);
        if (nZapWalletRet != DB_LOAD_OK) {
            InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return NULL;
        }

        delete tempWallet;
        tempWallet = NULL;
    }

    uiInterface.InitMessage(_("Loading wallet..."));

    int64_t nStart = GetTimeMillis();
    WalletLoadState loadState = NEW_WALLET;
    std::unique_ptr<CWalletDBWrapper> dbw(new CWalletDBWrapper(&bitdb, walletFile));
    CWallet *walletInstance = new CWallet(std::move(dbw));
    DBErrors nLoadWalletRet = walletInstance->LoadWallet(loadState);
    if (nLoadWalletRet != DB_LOAD_OK)
    {
        if (nLoadWalletRet == DB_CORRUPT) {
            InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return NULL;
        }
        else if (nLoadWalletRet == DB_NONCRITICAL_ERROR)
        {
            InitWarning(strprintf(_("Error reading %s! All keys read correctly, but transaction data"
                                         " or address book entries might be missing or incorrect."),
                walletFile));
        }
        else if (nLoadWalletRet == DB_TOO_NEW) {
            InitError(strprintf(_("Error loading %s: Wallet requires newer version of %s"), walletFile, _(PACKAGE_NAME)));
            return NULL;
        }
        else if (nLoadWalletRet == DB_NEED_REWRITE)
        {
            InitError(strprintf(_("Wallet needed to be rewritten: restart %s to complete"), _(PACKAGE_NAME)));
            return NULL;
        }
        else {
            InitError(strprintf(_("Error loading %s"), walletFile));
            return NULL;
        }
    }

    if (GetBoolArg("-upgradewallet", loadState == NEW_WALLET))
    {
        int nMaxVersion = GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
            nMaxVersion = CLIENT_VERSION;
            walletInstance->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
        }
        else
            LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
        if (nMaxVersion < walletInstance->GetVersion())
        {
            InitError(_("Cannot downgrade wallet"));
            return NULL;
        }
        walletInstance->SetMaxVersion(nMaxVersion);
    }

    if (loadState == NEW_WALLET)
    {
        // Create new keyUser and set as default key
        if (GetBoolArg("-usehd", DEFAULT_USE_HD_WALLET))
        {
            if (fNoUI)
            {
                std::vector<unsigned char> entropy(16);
                GetStrongRandBytes(&entropy[0], 16);
                GuldenAppManager::gApp->setRecoveryPhrase(mnemonicFromEntropy(entropy, entropy.size()*8));
            }

            if (GuldenAppManager::gApp->getRecoveryPhrase().size() == 0)
            {
                //Work around an issue with "non HD" wallets from older versions where active account may not be set in the wallet.
                if (!walletInstance->mapAccounts.empty())
                    walletInstance->setActiveAccount(walletInstance->mapAccounts.begin()->second);
                throw std::runtime_error("Invalid seed mnemonic");
            }

            // Generate a new primary seed and account (BIP44)
            walletInstance->activeSeed = new CHDSeed(GuldenAppManager::gApp->getRecoveryPhrase().c_str(), CHDSeed::CHDSeed::BIP44);
            if (!CWalletDB(*walletInstance->dbw).WriteHDSeed(*walletInstance->activeSeed))
            {
                throw std::runtime_error("Writing seed failed");
            }
            walletInstance->mapSeeds[walletInstance->activeSeed->getUUID()] = walletInstance->activeSeed;
            walletInstance->activeAccount = walletInstance->GenerateNewAccount("My account", AccountState::Normal, AccountType::Desktop);

            // Now generate children shadow accounts to handle legacy transactions
            // Only for recovery wallets though, new ones don't need them
            if (GuldenAppManager::gApp->isRecovery)
            {
                CHDSeed* seedBip32 = new CHDSeed(GuldenAppManager::gApp->getRecoveryPhrase().c_str(), CHDSeed::CHDSeed::BIP32);
                if (!CWalletDB(*walletInstance->dbw).WriteHDSeed(*seedBip32))
                {
                    throw std::runtime_error("Writing bip32 seed failed");
                }
                CHDSeed* seedBip32Legacy = new CHDSeed(GuldenAppManager::gApp->getRecoveryPhrase().c_str(), CHDSeed::CHDSeed::BIP32Legacy);
                if (!CWalletDB(*walletInstance->dbw).WriteHDSeed(*seedBip32Legacy))
                {
                    throw std::runtime_error("Writing bip32 legacy seed failed");
                }
                walletInstance->mapSeeds[seedBip32->getUUID()] = seedBip32;
                walletInstance->mapSeeds[seedBip32Legacy->getUUID()] = seedBip32Legacy;

                // Write new accounts
                CAccountHD* newAccountBip32 = seedBip32->GenerateAccount(AccountType::Desktop, NULL);
                newAccountBip32->m_State = AccountState::ShadowChild;
                walletInstance->activeAccount->AddChild(newAccountBip32);
                walletInstance->addAccount(newAccountBip32, "BIP32 child account");

                // Write new accounts
                CAccountHD* newAccountBip32Legacy = seedBip32Legacy->GenerateAccount(AccountType::Desktop, NULL);
                newAccountBip32Legacy->m_State = AccountState::ShadowChild;
                walletInstance->activeAccount->AddChild(newAccountBip32Legacy);
                walletInstance->addAccount(newAccountBip32Legacy, "BIP32 legacy child account");
            }

            // Write the seed last so that account index changes are reflected
            {
                CWalletDB walletdb(*walletInstance->dbw);
                walletdb.WritePrimarySeed(*walletInstance->activeSeed);
                walletdb.WritePrimaryAccount(walletInstance->activeAccount);
            }

            GuldenAppManager::gApp->BurnRecoveryPhrase();

            //Assign the bare minimum keys here, let the rest take place in the background thread
            walletInstance->TopUpKeyPool(1);
        }
        else
        {
            walletInstance->activeAccount = new CAccount();
            walletInstance->activeAccount->m_State = AccountState::Normal;
            walletInstance->activeAccount->m_Type = AccountType::Desktop;

            // Write the primary account into wallet file
            {
                CWalletDB walletdb(*walletInstance->dbw);
                if (!walletdb.WriteAccount(getUUIDAsString(walletInstance->activeAccount->getUUID()), walletInstance->activeAccount))
                {
                    throw std::runtime_error("Writing legacy account failed");
                }
                walletdb.WritePrimaryAccount(walletInstance->activeAccount);
            }

            //Assign the bare minimum keys here, let the rest take place in the bakcground thread
            walletInstance->TopUpKeyPool(1);
        }

        pactiveWallet = walletInstance;
        walletInstance->SetBestChain(chainActive.GetLocatorPoW2());
    }
    else if (loadState == EXISTING_WALLET_OLDACCOUNTSYSTEM)
    {
        // HD upgrade.
        // Only perform upgrade if usehd is present (default for UI)
        // For daemon we force users to choose (for exchanges etc.)
        if (fNoUI)
        {
            if (!walletInstance->activeAccount->IsHD() && !walletInstance->activeSeed)
            {
                if (IsArgSet("-usehd"))
                {
                    throw std::runtime_error("Must specify -usehd=1 or -usehd=0, in order to allow or refuse HD upgrade.");
                }
            }
        }

        if (GetBoolArg("-usehd", DEFAULT_USE_HD_WALLET))
        {
            if (!walletInstance->activeAccount->IsHD() && !walletInstance->activeSeed)
            {
                while (true)
                {
                    {
                        LOCK(walletInstance->cs_wallet);
                        if (!walletInstance->IsLocked())
                            break;
                        walletInstance->wantDelayLock = true;
                        uiInterface.RequestUnlock(walletInstance, _("Wallet unlock required for wallet upgrade"));
                    }
                    MilliSleep(5000);
                }

                bool walletWasCrypted = walletInstance->activeAccount->externalKeyStore.IsCrypted();
                {
                    LOCK(walletInstance->cs_wallet);

                    //Force old legacy account to resave
                    {
                        CWalletDB walletdb(*walletInstance->dbw);
                        walletInstance->changeAccountName(walletInstance->activeAccount, _("Legacy account"), true);
                        if (!walletdb.WriteAccount(getUUIDAsString(walletInstance->activeAccount->getUUID()), walletInstance->activeAccount))
                        {
                            throw std::runtime_error("Writing legacy account failed");
                        }
                        if (walletWasCrypted && !walletInstance->activeAccount->internalKeyStore.IsCrypted())
                        {
                            walletInstance->activeAccount->internalKeyStore.SetCrypted();
                            bool needsWriteToDisk = false;
                            walletInstance->activeAccount->internalKeyStore.Unlock(walletInstance->activeAccount->vMasterKey, needsWriteToDisk);
                            if (needsWriteToDisk)
                            {
                                CWalletDB db(*dbw);
                                if (!db.WriteAccount(getUUIDAsString(walletInstance->activeAccount->getUUID()), walletInstance->activeAccount))
                                {
                                    throw std::runtime_error("Writing account failed");
                                }
                            }
                        }
                        walletInstance->ForceRewriteKeys(*walletInstance->activeAccount);
                    }

                    // Generate a new primary seed and account (BIP44)
                    std::vector<unsigned char> entropy(16);
                    GetStrongRandBytes(&entropy[0], 16);
                    walletInstance->activeSeed = new CHDSeed(mnemonicFromEntropy(entropy, entropy.size()*8).c_str(), CHDSeed::CHDSeed::BIP44);
                    if (!CWalletDB(*walletInstance->dbw).WriteHDSeed(*walletInstance->activeSeed))
                    {
                        throw std::runtime_error("Writing seed failed");
                    }
                    if (walletWasCrypted)
                    {
                        if (!walletInstance->activeSeed->Encrypt(walletInstance->activeAccount->vMasterKey))
                        {
                            throw std::runtime_error("Encrypting seed failed");
                        }
                    }
                    walletInstance->mapSeeds[walletInstance->activeSeed->getUUID()] = walletInstance->activeSeed;
                    {
                        CWalletDB walletdb(*walletInstance->dbw);
                        walletdb.WritePrimarySeed(*walletInstance->activeSeed);
                    }
                    walletInstance->activeAccount = walletInstance->GenerateNewAccount(_("My account"), AccountState::Normal, AccountType::Desktop);
                    {
                        CWalletDB walletdb(*walletInstance->dbw);
                        walletdb.WritePrimaryAccount(walletInstance->activeAccount);
                    }
                }
            }
        }
        else
        {
            while (true)
            {
                {
                    LOCK(walletInstance->cs_wallet);
                    if (!walletInstance->IsLocked())
                        break;
                    walletInstance->wantDelayLock = true;
                    uiInterface.RequestUnlock(walletInstance, _("Wallet unlock required for wallet upgrade"));
                }
                MilliSleep(5000);
            }

            bool walletWasCrypted = walletInstance->activeAccount->externalKeyStore.IsCrypted();
            {
                LOCK(walletInstance->cs_wallet);

                //Force old legacy account to resave
                {
                    CWalletDB walletdb(*walletInstance->dbw);
                    walletInstance->changeAccountName(walletInstance->activeAccount, _("Legacy account"), true);
                    if (!walletdb.WriteAccount(getUUIDAsString(walletInstance->activeAccount->getUUID()), walletInstance->activeAccount))
                    {
                        throw std::runtime_error("Writing legacy account failed");
                    }
                    if (walletWasCrypted && !walletInstance->activeAccount->internalKeyStore.IsCrypted())
                    {
                        walletInstance->activeAccount->internalKeyStore.SetCrypted();
                        bool needsWriteToDisk = false;
                        walletInstance->activeAccount->internalKeyStore.Unlock(walletInstance->activeAccount->vMasterKey, needsWriteToDisk);
                        if (needsWriteToDisk)
                        {
                            CWalletDB db(*dbw);
                            if (!db.WriteAccount(getUUIDAsString(walletInstance->activeAccount->getUUID()), walletInstance->activeAccount))
                            {
                                throw std::runtime_error("Writing account failed");
                            }
                        }
                    }
                    walletInstance->ForceRewriteKeys(*walletInstance->activeAccount);
                }
            }
        }
    }
    else if (loadState == EXISTING_WALLET)
    {
        //Clean up a slight issue in 1.6.0 -> 1.6.3 wallets where a "usehd=0" account was created but no active account set.
        if (!walletInstance->activeAccount)
        {
            if (!walletInstance->mapAccounts.empty())
                walletInstance->setActiveAccount(walletInstance->mapAccounts.begin()->second);
            else
                throw std::runtime_error("Wallet contains no accounts, but is marked as upgraded.");
        }
    }
    else
    {
        throw std::runtime_error("Unknown wallet load state.");
    }

    if (GuldenAppManager::gApp->isRecovery)
    {
        walletInstance->nTimeFirstKey = Params().GenesisBlock().nTime;
    }

    LogPrintf(" wallet      %15dms\n", GetTimeMillis() - nStart);

    RegisterValidationInterface(walletInstance);

    CBlockIndex *pindexRescan = chainActive.Tip();
    if (GetBoolArg("-rescan", false) || GuldenAppManager::gApp->isRecovery)
    {
        pindexRescan = chainActive.Genesis();
    }
    else
    {
        CWalletDB walletdb(*walletInstance->dbw);
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = FindForkInGlobalIndex(chainActive, locator);
    }
    if (chainActive.Tip() && chainActive.Tip() != pindexRescan)
    {
        //We can't rescan beyond non-pruned blocks, stop and throw an error
        //this might happen if a user uses a old wallet within a pruned node
        // or if he ran -disablewallet for a longer time, then decided to re-enable
        if (fPruneMode)
        {
            CBlockIndex *block = chainActive.Tip();
            while (block && block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA) && block->pprev->nTx > 0 && pindexRescan != block)
                block = block->pprev;

            if (pindexRescan != block) {
                InitError(_("Prune: last wallet synchronisation goes beyond pruned data. You need to -reindex (download the whole blockchain again in case of pruned node)"));
                return NULL;
            }
        }

        uiInterface.InitMessage(_("Rescanning..."));
        LogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);
        nStart = GetTimeMillis();
        walletInstance->ScanForWalletTransactions(pindexRescan, true);
        LogPrintf(" rescan      %15dms\n", GetTimeMillis() - nStart);
        walletInstance->SetBestChain(chainActive.GetLocatorPoW2());
        walletInstance->dbw->IncrementUpdateCounter();

        // Restore wallet transaction metadata after -zapwallettxes=1
        if (GetBoolArg("-zapwallettxes", false) && GetArg("-zapwallettxes", "1") != "2")
        {
            CWalletDB walletdb(*walletInstance->dbw);

            for(const CWalletTx& wtxOld : vWtx)
            {
                uint256 hash = wtxOld.GetHash();
                std::map<uint256, CWalletTx>::iterator mi = walletInstance->mapWallet.find(hash);
                if (mi != walletInstance->mapWallet.end())
                {
                    const CWalletTx* copyFrom = &wtxOld;
                    CWalletTx* copyTo = &mi->second;
                    copyTo->mapValue = copyFrom->mapValue;
                    copyTo->vOrderForm = copyFrom->vOrderForm;
                    copyTo->nTimeReceived = copyFrom->nTimeReceived;
                    copyTo->nTimeSmart = copyFrom->nTimeSmart;
                    copyTo->fFromMe = copyFrom->fFromMe;
                    copyTo->strFromAccount = copyFrom->strFromAccount;
                    copyTo->nOrderPos = copyFrom->nOrderPos;
                    walletdb.WriteTx(*copyTo);
                }
            }
        }
    }
    walletInstance->SetBroadcastTransactions(GetBoolArg("-walletbroadcast", DEFAULT_WALLETBROADCAST));

    {
        LOCK(walletInstance->cs_wallet);
        //fixme: (2.1) - 'key pool size' concept for wallet doesn't really make sense anymore.
        LogPrintf("setKeyPool.size() = %u\n",      walletInstance->GetKeyPoolSize());
        LogPrintf("mapWallet.size() = %u\n",       walletInstance->mapWallet.size());
        LogPrintf("mapAddressBook.size() = %u\n",  walletInstance->mapAddressBook.size());
    }

    return walletInstance;
}

bool CWallet::InitLoadWallet()
{
    LOCK(cs_main);

    if (GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        LogPrintf("Wallet disabled!\n");
        return true;
    }

    for (const std::string& walletFile : gArgs.GetArgs("-wallet")) {
        CWallet * const pwallet = CreateWalletFromFile(walletFile);
        if (!pwallet) {
            return false;
        }
        vpwallets.push_back(pwallet);
        pactiveWallet = pwallet;
    }

    return true;
}

std::atomic<bool> CWallet::fFlushScheduled(false);

void CWallet::postInitProcess(CScheduler& scheduler)
{
    // Add wallet transactions that aren't already in a block to mempool
    // Do this here as mempool requires genesis block to be loaded
    ReacceptWalletTransactions();

    // Run a thread to flush wallet periodically
    if (!CWallet::fFlushScheduled.exchange(true)) {
        scheduler.scheduleEvery(MaybeCompactWalletDB, 500);
    }
}

bool CWallet::ParameterInteraction()
{
    SoftSetArg("-wallet", DEFAULT_WALLET_DAT);
    const bool is_multiwallet = gArgs.GetArgs("-wallet").size() > 1;

    if (GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET))
        return true;

    if (GetBoolArg("-blocksonly", DEFAULT_BLOCKSONLY) && SoftSetBoolArg("-walletbroadcast", false)) {
        LogPrintf("%s: parameter interaction: -blocksonly=1 -> setting -walletbroadcast=0\n", __func__);
    }

    if (GetBoolArg("-salvagewallet", false) && SoftSetBoolArg("-rescan", true)) {
        if (is_multiwallet) {
            return InitError(strprintf("%s is only allowed with a single wallet file", "-salvagewallet"));
        }
        // Rewrite just private keys: rescan to find transactions
        LogPrintf("%s: parameter interaction: -salvagewallet=1 -> setting -rescan=1\n", __func__);
    }

    // -zapwallettx implies a rescan
    if (GetBoolArg("-zapwallettxes", false) && SoftSetBoolArg("-rescan", true)) {
        if (is_multiwallet) {
            return InitError(strprintf("%s is only allowed with a single wallet file", "-zapwallettxes"));
        }
        LogPrintf("%s: parameter interaction: -zapwallettxes=<mode> -> setting -rescan=1\n", __func__);
    }

    if (is_multiwallet) {
        if (GetBoolArg("-upgradewallet", false)) {
            return InitError(strprintf("%s is only allowed with a single wallet file", "-upgradewallet"));
        }
    }

    if (GetBoolArg("-sysperms", false))
        return InitError("-sysperms is not allowed in combination with enabled wallet functionality");
    if (GetArg("-prune", 0) && GetBoolArg("-rescan", false))
        return InitError(_("Rescans are not possible in pruned mode. You will need to use -reindex which will download the whole blockchain again."));

    if (::minRelayTxFee.GetFeePerK() > HIGH_TX_FEE_PER_KB)
        InitWarning(AmountHighWarn("-minrelaytxfee") + " " +
                    _("The wallet will avoid paying less than the minimum relay fee."));

    if (IsArgSet("-mintxfee"))
    {
        CAmount n = 0;
        if (!ParseMoney(GetArg("-mintxfee", ""), n) || 0 == n)
            return InitError(AmountErrMsg("mintxfee", GetArg("-mintxfee", "")));
        if (n > HIGH_TX_FEE_PER_KB)
            InitWarning(AmountHighWarn("-mintxfee") + " " +
                        _("This is the minimum transaction fee you pay on every transaction."));
        CWallet::minTxFee = CFeeRate(n);
    }
    if (IsArgSet("-fallbackfee"))
    {
        CAmount nFeePerK = 0;
        if (!ParseMoney(GetArg("-fallbackfee", ""), nFeePerK))
            return InitError(strprintf(_("Invalid amount for -fallbackfee=<amount>: '%s'"), GetArg("-fallbackfee", "")));
        if (nFeePerK > HIGH_TX_FEE_PER_KB)
            InitWarning(AmountHighWarn("-fallbackfee") + " " +
                        _("This is the transaction fee you may pay when fee estimates are not available."));
        CWallet::fallbackFee = CFeeRate(nFeePerK);
    }
    if (IsArgSet("-paytxfee"))
    {
        CAmount nFeePerK = 0;
        if (!ParseMoney(GetArg("-paytxfee", ""), nFeePerK))
            return InitError(AmountErrMsg("paytxfee", GetArg("-paytxfee", "")));
        if (nFeePerK > HIGH_TX_FEE_PER_KB)
            InitWarning(AmountHighWarn("-paytxfee") + " " +
                        _("This is the transaction fee you will pay if you send a transaction."));

        payTxFee = CFeeRate(nFeePerK, 1000);
        if (payTxFee < ::minRelayTxFee)
        {
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                                       GetArg("-paytxfee", ""), ::minRelayTxFee.ToString()));
        }
    }
    if (IsArgSet("-maxtxfee"))
    {
        CAmount nMaxFee = 0;
        if (!ParseMoney(GetArg("-maxtxfee", ""), nMaxFee))
            return InitError(AmountErrMsg("maxtxfee", GetArg("-maxtxfee", "")));
        if (nMaxFee > HIGH_MAX_TX_FEE)
            InitWarning(_("-maxtxfee is set very high! Fees this large could be paid on a single transaction."));
        maxTxFee = nMaxFee;
        if (CFeeRate(maxTxFee, 1000) < ::minRelayTxFee)
        {
            return InitError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s' (must be at least the minrelay fee of %s to prevent stuck transactions)"),
                                       GetArg("-maxtxfee", ""), ::minRelayTxFee.ToString()));
        }
    }
    nTxConfirmTarget = GetArg("-txconfirmtarget", DEFAULT_TX_CONFIRM_TARGET);
    bSpendZeroConfChange = GetBoolArg("-spendzeroconfchange", DEFAULT_SPEND_ZEROCONF_CHANGE);
    fWalletRbf = GetBoolArg("-walletrbf", DEFAULT_WALLET_RBF);

    return true;
}

bool CWallet::BackupWallet(const std::string& strDest)
{
    return dbw->Backup(strDest);
}
