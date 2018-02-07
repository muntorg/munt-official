// Copyright (c) 2016-2017 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "wallet/wallet.h"
#include "policy/fees.h"
#include "account.h"
#include "script/ismine.h"
#include <boost/uuid/nil_generator.hpp>
#include <Gulden/mnemonic.h>

bool fShowChildAccountsSeperately = false;

void ThreadShadowPoolManager()
{
    boost::mutex condition_mutex;
    boost::unique_lock<boost::mutex> lock(condition_mutex);

    int depth = 1;
    while (true)
    {
        long milliSleep = 100;

        if (pactiveWallet)
        {
            LOCK(pactiveWallet->cs_wallet);

            milliSleep = 500;
            // Top up 'shadow' pool
            int numNew = 0;
            bool dolock=true;
            for (const auto& seedIter : pactiveWallet->mapSeeds)
            {
                //fixme: (GULDEN) (FUT) (1.6.1) (Support other seed types here)
                if (seedIter.second->m_type != CHDSeed::CHDSeed::BIP44 && seedIter.second->m_type != CHDSeed::CHDSeed::BIP44External && seedIter.second->m_type != CHDSeed::CHDSeed::BIP44NoHardening)
                    continue;

                for (const auto shadowSubType : { AccountSubType::Desktop, AccountSubType::Mobi, AccountSubType::PoW2Witness })
                {
                    int numShadow = 0;
                    {
                        for (const auto& accountPair : pactiveWallet->mapAccounts)
                        {
                            if (accountPair.second->IsHD() && ((CAccountHD*)accountPair.second)->getSeedUUID() == seedIter.second->getUUID())
                            {
                                if (accountPair.second->m_SubType == shadowSubType)
                                {
                                    if (accountPair.second->m_Type == AccountType::Shadow)
                                    {
                                        ++numShadow;
                                    }
                                }
                            }
                        }
                    }
                    if (numShadow < GetArg("-accountpool", 10))
                    {
                        dolock=false;
                        if (!pactiveWallet->IsLocked())
                        {
                            pactiveWallet->delayLock = true;
                            CWalletDB db(*pactiveWallet->dbw);
                            while (numShadow < GetArg("-accountpool", 10))
                            {
                                ++numShadow;
                                ++numNew;
                                depth=1;

                                // New shadow account
                                CAccountHD* newShadow = seedIter.second->GenerateAccount(shadowSubType, &db);

                                if (newShadow == NULL)
                                    break;

                                newShadow->m_Type = AccountType::Shadow;

                                // Write new account
                                pactiveWallet->addAccount(newShadow, "Shadow");

                                if (numNew > 2)
                                {
                                    milliSleep = 100;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            pactiveWallet->wantDelayLock = true;
                            if (numShadow < 2)
                            {
                                uiInterface.RequestUnlock(pactiveWallet, _("Wallet unlock required for account creation"));
                            }
                        }
                    }
                }
            }


            if (numNew == 0)
            {
                int targetPoolDepth = GetArg("-keypool", 40);
                int numToAllocatePerRound = 5;
                if (targetPoolDepth > 40)
                    numToAllocatePerRound = 20;
                int numAllocated = pactiveWallet->TopUpKeyPool(depth, numToAllocatePerRound);
                if (numAllocated >= 0)
                {
                    if (depth <= targetPoolDepth)
                    {
                        ++depth;
                    }
                    if (targetPoolDepth > 40 || numAllocated == 0)
                    {
                        milliSleep = 1;
                    }
                    else
                    {
                        //fixme: Look some more into these times - probably a bit slower than it needs to be
                        if (depth < 10)
                        {
                            milliSleep = 80;
                            dolock = false;
                        }
                        else if (depth < 20)
                        {
                            dolock = false;
                            milliSleep = 100;
                        }
                        else
                        {
                            milliSleep = 300;
                        }
                    }
                }
            }
            if (dolock)
            {
                if(pactiveWallet->didDelayLock)
                {
                    pactiveWallet->delayLock = false;
                    pactiveWallet->wantDelayLock = false;
                    pactiveWallet->didDelayLock = false;
                    pactiveWallet->Lock();
                }
            }
        }

        boost::this_thread::interruption_point();
        MilliSleep(milliSleep);
    }
}

void StartShadowPoolManagerThread(boost::thread_group& threadGroup)
{
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "shadowpoolmanager", &ThreadShadowPoolManager));
}

