#include "receivecoinsdialog.h"
#include <qt/_Gulden/forms/ui_receivecoinsdialog.h>

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "receiverequestdialog.h"
#include "recentrequeststablemodel.h"
#include "walletmodel.h"
#include "wallet/wallet.h"

#include <QAction>
#include <QCursor>
#include <QItemSelection>
#include <QMessageBox>
#include <QScrollBar>
#include <QTextDocument>
#include <QDesktopWidget>

#include "GuldenGUI.h"

//fixme: Future - mingw doesn't work with web engine view - for now we just use webkit everywhere but in future we should use web engine view
//Leaving the code here as it's already done and maybe someone can use it.
#ifdef HAVE_WEBENGINE_VIEW
#include <QWebEngineView>
#elif defined(HAVE_WEBKIT)
#include <QtWebKit>
#include <QWebView>
#include <QWebFrame>
#endif
#include <QMovie>

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

#ifdef HAVE_WEBENGINE_VIEW
class WebEngineView: public QWebEngineView
{
public:
    WebEngineView(QWidget* parent)
    : QWebEngineView(parent)
    {
        
    }

    QWebEngineView* createWindow(QWebEnginePage::WebWindowType type)
    {
        //https://bugreports.qt.io/browse/QTBUG-42216
        return this;
    }
};
#elif defined(HAVE_WEBKIT)
class WebView: public QWebView
{
public:
    WebView(QWidget *p = 0)
    : QWebView(p) 
    , parent(p)
    {
        dismissOnPopup = false;
    }
 
    QWebView *createWindow(QWebPage::WebWindowType type)
    {
        QWebView *webView = new QWebView;
        QWebPage *newWeb = new QWebPage(webView);
        if (type == QWebPage::WebModalDialog)
            webView->setWindowModality(Qt::ApplicationModal);
        webView->setAttribute(Qt::WA_DeleteOnClose, true);
        webView->setPage(newWeb);
        webView->setMinimumSize(800, 600);
        webView->show();
        
        QRect r = webView->geometry();
        r.moveCenter(QApplication::desktop()->availableGeometry().center());
        webView->setGeometry(r);
        
        if (dismissOnPopup)
        {
            QMetaObject::invokeMethod( parent, "cancelRequestPayment" );  
        }
 
        return webView;
    }
    bool dismissOnPopup;
    QWidget* parent;
};
#endif


