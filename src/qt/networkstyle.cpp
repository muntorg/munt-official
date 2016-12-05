// Copyright (c) 2014-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "networkstyle.h"

#include "guiconstants.h"

#include <QApplication>

static const struct {
    const char* networkId;
    const char* appName;
    const int iconColorHueShift;
    const int iconColorSaturationReduction;
    const char* titleAddText;
} network_styles[] = {
      { "main", QAPP_APP_NAME_DEFAULT, 0, 0, "" },
      { "test", QAPP_APP_NAME_TESTNET, 70, 30, QT_TRANSLATE_NOOP("SplashScreen", "[testnet]") },
      { "regtest", QAPP_APP_NAME_TESTNET, 160, 30, "[regtest]" }
  };
static const unsigned network_styles_count = sizeof(network_styles) / sizeof(*network_styles);

// titleAddText needs to be const char* for tr()
NetworkStyle::NetworkStyle(const QString& appName, const int iconColorHueShift, const int iconColorSaturationReduction, const char* titleAddText)
    : appName(appName)
    , titleAddText(qApp->translate("SplashScreen", titleAddText))
{

    QPixmap pixmap(":/icons/bitcoin");

    if (iconColorHueShift != 0 && iconColorSaturationReduction != 0) {

        QImage img = pixmap.toImage();

        int h, s, l, a;

        for (int y = 0; y < img.height(); y++) {
            QRgb* scL = reinterpret_cast<QRgb*>(img.scanLine(y));

            for (int x = 0; x < img.width(); x++) {

                a = qAlpha(scL[x]);
                QColor col(scL[x]);

                col.getHsl(&h, &s, &l);

                h += iconColorHueShift;

                if (s > iconColorSaturationReduction) {
                    s -= iconColorSaturationReduction;
                }
                col.setHsl(h, s, l, a);

                scL[x] = col.rgba();
            }
        }

#if QT_VERSION >= 0x040700
        pixmap.convertFromImage(img);
#else
        pixmap = QPixmap::fromImage(img);
#endif
    }

    appIcon = QIcon(pixmap);
    trayAndWindowIcon = QIcon(pixmap.scaled(QSize(256, 256)));
}

const NetworkStyle* NetworkStyle::instantiate(const QString& networkId)
{
    for (unsigned x = 0; x < network_styles_count; ++x) {
        if (networkId == network_styles[x].networkId) {
            return new NetworkStyle(
                network_styles[x].appName,
                network_styles[x].iconColorHueShift,
                network_styles[x].iconColorSaturationReduction,
                network_styles[x].titleAddText);
        }
    }
    return 0;
}