std::string accountNameForAddress(const CWallet &wallet, const CTxDestination& dest)
{
    LOCK(wallet.cs_wallet);
    std::string accountNameForAddress;

    isminetype ret = isminetype::ISMINE_NO;
    for (const auto& accountItem : wallet.mapAccounts)
    {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            isminetype temp = ( keyChain == KEYCHAIN_EXTERNAL ? IsMine(accountItem.second->externalKeyStore, dest) : IsMine(accountItem.second->internalKeyStore, dest) );
            if (temp > ret)
            {
                ret = temp;
                accountNameForAddress = accountItem.second->getLabel();
            }
        }
    }
    if (ret < isminetype::ISMINE_WATCH_ONLY)
        return "";
    return accountNameForAddress;
}

isminetype IsMine(const CWallet &wallet, const CTxDestination& dest)
{
    LOCK(wallet.cs_wallet);

    isminetype ret = isminetype::ISMINE_NO;
    for (const auto& accountItem : wallet.mapAccounts)
    {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            isminetype temp = ( keyChain == KEYCHAIN_EXTERNAL ? IsMine(accountItem.second->externalKeyStore, dest) : IsMine(accountItem.second->internalKeyStore, dest) );
            if (temp > ret)
                ret = temp;
        }
    }
    return ret;
}

isminetype IsMine(const CWallet &wallet, const CTxOut& out)
{
    LOCK(wallet.cs_wallet);

    uint256 outHash = out.output.GetHash();

    isminetype ret = isminetype::ISMINE_NO;
    for (const auto& accountItem : wallet.mapAccounts)
    {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            auto iter = accountItem.second->isminecache.find(outHash);
            if (iter != accountItem.second->isminecache.end())
            {
                if (iter->second > ret)
                    ret = iter->second;
            }
            else
            {
                isminetype temp = ( keyChain == KEYCHAIN_EXTERNAL ? IsMine(accountItem.second->externalKeyStore, out) : IsMine(accountItem.second->internalKeyStore, out) );
                if (temp > ret)
                    ret = temp;
                //fixme: keep trimmed by MRU
                accountItem.second->isminecache[outHash] = ret;
            }
            // No need to keep going through the remaining accounts at this point.
            if (ret >= ISMINE_SPENDABLE)
                return ret;
        }
    }
    return ret;
}

bool IsMine(const CAccount* forAccount, const CWalletTx& tx)
{
    isminetype ret = isminetype::ISMINE_NO;
    for (const auto& txout : tx.tx->vout)
    {
        for (auto keyChain : { KEYCHAIN_EXTERNAL, KEYCHAIN_CHANGE })
        {
            isminetype temp = ( keyChain == KEYCHAIN_EXTERNAL ? IsMine(forAccount->externalKeyStore, txout) : IsMine(forAccount->internalKeyStore, txout) );
            if (temp > ret)
                ret = temp;
        }
        if (ret > isminetype::ISMINE_NO)
            return true;
    }
    return false;
}


isminetype CGuldenWallet::IsMine(const CKeyStore &keystore, const CTxIn& txin) const
{
    {
        LOCK(cs_wallet);
        const CWalletTx* prev = pactiveWallet->GetWalletTx(txin.prevout.hash);
        if (prev)
        {
            if (txin.prevout.n < prev->tx->vout.size())
                return ::IsMine(keystore, prev->tx->vout[txin.prevout.n]);
        }
    }
    return ISMINE_NO;
}