ReceiveCoinsDialog::ReceiveCoinsDialog(const PlatformStyle *platformStyle, QWidget *parent)
: QDialog( parent )
, ui( new Ui::ReceiveCoinsDialog )
, model( 0 )
, platformStyle( platformStyle )
#if defined(HAVE_WEBENGINE_VIEW) || defined(HAVE_WEBKIT)
, buyView( NULL )
#endif
, buyReceiveAddress( NULL )
, currentAccount( NULL )
{
    ui->setupUi(this);
    
    ui->accountRequestPaymentButton->setCursor(Qt::PointingHandCursor);
    ui->accountBuyGuldenButton->setCursor(Qt::PointingHandCursor);
    ui->accountBuyButton->setCursor(Qt::PointingHandCursor);
    ui->accountSaveQRButton->setCursor(Qt::PointingHandCursor);
    ui->accountCopyToClipboardButton->setCursor(Qt::PointingHandCursor);
    ui->cancelButton->setCursor(Qt::PointingHandCursor);
    ui->closeButton->setCursor(Qt::PointingHandCursor);
    ui->generateRequestButton->setCursor(Qt::PointingHandCursor);
    ui->generateAnotherRequestButton->setCursor(Qt::PointingHandCursor);
    
    
    connect(ui->accountCopyToClipboardButton, SIGNAL(clicked()), this, SLOT(copyAddressToClipboard()));
    connect(ui->accountBuyGuldenButton, SIGNAL(clicked()), this, SLOT(showBuyGuldenDialog()));
    connect(ui->accountBuyButton, SIGNAL(clicked()), this, SLOT(buyGulden()));
    connect(ui->accountSaveQRButton, SIGNAL(clicked()), this, SLOT(saveQRAsImage()));
    connect(ui->accountRequestPaymentButton, SIGNAL(clicked()), this, SLOT(gotoRequestPaymentPage()));
    connect(ui->generateAnotherRequestButton, SIGNAL(clicked()), this, SLOT(gotoRequestPaymentPage()));
    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(cancelRequestPayment()));
    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(cancelRequestPayment()));
    
    connect( ui->generateRequestButton, SIGNAL(clicked()), this, SLOT(generateRequest()) );
    
    updateAddress("");    
    
    gotoReceievePage();
    
    #ifdef HAVE_WEBENGINE_VIEW
    buyView = new WebEngineView( this );
    buyView->setAttribute(Qt::WA_TranslucentBackground);
    ui->buyGuldenPageLayout->addWidget( buyView );
    buyView->show();
    
    ui->loadingAnimationLabel->setObjectName("buy_page_error_text");
    #elif defined(HAVE_WEBKIT)
    buyView = new WebView( this );
    buyView->settings()->setAttribute(QWebSettings::JavascriptEnabled, true);
    buyView->settings()->setAttribute(QWebSettings::JavascriptCanOpenWindows, true);
    buyView->settings()->setAttribute(QWebSettings::JavascriptCanCloseWindows, true);
    buyView->settings()->setAttribute(QWebSettings::PrivateBrowsingEnabled, true);
    buyView->settings()->setAttribute(QWebSettings::JavascriptCanAccessClipboard, false);
    buyView->settings()->setAttribute(QWebSettings::SpatialNavigationEnabled, true);
    buyView->settings()->setAttribute(QWebSettings::LocalContentCanAccessRemoteUrls, true);
    buyView->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
        
    //fixme: (FUT) (1.6.1)- figure out why this is necessary on osx.
    #ifdef MAC_OSX
    QSslConfiguration sslCfg = QSslConfiguration::defaultConfiguration();
    QList<QSslCertificate> ca_list = sslCfg.caCertificates();
    QList<QSslCertificate> ca_new = QSslCertificate::fromData("CaCertificates");
    ca_list += ca_new;

    sslCfg.setCaCertificates(ca_list);
    sslCfg.setProtocol(QSsl::AnyProtocol);
    QSslConfiguration::setDefaultConfiguration(sslCfg);
    
    connect(buyView->page()->networkAccessManager(), SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> & )), this, SLOT(sslErrorHandler(QNetworkReply*, const QList<QSslError> & )));
    #endif

    //buyView->setAttribute(Qt::WA_TranslucentBackground);
    ui->buyGuldenPageLayout->addWidget( buyView );
    buyView->show();
    
    ui->loadingAnimationLabel->setObjectName("buy_page_error_text");
    #else
    ui->accountBuyGuldenButton->setVisible(false);
    #endif
}

void ReceiveCoinsDialog::updateAddress(const QString& address)
{
    accountAddress = address;
    ui->accountAddress->setText(accountAddress);
    updateQRCode(accountAddress);
}

void ReceiveCoinsDialog::setActiveAccount(CAccount* account)
{
    if (account != currentAccount)
    {
        gotoReceievePage();
    }
    currentAccount = account;
}

void ReceiveCoinsDialog::activeAccountChanged()
{
    LogPrintf("ReceiveCoinsDialog::activeAccountChanged\n");
    
    setActiveAccount(model->getActiveAccount());
}

void ReceiveCoinsDialog::updateQRCode(const QString& uri)
{
    if(!uri.isEmpty())
    {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH)
        {
            ui->requestQRImage->setFixedWidth(900);
            ui->addressQRImage->setText(tr("Resulting URI too long, try to reduce the text for label / message."));
        }
        else
        {
            ui->requestQRImage->setText("");
            ui->requestQRImage->setFixedWidth(ui->requestQRImage->height());
            ui->addressQRImage->setCode(uri);
        }
    }
}

void ReceiveCoinsDialog::setModel(WalletModel *model)
{
    this->model = model;

    connect(model, SIGNAL(activeAccountChanged()), this, SLOT(activeAccountChanged()));
    activeAccountChanged();
    
    if(model && model->getOptionsModel())
    {
    }
}

