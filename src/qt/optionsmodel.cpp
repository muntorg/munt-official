// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "optionsmodel.h"

#include "bitcoinunits.h"
#include "guiutil.h"

#include "amount.h"
#include "init.h"
#include "main.h" // For DEFAULT_SCRIPTCHECK_THREADS
#include "net.h"
#include "txdb.h" // for -dbcache defaults
#include "intro.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif

#include <QNetworkProxy>
#include <QSettings>
#include <QStringList>

GuldenOptionsModel::GuldenOptionsModel(OptionsModel* parent)
    : QObject(parent)
    , m_pImpl(parent)
{
}

GuldenOptionsModel::~GuldenOptionsModel()
{
}

void GuldenOptionsModel::InitSettings(QSettings& settings)
{

    if (!settings.contains("localCurrencySymbol"))
        settings.setValue("localCurrencySymbol", QString("EUR"));
    localCurrency = settings.value("localCurrencySymbol").toString();
}

void GuldenOptionsModel::setLocalCurrency(const QString& value)
{
    QSettings settings;
    settings.setValue("localCurrencySymbol", value);
    localCurrency = settings.value("localCurrencySymbol").toString();
    Q_EMIT localCurrencyChanged(localCurrency);
}

QString GuldenOptionsModel::getLocalCurrency()
{
    return localCurrency;
}

OptionsModel::OptionsModel(QObject* parent, bool resetSettings)
    : QAbstractListModel(parent)
{

    guldenSettings = new GuldenOptionsModel(this);
    Init(resetSettings);
}

void OptionsModel::addOverriddenOption(const std::string& option)
{
    strOverriddenByCommandLine += QString::fromStdString(option) + "=" + QString::fromStdString(mapArgs[option]) + " ";
}

void OptionsModel::Init(bool resetSettings)
{
    if (resetSettings)
        Reset();

    checkAndMigrate();

    QSettings settings;

    setRestartRequired(false);

    if (!settings.contains("fHideTrayIcon"))
        settings.setValue("fHideTrayIcon", false);
    fHideTrayIcon = settings.value("fHideTrayIcon").toBool();
    Q_EMIT hideTrayIconChanged(fHideTrayIcon);

    if (!settings.contains("fMinimizeToTray"))
        settings.setValue("fMinimizeToTray", false);
    fMinimizeToTray = settings.value("fMinimizeToTray").toBool() && !fHideTrayIcon;

    if (!settings.contains("fMinimizeOnClose"))
        settings.setValue("fMinimizeOnClose", false);
    fMinimizeOnClose = settings.value("fMinimizeOnClose").toBool();

    if (!settings.contains("nDisplayUnit"))
        settings.setValue("nDisplayUnit", BitcoinUnits::BTC);
    nDisplayUnit = BitcoinUnits::BTC; //settings.value("nDisplayUnit").toInt();

    if (!settings.contains("strThirdPartyTxUrls"))
        settings.setValue("strThirdPartyTxUrls", "");
    strThirdPartyTxUrls = settings.value("strThirdPartyTxUrls", "").toString();

    if (!settings.contains("fCoinControlFeatures"))
        settings.setValue("fCoinControlFeatures", false);
    fCoinControlFeatures = false; //settings.value("fCoinControlFeatures", false).toBool();

    if (!settings.contains("nDatabaseCache"))
        settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);
    if (!SoftSetArg("-dbcache", settings.value("nDatabaseCache").toString().toStdString()))
        addOverriddenOption("-dbcache");

    if (!settings.contains("nThreadsScriptVerif"))
        settings.setValue("nThreadsScriptVerif", DEFAULT_SCRIPTCHECK_THREADS);
    if (!SoftSetArg("-par", settings.value("nThreadsScriptVerif").toString().toStdString()))
        addOverriddenOption("-par");

    if (!settings.contains("strDataDir"))
        settings.setValue("strDataDir", Intro::getDefaultDataDirectory());

#ifdef ENABLE_WALLET
    if (!settings.contains("bSpendZeroConfChange"))
        settings.setValue("bSpendZeroConfChange", true);
    if (!SoftSetBoolArg("-spendzeroconfchange", settings.value("bSpendZeroConfChange").toBool()))
        addOverriddenOption("-spendzeroconfchange");