void CGuldenWallet::MarkKeyUsed(CKeyID keyID, uint64_t usageTime)
{
    // Remove from key pool
    CWalletDB walletdb(*dbw);
    //NB! Must call ErasePool here even if HasPool is false - as ErasePool has other side effects.
    walletdb.ErasePool(static_cast<CWallet*>(this), keyID);

    //Update accounts if needed (creation time - shadow accounts etc.)
    {
        LOCK(cs_wallet);
        for (const auto& accountItem : mapAccounts)
        {
            if (accountItem.second->HaveKey(keyID))
            {
                if (usageTime > 0)
                {
                    CWalletDB walletdb(*dbw);
                    accountItem.second->possiblyUpdateEarliestTime(usageTime, &walletdb);
                }

                // We only do this the first time MarkKeyUsed is called - otherwise we have the following problem
                // 1) User empties account. 2) User deletes account 3) At a later point MarkKeyUsed is called subsequent times (new blocks) 4) The account user just deleted is now recovered.

                //fixme: (GULDEN) (FUT) (1.6.1) This is still not 100% right, if the user does the following there can still be issues:
                //1) Send funds from account
                //2) Immediately close wallet
                //3) Reopen wallet, as the new blocks are processed this code will be called and the just deleted account will be restored.
                //We will need a better way to detect this...

                //fixme: (GULDEN) (FUT) (1.6.1) 
                //Another edge bug here
                //1) User sends/receives from address
                //2) User deleted account
                //3) User receives on same address again
                //Account will not reappear - NB! This happens IFF there is no wallet restart anywhere in this process, and the user can always rescan still so it's not a disaster.
                static std::set<CKeyID> keyUsedSet;
                if (keyUsedSet.find(keyID) == keyUsedSet.end())
                {
                    keyUsedSet.insert(keyID);
                    if (accountItem.second->m_Type != AccountType::Normal && accountItem.second->m_Type != AccountType::ShadowChild)
                    {
                        accountItem.second->m_Type = AccountType::Normal;
                        std::string name = accountItem.second->getLabel();
                        if (name.find(_("[Deleted]")) != std::string::npos)
                        {
                            name = name.replace(name.find(_("[Deleted]")), _("[Deleted]").length(), _("[Restored]"));
                        }
                        else
                        {
                            name = _("Restored");
                        }
                        addAccount(accountItem.second, name);

                        //fixme: Shadow accounts during rescan...
                    }
                }
            }
        }
    }

    //Only assign the bare minimum keys - let the background thread do the rest.
    static_cast<CWallet*>(this)->TopUpKeyPool(10);
}

void CGuldenWallet::changeAccountName(CAccount* account, const std::string& newName, bool notify)
{
    // Force names to be unique
    std::string finalNewName = newName;
    std::string oldName = account->getLabel();
    {
        LOCK(cs_wallet);

        CWalletDB db(*dbw);

        int nPrefix = 1;
        bool possibleDuplicate = true;
        while(possibleDuplicate)
        {
            possibleDuplicate = false;
            for (const auto& labelIter : mapAccountLabels)
            {
                if (labelIter.second == finalNewName)
                {
                    finalNewName = newName + strprintf("_%d", nPrefix++);
                    possibleDuplicate = true;
                    continue;
                }
            }
        }

        account->setLabel(finalNewName, &db);
        mapAccountLabels[account->getUUID()] = finalNewName;
    }

    if (notify && oldName != finalNewName)
        NotifyAccountNameChanged(static_cast<CWallet*>(this), account);
}

void CGuldenWallet::deleteAccount(CAccount* account)
{
    std::string newLabel = account->getLabel();
    if (newLabel.find(_("[Restored]")) != std::string::npos)
    {
        newLabel = newLabel.replace(newLabel.find(_("[Restored]")), _("[Restored]").length(), _("[Deleted]"));
    }
    else
    {
        newLabel = newLabel + " " + _("[Deleted]");
    }

    {
        LOCK(cs_wallet);

        CWalletDB db(*dbw);
        account->setLabel(newLabel, &db);
        account->m_Type = AccountType::Deleted;
        mapAccountLabels[account->getUUID()] = newLabel;
        if (!db.WriteAccount(account->getUUID(), account))
        {
            throw std::runtime_error("Writing account failed");
        }
    }
    NotifyAccountDeleted(static_cast<CWallet*>(this), account);
}