ReceiveCoinsDialog::~ReceiveCoinsDialog()
{
    delete ui;
}

void ReceiveCoinsDialog::copyAddressToClipboard()
{
    if (ui->receiveCoinsStackedWidget->currentIndex() == 0)
    {
        GUIUtil::setClipboard(accountAddress);
    }
    else if (ui->receiveCoinsStackedWidget->currentIndex() == 3)
    {
        GUIUtil::setClipboard(ui->labelPaymentRequest->text());
    }
}


void ReceiveCoinsDialog::saveQRAsImage()
{
    if (ui->receiveCoinsStackedWidget->currentIndex() == 0)
    {
        ui->addressQRImage->saveImage();
    }
    else if (ui->receiveCoinsStackedWidget->currentIndex() == 3)
    {
        ui->requestQRImage->saveImage();
    }
}

void ReceiveCoinsDialog::gotoReceievePage()
{
    ui->receiveCoinsStackedWidget->setCurrentIndex(0);
    ui->requestLabel->setText("");
    ui->requestAmount->clear();

    ui->accountRequestPaymentButtonComposite->setVisible(true);
    ui->accountBuyGuldenButton->setVisible(true);
    ui->accountSaveQRButtonComposite->setVisible(true);
    ui->accountCopyToClipboardButtonComposite->setVisible(true);
    ui->cancelButton->setVisible(false);
    ui->closeButton->setVisible(false);
    ui->cancelButtonGroup->setVisible(false);
    ui->generateRequestButton->setVisible(false);
    ui->generateAnotherRequestButton->setVisible(false);
    ui->accountBuyButton->setVisible(false);
    
    ui->accountCopyToClipboardButton->setText(tr("Copy address to clipboard"));
}


void ReceiveCoinsDialog::showBuyGuldenDialog()
{
    #if defined(HAVE_WEBENGINE_VIEW) || defined(HAVE_WEBKIT)
    ui->receiveCoinsStackedWidget->setCurrentIndex(1);

    ui->accountRequestPaymentButtonComposite->setVisible(false);
    ui->accountBuyGuldenButton->setVisible(false);
    ui->accountSaveQRButtonComposite->setVisible(false);
    ui->accountCopyToClipboardButtonComposite->setVisible(false);
    ui->cancelButton->setVisible(true);
    ui->closeButton->setVisible(false);
    ui->cancelButtonGroup->setVisible(true);
    ui->generateRequestButton->setVisible(false);
    ui->generateAnotherRequestButton->setVisible(false);
    ui->accountBuyButton->setVisible(true);

    
    
    QMovie *movie = new QMovie(":/Gulden/loading_animation");
    if ( movie->isValid() )
    {
        ui->loadingAnimationLabel->setVisible(true);
        buyView->setVisible(false);
        movie->setScaledSize(QSize(30, 30));
        ui->loadingAnimationLabel->setMovie(movie);
        movie->start();
    }
    else
    {
        ui->loadingAnimationLabel->setVisible(false);
        buyView->setVisible(true);
        delete movie;
    }    
    
    buyView->load(QUrl("https://gulden.com/purchase"));
    
    #if defined(HAVE_WEBENGINE_VIEW)
    buyView->page()->setBackgroundColor(Qt::transparent);
    #else
    QPalette palette = buyView->palette();
    palette.setBrush(QPalette::Base, Qt::transparent);
    buyView->page()->setPalette(palette);
    buyView->setAttribute(Qt::WA_OpaquePaintEvent, false);
    buyView->page()->setLinkDelegationPolicy(QWebPage::DontDelegateLinks);
    #endif
    
    connect(buyView, SIGNAL( loadFinished(bool) ), this, SLOT( loadBuyViewFinished(bool) ) );
    #endif
}

