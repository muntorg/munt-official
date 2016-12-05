// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

// The below file implements the various Mnemonic/Seed/Entropy relations required to create a BIP39 compliant HD wallet.
// It is likely that none of these functions are implemented in the most elegant (or efficient) way possible
// However they are called infrequently (usually once per user) and the time spent in them is negligable - so it doesn't matter much
// Correctness is favoured here over speed - also deadlines were tight :)

// Note this has not been tested on big endian machines and likely requires further work for them.

#ifndef GULDEN_MNEMONIC_H
#define GULDEN_MNEMONIC_H

#include <string>
#include <vector>
#include "support/allocators/secure.h"

extern unsigned char* seedFromMnemonic(const SecureString& mnemonic, const SecureString& passphrase = "");
extern SecureString mnemonicFromEntropy(std::vector<unsigned char> entropyIn, int entropySizeInBytes);
extern std::vector<unsigned char> entropyFromMnemonic(const SecureString& mnemonic);
extern bool checkMnemonic(const SecureString& mnemonic);

bool testMnemonics();

#endif