void CGuldenWallet::addAccount(CAccount* account, const std::string& newName)
{
    {
        LOCK(cs_wallet);

        CWalletDB walletdb(*dbw);
        if (!walletdb.WriteAccount(account->getUUID(), account))
        {
            throw std::runtime_error("Writing account failed");
        }
        mapAccounts[account->getUUID()] = account;
        changeAccountName(account, newName, false);
    }

    if (account->m_Type == AccountType::Normal)
    {
        NotifyAccountAdded(static_cast<CWallet*>(this), account);
        NotifyAccountNameChanged(static_cast<CWallet*>(this), account);
        NotifyUpdateAccountList(static_cast<CWallet*>(this), account);
    }
}

void CGuldenWallet::setActiveAccount(CAccount* newActiveAccount)
{
    if (activeAccount != newActiveAccount)
    {
        activeAccount = newActiveAccount;
        CWalletDB walletdb(*dbw);
        walletdb.WritePrimaryAccount(activeAccount);

        NotifyActiveAccountChanged(static_cast<CWallet*>(this), newActiveAccount);
    }
}

CAccount* CGuldenWallet::getActiveAccount()
{
    return activeAccount;
}

void CGuldenWallet::setActiveSeed(CHDSeed* newActiveSeed)
{
    if (activeSeed != newActiveSeed)
    {
        activeSeed = newActiveSeed;
        CWalletDB walletdb(*dbw);
        walletdb.WritePrimarySeed(*activeSeed);

        //fixme: (FUT) (1.6.1)
        //NotifyActiveSeedChanged(this, newActiveAccount);
    }
}

CHDSeed* CGuldenWallet::GenerateHDSeed(CHDSeed::SeedType seedType)
{
    if (IsCrypted() && (!activeAccount || IsLocked()))
    {
        throw std::runtime_error("Generating seed requires unlocked wallet");
    }

    std::vector<unsigned char> entropy(16);
    GetStrongRandBytes(&entropy[0], 16);
    CHDSeed* newSeed = new CHDSeed(mnemonicFromEntropy(entropy, entropy.size()*8).c_str(), seedType);
    if (!CWalletDB(*dbw).WriteHDSeed(*newSeed))
    {
        throw std::runtime_error("Writing seed failed");
    }
    if (IsCrypted())
    {
        if (!newSeed->Encrypt(activeAccount->vMasterKey))
        {
            throw std::runtime_error("Encrypting seed failed");
        }
    }
    mapSeeds[newSeed->getUUID()] = newSeed;

    return newSeed;
}

void CGuldenWallet::DeleteSeed(CHDSeed* deleteSeed, bool purge)
{
    mapSeeds.erase(mapSeeds.find(deleteSeed->getUUID()));
    if (!CWalletDB(*dbw).DeleteHDSeed(*deleteSeed))
    {
        throw std::runtime_error("Deleting seed failed");
    }

    //fixme: purge accounts completely if empty?
    for (const auto& accountPair : pactiveWallet->mapAccounts)
    {
        if (accountPair.second->IsHD() && ((CAccountHD*)accountPair.second)->getSeedUUID() == deleteSeed->getUUID())
        {
            //fixme: check balance
            deleteAccount(accountPair.second);
        }
    }

    if (activeSeed == deleteSeed)
    {
        if (mapSeeds.empty())
            setActiveSeed(NULL);
        else
            setActiveSeed(mapSeeds.begin()->second);
    }

    delete deleteSeed;
}