#endif

    if (!settings.contains("fUseUPnP"))
        settings.setValue("fUseUPnP", DEFAULT_UPNP);
    if (!SoftSetBoolArg("-upnp", settings.value("fUseUPnP").toBool()))
        addOverriddenOption("-upnp");

    if (!settings.contains("fListen"))
        settings.setValue("fListen", DEFAULT_LISTEN);
    if (!SoftSetBoolArg("-listen", settings.value("fListen").toBool()))
        addOverriddenOption("-listen");

    if (!settings.contains("fUseProxy"))
        settings.setValue("fUseProxy", false);
    if (!settings.contains("addrProxy"))
        settings.setValue("addrProxy", "127.0.0.1:9050");

    if (settings.value("fUseProxy").toBool() && !SoftSetArg("-proxy", settings.value("addrProxy").toString().toStdString()))
        addOverriddenOption("-proxy");
    else if (!settings.value("fUseProxy").toBool() && !GetArg("-proxy", "").empty())
        addOverriddenOption("-proxy");

    if (!settings.contains("fUseSeparateProxyTor"))
        settings.setValue("fUseSeparateProxyTor", false);
    if (!settings.contains("addrSeparateProxyTor"))
        settings.setValue("addrSeparateProxyTor", "127.0.0.1:9050");

    if (settings.value("fUseSeparateProxyTor").toBool() && !SoftSetArg("-onion", settings.value("addrSeparateProxyTor").toString().toStdString()))
        addOverriddenOption("-onion");
    else if (!settings.value("fUseSeparateProxyTor").toBool() && !GetArg("-onion", "").empty())
        addOverriddenOption("-onion");

    if (!settings.contains("language"))
        settings.setValue("language", "");
    if (!SoftSetArg("-lang", settings.value("language").toString().toStdString()))
        addOverriddenOption("-lang");

    language = settings.value("language").toString();

    currencyTicker = NULL;
    nocksSettings = NULL;
    guldenSettings->InitSettings(settings);
}

void OptionsModel::Reset()
{
    QSettings settings;

    QString dataDir = Intro::getDefaultDataDirectory();
    dataDir = settings.value("strDataDir", dataDir).toString();

    settings.clear();

    settings.setValue("strDataDir", dataDir);

    settings.setValue("fReset", true);

    if (GUIUtil::GetStartOnSystemStartup())
        GUIUtil::SetStartOnSystemStartup(false);
}

int OptionsModel::rowCount(const QModelIndex& parent) const
{
    return OptionIDRowCount;
}

