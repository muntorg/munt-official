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

#include "bitcoingui.h"

#include "chainparams.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "intro.h"
#include "networkstyle.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "splashscreen.h"
#include "utilitydialog.h"
#include "winshutdownmonitor.h"

#ifdef ENABLE_WALLET
#include "paymentserver.h"
#include "walletmodel.h"
#endif

#include "init.h"
#include "rpc/server.h"
#include "scheduler.h"
#include "ui_interface.h"
#include "util.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <stdint.h>

#include <boost/filesystem/operations.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>

#include <QApplication>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <_Gulden/GuldenTranslator.h>
#include <QSslConfiguration>

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if QT_VERSION < 0x050000
Q_IMPORT_PLUGIN(qcncodecs)
Q_IMPORT_PLUGIN(qjpcodecs)
Q_IMPORT_PLUGIN(qtwcodecs)
Q_IMPORT_PLUGIN(qkrcodecs)
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#else
#if QT_VERSION < 0x050400
Q_IMPORT_PLUGIN(AccessibleFactory)
#endif
#if defined(QT_QPA_PLATFORM_XCB)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_WINDOWS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_COCOA)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#endif
#endif
#endif

#if QT_VERSION < 0x050000
#include <QTextCodec>
#endif

Q_DECLARE_METATYPE(bool*)
Q_DECLARE_METATYPE(CAmount)

static void InitMessage(const std::string& message)
{
    LogPrintf("init message: %s\n", message);
}

/*
   Translate string to current locale using Qt.
 */
static std::string Translate(const char* psz)
{
    return QCoreApplication::translate("Gulden", psz).toStdString();
}

static QString GetLangTerritory()
{
    QSettings settings;

    QString lang_territory = QLocale::system().name();

    QString lang_territory_qsettings = settings.value("language", "").toString();
    if (!lang_territory_qsettings.isEmpty())
        lang_territory = lang_territory_qsettings;

    lang_territory = QString::fromStdString(GetArg("-lang", lang_territory.toStdString()));
    return lang_territory;
}