CHDSeed* CGuldenWallet::ImportHDSeedFromPubkey(SecureString pubKeyString)
{
    if (IsCrypted() && (!activeAccount || IsLocked()))
    {
        throw std::runtime_error("Generating seed requires unlocked wallet");
    }

    CExtPubKey pubkey;
    try
    {
        CBitcoinSecretExt<CExtPubKey> secretExt;
        secretExt.SetString(pubKeyString.c_str());
        pubkey = secretExt.GetKey();
    }
    catch(...)
    {
        throw std::runtime_error("Not a valid Gulden extended public key");
    }

    if (!pubkey.pubkey.IsValid())
    {
        throw std::runtime_error("Not a valid Gulden extended public key");
    }

    CHDSeed* newSeed = new CHDSeed(pubkey, CHDSeed::CHDSeed::BIP44NoHardening);
    if (!CWalletDB(*dbw).WriteHDSeed(*newSeed))
    {
        throw std::runtime_error("Writing seed failed");
    }
    if (IsCrypted())
    {
        if (!newSeed->Encrypt(activeAccount->vMasterKey))
        {
            throw std::runtime_error("Encrypting seed failed");
        }
    }
    mapSeeds[newSeed->getUUID()] = newSeed;

    return newSeed;
}

CHDSeed* CGuldenWallet::ImportHDSeed(SecureString mnemonic, CHDSeed::SeedType type)
{
    if (IsCrypted() && (!activeAccount || IsLocked()))
    {
        throw std::runtime_error("Generating seed requires unlocked wallet");
    }

    if (!checkMnemonic(mnemonic))
    {
        throw std::runtime_error("Not a valid Gulden mnemonic");
    }

    std::vector<unsigned char> entropy(16);
    GetStrongRandBytes(&entropy[0], 16);
    CHDSeed* newSeed = new CHDSeed(mnemonic, type);
    if (!CWalletDB(*dbw).WriteHDSeed(*newSeed))
    {
        throw std::runtime_error("Writing seed failed");
    }
    if (IsCrypted())
    {
        if (!newSeed->Encrypt(activeAccount->vMasterKey))
        {
            throw std::runtime_error("Encrypting seed failed");
        }
    }
    mapSeeds[newSeed->getUUID()] = newSeed;

    return newSeed;
}


CHDSeed* CGuldenWallet::getActiveSeed()
{
    return activeSeed;
}

void CGuldenWallet::RemoveAddressFromKeypoolIfIsMine(const CTxOut& txout, uint64_t time)
{
    ::RemoveAddressFromKeypoolIfIsMine(*static_cast<CWallet*>(this), txout, time);
}


void CGuldenWallet::RemoveAddressFromKeypoolIfIsMine(const CTransaction& tx, uint64_t time)
{
    for(const CTxOut& txout : tx.vout)
    {
        RemoveAddressFromKeypoolIfIsMine(txout, time);
    }
}

void CGuldenWallet::RemoveAddressFromKeypoolIfIsMine(const CTxIn& txin, uint64_t time)
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
                RemoveAddressFromKeypoolIfIsMine(prev.tx->vout[txin.prevout.n], time);
        }
    }
}