QVariant OptionsModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::EditRole) {
        QSettings settings;
        switch (index.row()) {
        case StartAtStartup:
            return GUIUtil::GetStartOnSystemStartup();
        case HideTrayIcon:
            return fHideTrayIcon;
        case MinimizeToTray:
            return fMinimizeToTray;
        case MapPortUPnP:
#ifdef USE_UPNP
            return settings.value("fUseUPnP");
#else
            return false;
#endif
        case MinimizeOnClose:
            return fMinimizeOnClose;

        case ProxyUse:
            return settings.value("fUseProxy", false);
        case ProxyIP: {

            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(0);
        }
        case ProxyPort: {

            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(1);
        }

        case ProxyUseTor:
            return settings.value("fUseSeparateProxyTor", false);
        case ProxyIPTor: {

            QStringList strlIpPort = settings.value("addrSeparateProxyTor").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(0);
        }
        case ProxyPortTor: {

            QStringList strlIpPort = settings.value("addrSeparateProxyTor").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(1);
        }

#ifdef ENABLE_WALLET
        case SpendZeroConfChange:
            return settings.value("bSpendZeroConfChange");
#endif
        case DisplayUnit:
            return BitcoinUnits::BTC; ///
        case ThirdPartyTxUrls:
            return ""; ///
        case Language:
            return settings.value("language");
        case CoinControlFeatures:
            return false; ///
        case DatabaseCache:
            return settings.value("nDatabaseCache");
        case ThreadsScriptVerif:
            return settings.value("nThreadsScriptVerif");
        case Listen:
            return settings.value("fListen");
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool OptionsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    bool successful = true; /* set to false on parse error */
    if (role == Qt::EditRole) {
        QSettings settings;
        switch (index.row()) {
        case StartAtStartup:
            successful = GUIUtil::SetStartOnSystemStartup(value.toBool());
            break;
        case HideTrayIcon:
            fHideTrayIcon = value.toBool();
            settings.setValue("fHideTrayIcon", fHideTrayIcon);
            Q_EMIT hideTrayIconChanged(fHideTrayIcon);
            break;
        case MinimizeToTray:
            fMinimizeToTray = value.toBool();
            settings.setValue("fMinimizeToTray", fMinimizeToTray);
            break;
        case MapPortUPnP: // core option - can be changed on-the-fly
            settings.setValue("fUseUPnP", value.toBool());
            MapPort(value.toBool());
            break;
        case MinimizeOnClose:
            fMinimizeOnClose = value.toBool();
            settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
            break;

        case ProxyUse:
            if (settings.value("fUseProxy") != value) {
                settings.setValue("fUseProxy", value.toBool());
                setRestartRequired(true);
            }
            break;
        case ProxyIP: {

            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);

            if (!settings.contains("addrProxy") || strlIpPort.at(0) != value.toString()) {

                QString strNewValue = value.toString() + ":" + strlIpPort.at(1);
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        } break;
        case ProxyPort: {

            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);

            if (!settings.contains("addrProxy") || strlIpPort.at(1) != value.toString()) {

                QString strNewValue = strlIpPort.at(0) + ":" + value.toString();
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        } break;

        case ProxyUseTor:
            if (settings.value("fUseSeparateProxyTor") != value) {
                settings.setValue("fUseSeparateProxyTor", value.toBool());
                setRestartRequired(true);
            }
            break;
        case ProxyIPTor: {

            QStringList strlIpPort = settings.value("addrSeparateProxyTor").toString().split(":", QString::SkipEmptyParts);

            if (!settings.contains("addrSeparateProxyTor") || strlIpPort.at(0) != value.toString()) {

                QString strNewValue = value.toString() + ":" + strlIpPort.at(1);
                settings.setValue("addrSeparateProxyTor", strNewValue);
                setRestartRequired(true);
            }
        } break;
        case ProxyPortTor: {

            QStringList strlIpPort = settings.value("addrSeparateProxyTor").toString().split(":", QString::SkipEmptyParts);

            if (!settings.contains("addrSeparateProxyTor") || strlIpPort.at(1) != value.toString()) {

                QString strNewValue = strlIpPort.at(0) + ":" + value.toString();
                settings.setValue("addrSeparateProxyTor", strNewValue);
                setRestartRequired(true);
            }
        } break;

#ifdef ENABLE_WALLET
        case SpendZeroConfChange:
            if (settings.value("bSpendZeroConfChange") != value) {
                settings.setValue("bSpendZeroConfChange", value);
                setRestartRequired(true);
            }
            break;
#endif
        case DisplayUnit:
            setDisplayUnit(value);
            break;
        case ThirdPartyTxUrls:
            if (strThirdPartyTxUrls != value.toString()) {
                strThirdPartyTxUrls = value.toString();
                settings.setValue("strThirdPartyTxUrls", strThirdPartyTxUrls);
                setRestartRequired(true);
            }
            break;
        case Language:
            if (settings.value("language") != value) {
                settings.setValue("language", value);
                setRestartRequired(true);
            }
            break;
        case CoinControlFeatures:
            fCoinControlFeatures = value.toBool();
            settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
            Q_EMIT coinControlFeaturesChanged(fCoinControlFeatures);
            break;
        case DatabaseCache:
            if (settings.value("nDatabaseCache") != value) {
                settings.setValue("nDatabaseCache", value);
                setRestartRequired(true);
            }
            break;
        case ThreadsScriptVerif:
            if (settings.value("nThreadsScriptVerif") != value) {
                settings.setValue("nThreadsScriptVerif", value);
                setRestartRequired(true);
            }
            break;
        case Listen:
            if (settings.value("fListen") != value) {
                settings.setValue("fListen", value);
                setRestartRequired(true);
            }
            break;
        default:
            break;
        }
    }

    Q_EMIT dataChanged(index, index);

    return successful;
}

/** Updates current unit in memory, settings and emits displayUnitChanged(newUnit) signal */
void OptionsModel::setDisplayUnit(const QVariant& value)
{
    if (!value.isNull()) {
        QSettings settings;
        nDisplayUnit = value.toInt();
        settings.setValue("nDisplayUnit", nDisplayUnit);
        Q_EMIT displayUnitChanged(nDisplayUnit);
    }
}

bool OptionsModel::getProxySettings(QNetworkProxy& proxy) const
{

    proxyType curProxy;
    if (GetProxy(NET_IPV4, curProxy)) {
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName(QString::fromStdString(curProxy.proxy.ToStringIP()));
        proxy.setPort(curProxy.proxy.GetPort());

        return true;
    } else
        proxy.setType(QNetworkProxy::NoProxy);

    return false;
}

void OptionsModel::setRestartRequired(bool fRequired)
{
    QSettings settings;
    return settings.setValue("fRestartRequired", fRequired);
}

bool OptionsModel::isRestartRequired()
{
    QSettings settings;
    return settings.value("fRestartRequired", false).toBool();
}

void OptionsModel::checkAndMigrate()
{

    QSettings settings;
    static const char strSettingsVersionKey[] = "nSettingsVersion";
    int settingsVersion = settings.contains(strSettingsVersionKey) ? settings.value(strSettingsVersionKey).toInt() : 0;
    if (settingsVersion < CLIENT_VERSION) {

        if (settingsVersion < 130000 && settings.contains("nDatabaseCache") && settings.value("nDatabaseCache").toLongLong() == 100)
            settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);

        settings.setValue(strSettingsVersionKey, CLIENT_VERSION);
    }
}
