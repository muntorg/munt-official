#include "unity_impl.h"
#include "libinit.h"
#include "util.h"
#include "ui_interface.h"
#include <qrencode.h>

#include "gulden_unified_backend.hpp"
#include "gulden_unified_frontend.hpp"
#include "qrcode_record.hpp"
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

QrcodeRecord GuldenUnifiedBackend::QRImageFromString(const std::string& qr_string, int32_t width_hint)
{
    QRcode* code = QRcode_encodeString(qr_string.c_str(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!code)
    {
        return QrcodeRecord(0, std::vector<uint8_t>());
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
        return QrcodeRecord(finalWidth, dataVector);
    }
}
