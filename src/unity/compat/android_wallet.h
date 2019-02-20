// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include <string>

struct android_wallet
{
    bool fileOpenError = false;
    // True if we recognise the data file as being a minimally valid wallet file, does not guarantee that it actually contains any useful info.
    bool validWalletProto = false;
    // True if we manages to succesfully extract information from the wallet, may be false if that information is encrypted and no password was provided.
    bool validWallet = false;
    // True if we encountered encrypted information in the wallet.
    bool encrypted = false;
    // If the wallet contained a seed mnemonic that we managed to extract it will be set here.
    std::string walletSeedMnemonic;
    // If we managed to extract a wallet birth date this will be set to non zero with that date.
    uint64_t walletBirth=0;
    // In the case of failure resultMessage will be non empty and contain additional information.
    std::string resultMessage;
    // The number of fields encountered inside the wallet file.
    uint64_t numWalletFields=0;
};

android_wallet ParseAndroidProtoWallet(std::string walletPath, std::string walletPassword);
