#include "unity_impl.h"
#include "libinit.h"
#include "util.h"
#include "ui_interface.h"

#include "gulden_unified_backend.hpp"
#include "gulden_unified_frontend.hpp"
#include "djinni_support.hpp"

static std::shared_ptr<GuldenUnifiedFrontend> signalHandler;
void handlePostInitMain()
{
    uiInterface.NotifySPVProgress.connect(
        [=](int startHeight, int processedHeight, int expectedHeight)
        {
            signalHandler->notifySPVProgress(startHeight, processedHeight, expectedHeight);
        }
    );
}

int32_t GuldenUnifiedBackend::InitUnityLib(const std::string& dataDir, const std::shared_ptr<GuldenUnifiedFrontend>& signals)
{
    if (!dataDir.empty())
        SoftSetArg("-datadir", dataDir);
    SoftSetArg("-listen", "0");
    SoftSetArg("-debug", "0");
    SoftSetArg("-fullsync", "0");
    SoftSetArg("-spv", "1");
    SoftSetArg("-accountpool", "1");

    signalHandler = signals;

    return InitUnity();
}