void ReceiveCoinsDialog::gotoRequestPaymentPage()
{
    ui->receiveCoinsStackedWidget->setCurrentIndex(2);

    ui->accountRequestPaymentButtonComposite->setVisible(false);
    ui->accountBuyGuldenButton->setVisible(false);
    ui->accountSaveQRButtonComposite->setVisible(false);
    ui->accountCopyToClipboardButtonComposite->setVisible(false);
    ui->cancelButton->setVisible(true);
    ui->closeButton->setVisible(false);
    ui->cancelButtonGroup->setVisible(true);
    ui->generateRequestButton->setVisible(true);
    ui->generateAnotherRequestButton->setVisible(false);
    ui->accountBuyButton->setVisible(false);
}

void ReceiveCoinsDialog::generateRequest()
{
    //fixme: (HD) kep gaps
    CReserveKey reservekey(pwalletMain, model->getActiveAccount(), KEYCHAIN_EXTERNAL);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey))
        return;
    reservekey.KeepKey();
    
    
    ui->receiveCoinsStackedWidget->setCurrentIndex(3);
    ui->accountRequestPaymentButtonComposite->setVisible(false);
    ui->accountBuyGuldenButton->setVisible(false);
    ui->accountSaveQRButtonComposite->setVisible(true);
    ui->accountCopyToClipboardButtonComposite->setVisible(true);
    ui->cancelButton->setVisible(false);
    ui->closeButton->setVisible(true);
    ui->cancelButtonGroup->setVisible(true);
    ui->generateRequestButton->setVisible(false);
    ui->generateAnotherRequestButton->setVisible(true);
    ui->accountBuyButton->setVisible(false);
    
    ui->accountCopyToClipboardButton->setText(tr("Copy request to clipboard"));
    
    CAmount amount = ui->requestAmount->value();    
    if (amount > 0)
    {
        ui->labelPaymentRequestHeading->setText(tr("Request %1 Gulden").arg(BitcoinUnits::format( BitcoinUnits::BTC, amount, false, BitcoinUnits::separatorStandard, 2 )));
    }
    else
    {
        ui->labelPaymentRequestHeading->setText(tr("Request Gulden"));
    }
        
    QString args;
    QString label =  QUrl::toPercentEncoding(ui->requestLabel->text());
    QString strAmount;
    if (!label.isEmpty())
    {
        label = "label=" + label;
    }
    if (amount > 0)
    {
        strAmount = "amount=" +  QUrl::toPercentEncoding(BitcoinUnits::format( BitcoinUnits::BTC, amount, false, BitcoinUnits::separatorNever, -1 ));
        //Trim trailing decimal zeros
        while(strAmount.endsWith("0"))
        {
            strAmount.chop(1);
        }
        if(strAmount.endsWith("."))
        {
            strAmount.chop(1);
        }
    }
    if (!strAmount.isEmpty() && !label.isEmpty())
    {
        args = "?" + label + "&" + strAmount;
    }
    else if(!strAmount.isEmpty())
    {
        args = "?" + strAmount;
    }
    else if(!label.isEmpty())
    {
        args = "?" + label;
    }
    
    QString uri = QString("Gulden:") + QString::fromStdString(CBitcoinAddress(vchPubKey.GetID()).ToString()) + args;
    ui->labelPaymentRequest->setText( uri );    
    if(!uri.isEmpty())
    {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH)
        {
            ui->requestQRImage->setFixedWidth(900);
            ui->requestQRImage->setText(tr("Resulting URI too long, try to reduce the text for the label."));
        }
        else
        {
            ui->requestQRImage->setText("");
            ui->requestQRImage->setFixedWidth(ui->requestQRImage->height());
            ui->requestQRImage->setCode(uri);

        }
    }
    
    pwalletMain->SetAddressBook(CBitcoinAddress(vchPubKey.GetID()).ToString(), ui->requestLabel->text().toStdString(), "receive");
}


void ReceiveCoinsDialog::cancelRequestPayment()
{
    ui->requestLabel->setText("");
    ui->requestAmount->clear();
    #if defined(HAVE_WEBENGINE_VIEW) || defined(HAVE_WEBKIT)
    buyView->load(QUrl("about:blank"));
    #endif
    gotoReceievePage();
}