// Shadow accounts... For HD we keep a 'cache' of already created accounts, the reason being that another shared wallet might create new addresses, and we need to be able to detect those.
// So we keep these 'shadow' accounts, and if they ever receive a transaction we automatically 'convert' them into normal accounts in the UI.
// If/When the user wants new accounts, we hand out the previous shadow account and generate a new Shadow account to take it's place...
CAccountHD* CGuldenWallet::GenerateNewAccount(std::string strAccount, AccountType type, AccountSubType subType)
{

    assert(type != AccountType::ShadowChild);
    assert(type != AccountType::Deleted);
    CAccountHD* newAccount = NULL;

    // Grab account with lowest index from existing pool of shadow accounts and convert it into a new account.
    if (type == AccountType::Normal || type == AccountType::Shadow)
    {
        LOCK(cs_wallet);

        for (const auto& accountPair : mapAccounts)
        {
            if (accountPair.second->m_SubType == subType)
            {
                if (accountPair.second->m_Type == AccountType::Shadow)
                {
                    if (!newAccount || ((CAccountHD*)accountPair.second)->getIndex() < newAccount->getIndex())
                    {
                        if (((CAccountHD*)accountPair.second)->getSeedUUID() == getActiveSeed()->getUUID())
                        {
                            //Only consider accounts that are
                            //1) Of the required type
                            //2) Marked as being shadow
                            //3) From the active seed
                            //4) Always take the lowest account index that we can find
                            newAccount = (CAccountHD*)accountPair.second;
                        }
                    }
                }
            }
        }
    }

    // Create a new account in the (unlikely) event there was no shadow type
    if (!newAccount)
    {
        while (true)
        {
            //fixme: GULDEN (FUT) (1.6.1) - No way to cancel here...
            {
                LOCK(cs_wallet);
                if (!IsLocked())
                    break;
                wantDelayLock = true;
                uiInterface.RequestUnlock(static_cast<CWallet*>(this), _("Wallet unlock required for account creation"));
            }
            MilliSleep(5000);
        }
        if (!IsLocked())
        {
            CWalletDB db(*dbw);
            newAccount = activeSeed->GenerateAccount(subType, &db);
        }
        else
        {
            return NULL;
        }
    }
    newAccount->m_Type = type;

    // We don't top up the shadow pool here - we have a thread for that.

    // Write new account
    //NB! We have to do this -after- we create the new shadow account, otherwise the shadow account ends up being the active account.
    addAccount(newAccount, strAccount);

    // Shadow accounts have less keys - so we need to top up the keypool for our new 'non shadow' account at this point.
    if( activeAccount ) //fixme: (GULDEN) IsLocked() requires activeAccount - so we avoid calling this if activeAccount not yet set.
        static_cast<CWallet*>(this)->TopUpKeyPool(2);//We only assign the bare minimum addresses here - and let the background thread do the rest

    return newAccount;
}

CAccount* CGuldenWallet::GenerateNewLegacyAccount(std::string strAccount)
{
    CAccount* newAccount = new CAccount();
    addAccount(newAccount, strAccount);

    return newAccount;
}

CAccountHD* CGuldenWallet::CreateReadOnlyAccount(std::string strAccount, SecureString encExtPubKey)
{
    CAccountHD* newAccount = NULL;

    CExtPubKey pubkey;
    try
    {
        CBitcoinSecretExt<CExtPubKey> secretExt;
        secretExt.SetString(encExtPubKey.c_str());
        pubkey = secretExt.GetKey();
    }
    catch(...)
    {
        throw std::runtime_error("Not a valid Gulden extended public key");
    }

    newAccount = new CAccountHD(pubkey, boost::uuids::nil_generator()());

    // Write new account
    addAccount(newAccount, strAccount);

    //We only assign the bare minimum addresses here - and let the background thread do the rest
    static_cast<CWallet*>(this)->TopUpKeyPool(2);

    return newAccount;
}

