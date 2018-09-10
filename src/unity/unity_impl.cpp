#include "unity_impl.h"
#include "libinit.h"
#include "util.h"

int32_t GuldenUnifiedBackend::InitUnityLib(const std::string& dataDir)
{
    if (!dataDir.empty())
        SoftSetArg("-datadir", dataDir);
    SoftSetArg("-listen", "0");
    SoftSetArg("-debug", "1");
    SoftSetArg("-fullsync", "0");
    SoftSetArg("-spv", "1");
    return InitUnity();
}