void ReceiveCoinsDialog::buyGulden()
{
    #if defined(HAVE_WEBENGINE_VIEW)
    buyView->page()->runJavaScript(QString("$('#submit').show().focus().click().hide()"));
    #else
    ((WebView*)buyView)->dismissOnPopup=true;
    buyView->page()->mainFrame()->evaluateJavaScript(QString("$('#submit').show().focus().click().hide()"));
    #endif
}

void ReceiveCoinsDialog::loadBuyViewFinished(bool bOk)
{    
    #if defined(HAVE_WEBENGINE_VIEW) || defined(HAVE_WEBKIT)
    if (bOk)
    {
        if (buyReceiveAddress)
        {
            delete buyReceiveAddress;
            buyReceiveAddress = NULL;
        }
        
        buyReceiveAddress = new CReserveKey(pwalletMain, currentAccount, KEYCHAIN_EXTERNAL);
        CPubKey pubKey;
        buyReceiveAddress->GetReservedKey(pubKey);
        CKeyID keyID = pubKey.GetID();
            
        QString guldenAddress = QString::fromStdString(CBitcoinAddress(keyID).ToString());
        //fixme: (FUT) (1.6.1) fill proper email address etc. here
        QString emailAddress = QString("");
        QString paymentMethod = QString("");
        QString paymentDetails = QString("");
        #if defined(HAVE_WEBENGINE_VIEW)
        buyView->page()->runJavaScript(QString("NocksBuyFormFillDetails('%1', '%2')").arg(guldenAddress, emailAddress));
        buyView->page()->runJavaScript(QString("$('.extra-row').hide()"));
        #else
        QVariant ret = buyView->page()->mainFrame()->evaluateJavaScript(QString("NocksBuyFormFillDetails('%1', '%2')").arg(guldenAddress, emailAddress));
        QString temp = ret.toString();
        buyView->page()->mainFrame()->evaluateJavaScript(QString("$('.extra-row').hide()"));
        #endif
        
        //https://bugreports.qt.io/browse/QTBUG-42216
        #if defined(HAVE_WEBENGINE_VIEW)
        #if QT_VERSION < QT_VERSION_CHECK(5, 6, 2) || QT_VERSION == QT_VERSION_CHECK(5, 7, 0)
        buyView->page()->runJavaScript(QString("$('a[href]').attr('target', '_blank');"));
        buyView->page()->runJavaScript(QString("$('form').attr('target', '_blank');"));
        #endif
        #else
        buyView->page()->mainFrame()->evaluateJavaScript(QString("$('a[href]').attr('target', '_blank');"));
        buyView->page()->mainFrame()->evaluateJavaScript(QString("$('form').attr('target', '_blank');"));
        #endif
        
        //Force the WebView to have the fonts we want - it doesn't pick up our registered fonts otherwise.
        {
            QFile fontFile(":/Gulden/fontawesome");
            fontFile.open(QIODevice::ReadOnly);
            QString rawCSS = "@font-face{ font-family: FontAwesome;src: url(data:font/ttf;base64," + fontFile.readAll().toBase64() + ") format('truetype');";
            std::string encodedCSS = EncodeBase64(rawCSS.toStdString());
            QString insertFontScript = QString("(function() {") +
                    "var parent = document.getElementsByTagName('head').item(0);" +
                    "var style = document.createElement('style');" +
                    "style.type = 'text/css';" +
                    "style.innerHTML = window.atob('" + encodedCSS.c_str() + "');" +
                    "parent.appendChild(style)" +
                    "})()";
            #if defined(HAVE_WEBENGINE_VIEW)
            buyView->page()->runJavaScript(insertFontScript);
            #else
            buyView->page()->mainFrame()->evaluateJavaScript(insertFontScript);
            #endif
        }
    
        ui->loadingAnimationLabel->setVisible(false);
        buyView->setVisible(true);
    }
    else
    {
        ui->loadingAnimationLabel->setText(tr("Error loading the buy page, please check your connection and try again later."));
        ui->loadingAnimationLabel->setVisible(true);
        buyView->setVisible(false);
    }
    #endif
}

void ReceiveCoinsDialog::sslErrorHandler(QNetworkReply* qnr, const QList<QSslError> & errlist)
{
    qnr->ignoreSslErrors();
}