void CGuldenWallet::ForceRewriteKeys(CAccount& forAccount)
{
    {
        LOCK(cs_wallet);

        std::set<CKeyID> setAddress;
        forAccount.GetKeys(setAddress);

        CWalletDB walletDB(*dbw);
        for (const auto& keyID : setAddress)
        {
            if (!IsCrypted())
            {
                CKey keyPair;
                if (!GetKey(keyID, keyPair))
                {
                    throw std::runtime_error("Fatal error upgrading legacy wallet - could not upgrade all keys");
                }
                walletDB.EraseKey(keyPair.GetPubKey());
                if (!walletDB.WriteKey(keyPair.GetPubKey(), keyPair.GetPrivKey(), static_cast<CWallet*>(this)->mapKeyMetadata[keyID], forAccount.getUUID(), KEYCHAIN_EXTERNAL))
                {
                    throw std::runtime_error("Fatal error upgrading legacy wallet - could not write all upgraded keys");
                }
            }
            else
            {
                CPubKey pubKey;
                if (!GetPubKey(keyID, pubKey))
                { 
                    throw std::runtime_error("Fatal error upgrading legacy wallet - could not upgrade all encrypted keys");
                }
                walletDB.EraseEncryptedKey(pubKey);
                std::vector<unsigned char> secret;
                if (!forAccount.GetKey(keyID, secret))
                { 
                    throw std::runtime_error("Fatal error upgrading legacy wallet - could not upgrade all encrypted keys");
                }
                if (!walletDB.WriteCryptedKey(pubKey, secret, static_cast<CWallet*>(this)->mapKeyMetadata[keyID], forAccount.getUUID(), KEYCHAIN_EXTERNAL))
                {
                    throw std::runtime_error("Fatal error upgrading legacy wallet - could not write all upgraded keys");
                }
            }
        }
    }


    CWalletDB walletDB(*dbw);
    BOOST_FOREACH(int64_t nIndex, forAccount.setKeyPoolInternal)
    {
        CKeyPool keypoolentry;
        if (!walletDB.ReadPool(nIndex, keypoolentry))
        {
            throw std::runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
        if (!walletDB.ErasePool( static_cast<CWallet*>(this), nIndex ))
        {
            throw std::runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
        keypoolentry.accountName = forAccount.getUUID();
        keypoolentry.nChain = KEYCHAIN_CHANGE;
        if ( !walletDB.WritePool( nIndex, keypoolentry ) )
        {
            throw std::runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
    }
    BOOST_FOREACH(int64_t nIndex, forAccount.setKeyPoolExternal)
    {
        CKeyPool keypoolentry;
        if (!walletDB.ReadPool(nIndex, keypoolentry))
        {
            throw std::runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
        if (!walletDB.ErasePool( static_cast<CWallet*>(this), nIndex ))
        {
            throw std::runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
        keypoolentry.accountName = forAccount.getUUID();
        keypoolentry.nChain = KEYCHAIN_EXTERNAL;
        if ( !walletDB.WritePool( nIndex, keypoolentry ) )
        {
            throw std::runtime_error("Fatal error upgrading legacy wallet - could not upgrade entire keypool");
        }
    }
}


bool CGuldenWallet::AddKeyPubKey(int64_t HDKeyIndex, const CPubKey &pubkey, CAccount& forAccount, int keyChain)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (!forAccount.AddKeyPubKey(HDKeyIndex, pubkey, keyChain))
        return false;

    // check if we need to remove from watch-only
    CScript script;
    script = GetScriptForDestination(pubkey.GetID());
    if (HaveWatchOnly(script))
        static_cast<CWallet*>(this)->RemoveWatchOnly(script);
    script = GetScriptForRawPubKey(pubkey);
    if (HaveWatchOnly(script))
        static_cast<CWallet*>(this)->RemoveWatchOnly(script);

    return CWalletDB(*dbw).WriteKeyHD(pubkey, HDKeyIndex, keyChain, static_cast<CWallet*>(this)->mapKeyMetadata[pubkey.GetID()], forAccount.getUUID());
}


bool CGuldenWallet::LoadKey(int64_t HDKeyIndex, int64_t keyChain, const CPubKey &pubkey, const std::string& forAccount)
{
    LOCK(cs_wallet);
    return mapAccounts[forAccount]->AddKeyPubKey(HDKeyIndex, pubkey, keyChain);
}

CPubKey CWallet::GenerateNewKey(CAccount& forAccount, int keyChain)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata

    // Create new metadata
    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    CPubKey pubkey = forAccount.GenerateNewKey(*this, metadata, keyChain);

    mapKeyMetadata[pubkey.GetID()] = metadata;
    UpdateTimeFirstKey(nCreationTime);

    return pubkey;
}

bool CWallet::LoadKey(const CKey& key, const CPubKey &pubkey, const std::string& forAccount, int64_t nKeyChain)
{
    LOCK(cs_wallet);
    return mapAccounts[forAccount]->AddKeyPubKey(key, pubkey, nKeyChain);
}
