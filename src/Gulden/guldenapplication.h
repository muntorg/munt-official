// Copyright (c) 2015 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_QT_GULDENAPPLICATION_H
#define GULDEN_QT_GULDENAPPLICATION_H

#include <string>
#include "support/allocators/secure.h"




class GuldenApplication
{
public:
    GuldenApplication();
    void setRecoveryPhrase(const SecureString& recoveryPhrase);
    SecureString getRecoveryPhrase();
    void BurnRecoveryPhrase();
    static GuldenApplication* gApp;
    bool isRecovery;
private:
    SecureString recoveryPhrase;
};

#endif