/** Set up translations */
static void initTranslations(GuldenTranslator& qtTranslatorBase, GuldenTranslator& qtTranslator, GuldenTranslator& translatorBase, GuldenTranslator& translator, GuldenTranslator& translatorBaseGulden, GuldenTranslator& translatorGulden)
{

    QApplication::removeTranslator(&qtTranslatorBase);
    QApplication::removeTranslator(&qtTranslator);
    QApplication::removeTranslator(&translatorBase);
    QApplication::removeTranslator(&translator);

    QString lang_territory = GetLangTerritory();

    QString lang = lang_territory;
    lang.truncate(lang_territory.lastIndexOf('_'));

    if (qtTranslatorBase.load("qt_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslatorBase);

    if (qtTranslator.load("qt_" + lang_territory, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslator);

    if (translatorBase.load(lang, ":/translations/"))
        QApplication::installTranslator(&translatorBase);

    if (translatorBaseGulden.load(lang, ":/gulden_translations/"))
        QApplication::installTranslator(&translatorBaseGulden);

    if (translator.load(lang_territory, ":/translations/"))
        QApplication::installTranslator(&translator);

    if (translatorGulden.load(lang_territory, ":/gulden_translations/"))
        QApplication::installTranslator(&translatorGulden);
}

/* qDebug() message handler --> debug.log */
#if QT_VERSION < 0x050000
void DebugMessageHandler(QtMsgType type, const char* msg)
{
    const char* category = (type == QtDebugMsg) ? "qt" : NULL;
    LogPrint(category, "GUI: %s\n", msg);
}
#else
void DebugMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    Q_UNUSED(context);
    const char* category = (type == QtDebugMsg) ? "qt" : NULL;
    LogPrint(category, "GUI: %s\n", msg.toStdString());
}
#endif

/** Class encapsulating Bitcoin Core startup and shutdown.
 * Allows running startup and shutdown in a different thread from the UI thread.
 */
class BitcoinCore : public QObject {
    Q_OBJECT
public:
    explicit BitcoinCore();

public Q_SLOTS:
    void initialize();
    void shutdown();

Q_SIGNALS:
    void initializeResult(int retval);
    void shutdownResult(int retval);
    void runawayException(const QString& message);

private:
    boost::thread_group threadGroup;
    CScheduler scheduler;

    void handleRunawayException(const std::exception* e);
};

/** Main Bitcoin application object */
class BitcoinApplication : public QApplication {
    Q_OBJECT
public:
    explicit BitcoinApplication(int& argc, char** argv);
    ~BitcoinApplication();

#ifdef ENABLE_WALLET

    void createPaymentServer();
#endif

    void parameterSetup();

    void createOptionsModel(bool resetSettings);

    void createWindow(const NetworkStyle* networkStyle);

    void createSplashScreen(const NetworkStyle* networkStyle);

    void requestShutdown();

    int getReturnValue() { return returnValue; }

    WId getMainWinId() const;

public Q_SLOTS:

    void requestInitialize();
    void initializeResult(int retval);
    void shutdownResult(int retval);

    void handleRunawayException(const QString& message);

Q_SIGNALS:
    void requestedInitialize();
    void requestedShutdown();
    void stopThread();
    void splashFinished(QWidget* window);

private:
    QThread* coreThread;
    OptionsModel* optionsModel;
    ClientModel* clientModel;
    BitcoinGUI* window;
    QTimer* pollShutdownTimer;
#ifdef ENABLE_WALLET
    PaymentServer* paymentServer;
    WalletModel* walletModel;
#endif
    int returnValue;
    const PlatformStyle* platformStyle;

    void startThread();

    bool shutDownRequested;
};

#include "bitcoin.moc"

BitcoinCore::BitcoinCore()
    : QObject()
{
}

void BitcoinCore::handleRunawayException(const std::exception* e)
{
    PrintExceptionContinue(e, "Runaway exception");
    Q_EMIT runawayException(QString::fromStdString(strMiscWarning));
}

void BitcoinCore::initialize()
{
    try {
        qDebug() << __func__ << ": Running AppInit2 in thread";
        int rv = AppInit2(threadGroup, scheduler);
        Q_EMIT initializeResult(rv);
    }
    catch (const std::exception& e) {
        handleRunawayException(&e);
    }
    catch (...) {
        handleRunawayException(NULL);
    }
}

void BitcoinCore::shutdown()
{
    try {
        qDebug() << __func__ << ": Running Shutdown in thread";
        Interrupt(threadGroup);
        threadGroup.join_all();
        Shutdown();
        qDebug() << __func__ << ": Shutdown finished";
        Q_EMIT shutdownResult(1);
    }
    catch (const std::exception& e) {
        handleRunawayException(&e);
    }
    catch (...) {
        handleRunawayException(NULL);
    }
}

BitcoinApplication::BitcoinApplication(int& argc, char** argv)
    : QApplication(argc, argv)
    , coreThread(0)
    , optionsModel(0)
    , clientModel(0)
    , window(0)
    , pollShutdownTimer(0)
    ,
#ifdef ENABLE_WALLET
    paymentServer(0)
    , walletModel(0)
    ,
#endif
    returnValue(0)
    , shutDownRequested(false)
{
    setQuitOnLastWindowClosed(false);

    std::string platformName;
    platformName = GetArg("-uiplatform", BitcoinGUI::DEFAULT_UIPLATFORM);
    platformStyle = PlatformStyle::instantiate(QString::fromStdString(platformName));
    if (!platformStyle) // Fall back to "other" if specified name not found
        platformStyle = PlatformStyle::instantiate("other");
    assert(platformStyle);
}

BitcoinApplication::~BitcoinApplication()
{
    if (coreThread) {
        qDebug() << __func__ << ": Stopping thread";
        Q_EMIT stopThread();
        coreThread->wait();
        qDebug() << __func__ << ": Stopped thread";
    }

    delete window;
    window = 0;
#ifdef ENABLE_WALLET
    delete paymentServer;
    paymentServer = 0;
#endif
    delete optionsModel;
    optionsModel = 0;
    delete platformStyle;
    platformStyle = 0;
}

#ifdef ENABLE_WALLET
void BitcoinApplication::createPaymentServer()
{
    paymentServer = new PaymentServer(this);
}
#endif

void BitcoinApplication::createOptionsModel(bool resetSettings)
{
    optionsModel = new OptionsModel(NULL, resetSettings);
}

void BitcoinApplication::createWindow(const NetworkStyle* networkStyle)
{
    window = new BitcoinGUI(platformStyle, networkStyle, 0);
    connect((QObject*)window->walletFrame, SIGNAL(loadWallet()), this, SLOT(requestInitialize()));

    pollShutdownTimer = new QTimer(window);
    connect(pollShutdownTimer, SIGNAL(timeout()), window, SLOT(detectShutdown()));
    pollShutdownTimer->start(200);

    window->show();
}

void BitcoinApplication::createSplashScreen(const NetworkStyle* networkStyle)
{
}

void BitcoinApplication::startThread()
{
    if (coreThread)
        return;
    coreThread = new QThread(this);
    BitcoinCore* executor = new BitcoinCore();
    executor->moveToThread(coreThread);

    /*  communication to and from thread */
    connect(executor, SIGNAL(initializeResult(int)), this, SLOT(initializeResult(int)));
    connect(executor, SIGNAL(shutdownResult(int)), this, SLOT(shutdownResult(int)));
    connect(executor, SIGNAL(runawayException(QString)), this, SLOT(handleRunawayException(QString)));
    connect(this, SIGNAL(requestedInitialize()), executor, SLOT(initialize()));
    connect(this, SIGNAL(requestedShutdown()), executor, SLOT(shutdown()));
    /*  make sure executor object is deleted in its own thread */
    connect(this, SIGNAL(stopThread()), executor, SLOT(deleteLater()));
    connect(this, SIGNAL(stopThread()), coreThread, SLOT(quit()));

    coreThread->start();
}

void BitcoinApplication::parameterSetup()
{
    InitLogging();
    InitParameterInteraction();
}

void BitcoinApplication::requestInitialize()
{
    qDebug() << __func__ << ": Requesting initialize";
    startThread();
    Q_EMIT requestedInitialize();
}

void BitcoinApplication::requestShutdown()
{
    qDebug() << __func__ << ": Requesting shutdown";
    startThread();
    window->hide();
    window->setClientModel(0);
    pollShutdownTimer->stop();

    shutDownRequested = true;

#ifdef ENABLE_WALLET
    window->removeAllWallets();
    delete walletModel;
    walletModel = 0;
#endif
    delete clientModel;
    clientModel = 0;

    ShutdownWindow::showShutdownWindow(window);

    Q_EMIT requestedShutdown();
}

void BitcoinApplication::initializeResult(int retval)
{
    qDebug() << __func__ << ": Initialization result: " << retval;

    returnValue = retval ? 0 : 1;
    if (retval && shutDownRequested == false) {

        qWarning() << "Platform customization:" << platformStyle->getName();
#ifdef ENABLE_WALLET
        PaymentServer::LoadRootCAs();
        paymentServer->setOptionsModel(optionsModel);
#endif

        clientModel = new ClientModel(optionsModel);
        window->setClientModel(clientModel);

#ifdef ENABLE_WALLET
        if (pwalletMain) {
            walletModel = new WalletModel(platformStyle, pwalletMain, optionsModel);

            window->addWallet(BitcoinGUI::DEFAULT_WALLET, walletModel);
            window->setCurrentWallet(BitcoinGUI::DEFAULT_WALLET);

            connect(walletModel, SIGNAL(coinsSent(CWallet*, SendCoinsRecipient, QByteArray)),
                    paymentServer, SLOT(fetchPaymentACK(CWallet*, const SendCoinsRecipient&, QByteArray)));
        }
#endif

        if (GetBoolArg("-min", false)) {
            window->showMinimized();
        } else {
            window->show();
        }
        Q_EMIT splashFinished(window);

#ifdef ENABLE_WALLET

        connect(paymentServer, SIGNAL(receivedPaymentRequest(SendCoinsRecipient)),
                window, SLOT(handlePaymentRequest(SendCoinsRecipient)));
        connect(window, SIGNAL(receivedURI(QString)),
                paymentServer, SLOT(handleURIOrFile(QString)));
        connect(paymentServer, SIGNAL(message(QString, QString, unsigned int)),
                window, SLOT(message(QString, QString, unsigned int)));
        QTimer::singleShot(100, paymentServer, SLOT(uiReady()));
#endif
    } else {
        quit(); // Exit main loop
    }
}

void BitcoinApplication::shutdownResult(int retval)
{
    qDebug() << __func__ << ": Shutdown result: " << retval;
    quit(); // Exit main loop after shutdown finished
}

void BitcoinApplication::handleRunawayException(const QString& message)
{
    QMessageBox::critical(0, "Runaway exception", BitcoinGUI::tr("A fatal error occurred. Bitcoin can no longer continue safely and will quit.") + QString("\n\n") + message);
    ::exit(1);
}

WId BitcoinApplication::getMainWinId() const
{
    if (!window)
        return 0;

    return window->winId();
}

#ifndef BITCOIN_QT_TEST
int main(int argc, char* argv[])
{
    SetupEnvironment();

    ParseParameters(argc, argv);

#if QT_VERSION < 0x050000

    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForTr());
#endif

    Q_INIT_RESOURCE(bitcoin);
    Q_INIT_RESOURCE(bitcoin_locale);

#if QT_VERSION > 0x050100

    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if QT_VERSION >= 0x050600
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
#ifdef Q_OS_MAC
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    BitcoinApplication app(argc, argv);
#if QT_VERSION >= 0x050500

    QSslConfiguration sslconf = QSslConfiguration::defaultConfiguration();
    sslconf.setProtocol(QSsl::TlsV1_0OrLater);
    QSslConfiguration::setDefaultConfiguration(sslconf);
#endif

    qRegisterMetaType<bool*>();

    qRegisterMetaType<CAmount>("CAmount");

    QApplication::setOrganizationName(QAPP_ORG_NAME);
    QApplication::setOrganizationDomain(QAPP_ORG_DOMAIN);
    QApplication::setApplicationName(QAPP_APP_NAME_DEFAULT);
    GUIUtil::SubstituteFonts(GetLangTerritory());

    GuldenTranslator qtEmptyTranslator(true), qtTranslatorBase, qtTranslator, translatorBase, translator, translatorBaseGulden, translatorGulden;

    QApplication::installTranslator(&qtEmptyTranslator);

    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator, translatorBaseGulden, translatorGulden);
    translationInterface.Translate.connect(Translate);

    if (mapArgs.count("-?") || mapArgs.count("-h") || mapArgs.count("-help") || mapArgs.count("-version")) {
        HelpMessageDialog help(NULL, mapArgs.count("-version"));
        help.showOrPrint();
        return 1;
    }

    if (!Intro::pickDataDirectory())
        return 0;

    if (!boost::filesystem::is_directory(GetDataDir(false))) {
        QMessageBox::critical(0, QObject::tr(PACKAGE_NAME),
                              QObject::tr("Error: Specified data directory \"%1\" does not exist.").arg(QString::fromStdString(mapArgs["-datadir"])));
        return 1;
    }
    try {
        ReadConfigFile(mapArgs, mapMultiArgs);
    }
    catch (const std::exception& e) {
        QMessageBox::critical(0, QObject::tr(PACKAGE_NAME),
                              QObject::tr("Error: Cannot parse configuration file: %1. Only use key=value syntax.").arg(e.what()));
        return false;
    }

    try {
        SelectParams(ChainNameFromCommandLine());
    }
    catch (std::exception& e) {
        QMessageBox::critical(0, QObject::tr(PACKAGE_NAME), QObject::tr("Error: %1").arg(e.what()));
        return 1;
    }
#ifdef ENABLE_WALLET

    PaymentServer::ipcParseCommandLine(argc, argv);
#endif

    QScopedPointer<const NetworkStyle> networkStyle(NetworkStyle::instantiate(QString::fromStdString(Params().NetworkIDString())));
    assert(!networkStyle.isNull());

    QApplication::setApplicationName(networkStyle->getAppName());

    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator, translatorBaseGulden, translatorGulden);

