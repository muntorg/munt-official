// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "guldenapplication.h"

GuldenApplication* GuldenApplication::gApp = new GuldenApplication();

GuldenApplication::GuldenApplication()
{
    gApp = this;
    isRecovery = false;
}

void GuldenApplication::setRecoveryPhrase(const SecureString& recoveryPhrase_)
{
    recoveryPhrase = recoveryPhrase_;
}

SecureString GuldenApplication::getRecoveryPhrase()
{
    return recoveryPhrase;
}

void GuldenApplication::BurnRecoveryPhrase()
{

    recoveryPhrase = "";
}
