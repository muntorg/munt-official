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
    // The below is a 'SecureString' - so no memory burn necessary, it should burn itself.
    recoveryPhrase = "";
}