#ifdef ENABLE_WALLET

    if (PaymentServer::ipcSendCommandLine())
        exit(0);

    app.createPaymentServer();
#endif

    app.installEventFilter(new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));
#if QT_VERSION < 0x050000

    qInstallMsgHandler(DebugMessageHandler);
#else
#if defined(Q_OS_WIN)

    qApp->installNativeEventFilter(new WinShutdownMonitor());
#endif

    qInstallMessageHandler(DebugMessageHandler);
#endif

    app.parameterSetup();

    app.createOptionsModel(mapArgs.count("-resetguisettings") != 0);

    uiInterface.InitMessage.connect(InitMessage);

    app.createSplashScreen(networkStyle.data());

    boost::filesystem::path pathLockFile = GetDataDir() / ".lock";
    FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file)
        fclose(file);

    try {
        static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
        if (!lock.try_lock()) {
            QMessageBox::critical(0, QObject::tr(PACKAGE_NAME), QString::fromStdString(strprintf(_("Cannot obtain a lock on data directory %s. %s is probably already running."), GetDataDir().string(), _(PACKAGE_NAME))));
            return 1;
        }
    }
    catch (const boost::interprocess::interprocess_exception& e) {
        QMessageBox::critical(0, QObject::tr(PACKAGE_NAME), QString::fromStdString(strprintf(_("Cannot obtain a lock on data directory %s. %s is probably already running."), GetDataDir().string(), _(PACKAGE_NAME))));
        return 1;
    }

    try {
        app.createWindow(networkStyle.data());

#if defined(Q_OS_WIN) && QT_VERSION >= 0x050000
        WinShutdownMonitor::registerShutdownBlockReason(QObject::tr("%1 didn't yet exit safely...").arg(QObject::tr(PACKAGE_NAME)), (HWND)app.getMainWinId());
#endif
        app.exec();
        app.requestShutdown();
        app.exec();
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(strMiscWarning));
    }
    catch (...) {
        PrintExceptionContinue(NULL, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(strMiscWarning));
    }
    return app.getReturnValue();
}
#endif // BITCOIN_QT_TEST
