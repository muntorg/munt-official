// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "amountfield.h"

#include "units.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "qvaluecombobox.h"

#include <QApplication>
#include <QAbstractSpinBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLabel>

#include "optionsmodel.h"
#include "_Gulden/ticker.h"
#include "_Gulden/GuldenGUI.h"
#include "_Gulden/clickablelabel.h"
#include <assert.h>

const int DISPLAY_DECIMALS = 2;

/** QSpinBox that uses fixed-point numbers internally and uses our own
 * formatting/parsing functions.
 */
class AmountSpinBox: public QAbstractSpinBox
{
    Q_OBJECT

public:
    explicit AmountSpinBox(QWidget *parent):
        QAbstractSpinBox(parent),
        currentUnit(GuldenUnits::NLG),
        singleStep(100000000) // satoshis
    {
        setAlignment(Qt::AlignRight);

        connect(lineEdit(), SIGNAL(textEdited(QString)), this, SIGNAL(valueChanged()));
    }

    QValidator::State validate(QString &text, [[maybe_unused]] int &pos) const
    {
        if(text.isEmpty())
            return QValidator::Intermediate;
        bool valid = false;
        parse(text, &valid);
        /* Make sure we return Intermediate so that fixup() is called on defocus */
        return valid ? QValidator::Intermediate : QValidator::Invalid;
    }

    //fixme: (Post-2.1) - hardcoded to point - but so is parse(), we should fix this to be comma for some locales
    int getCurrentDecimalPlaces(const QString& val) const
    {
        int pos = val.indexOf(".");;
        int currentDecimalPlaces = -1;
        if (pos != -1)
        {
            currentDecimalPlaces = val.length() - pos  -1;
            if (currentDecimalPlaces == 0)
                currentDecimalPlaces = 1;
        }
        else
        {
            currentDecimalPlaces = 0;
        }
        return currentDecimalPlaces;
    }

    //fixme: (Post-2.1) - hardcoded to point - but so is parse(), we should fix this to be comma for some locales
    void trimTailingZerosToCurrentDecimalPlace(QString& val, int currentDecimalPlaces) const
    {
        if (currentDecimalPlaces != -1)
        {
            int pos = val.indexOf(".");;
            if (pos != -1)
            {
                int places = val.length() - pos;
                while (--places > currentDecimalPlaces)
                {
                    if (val[ val.length()-1 ] == '0')
                    {
                        val.chop(1);
                    }
                    else
                    {
                        break;
                    }
                }
                if (val[ val.length()-1 ] == '.')
                {
                    val.chop(1);
                }
            }
        }
    }



    void fixup(QString &input) const
    {
        bool valid = false;
        CAmount val = parse(input, &valid);
        if(valid)
        {
            int currentDecimalPlaces = getCurrentDecimalPlaces(input);
            input = GuldenUnits::format(currentUnit, val, false, GuldenUnits::separatorAlways, currentDecimalPlaces);
            trimTailingZerosToCurrentDecimalPlace(input, currentDecimalPlaces);
            lineEdit()->setText(input);
        }
    }

    CAmount value(bool *valid_out=0) const
    {
        return parse(text(), valid_out);
    }

    CAmount value(int& currentDecimalPlaces, bool *valid_out=0) const
    {
        QString val(text());
        currentDecimalPlaces = getCurrentDecimalPlaces(val);
        return parse(val, valid_out);
    }

    void setValue(const CAmount& value, int limitDecimals=-1)
    {
        QString val = GuldenUnits::format(currentUnit, value, false, GuldenUnits::separatorAlways, limitDecimals);
        trimTailingZerosToCurrentDecimalPlace(val, limitDecimals);
        lineEdit()->setText(val);
    }

    void stepBy(int steps)
    {
        bool valid = false;
        int currentDecimalPlaces;
        CAmount val = value(currentDecimalPlaces, &valid);
        val = val + steps * singleStep;
        val = qMin(qMax(val, CAmount(0)), GuldenUnits::maxMoney());
        setValue(val, currentDecimalPlaces);
        Q_EMIT valueChanged();
    }

    void setDisplayUnit(int unit)
    {
        bool valid = false;
        CAmount val = value(&valid);

        currentUnit = unit;

        if(valid)
            setValue(val);
        else
            clear();
    }

    void setSingleStep(const CAmount& step)
    {
        singleStep = step;
    }

    QSize minimumSizeHint() const
    {
        if(cachedMinimumSizeHint.isEmpty())
        {
            ensurePolished();

            const QFontMetrics fm(fontMetrics());
            int h = lineEdit()->minimumSizeHint().height();
            int w = fm.width(GuldenUnits::format(GuldenUnits::NLG, GuldenUnits::maxMoney(), false, GuldenUnits::separatorAlways));
            w += 2; // cursor blinking space

            QStyleOptionSpinBox opt;
            initStyleOption(&opt);
            QSize hint(w, h);
            QSize extra(35, 6);
            opt.rect.setSize(hint + extra);
            extra += hint - style()->subControlRect(QStyle::CC_SpinBox, &opt,
                                                    QStyle::SC_SpinBoxEditField, this).size();
            // get closer to final result by repeating the calculation
            opt.rect.setSize(hint + extra);
            extra += hint - style()->subControlRect(QStyle::CC_SpinBox, &opt,
                                                    QStyle::SC_SpinBoxEditField, this).size();
            hint += extra;
            hint.setHeight(h);

            opt.rect = rect();

            cachedMinimumSizeHint = style()->sizeFromContents(QStyle::CT_SpinBox, &opt, hint, this)
                                    .expandedTo(QApplication::globalStrut());
        }
        return cachedMinimumSizeHint;
    }

private:
    int currentUnit;
    CAmount singleStep;
    mutable QSize cachedMinimumSizeHint;

    /**
     * Parse a string into a number of base monetary units and
     * return validity.
     * @note Must return 0 if !valid.
     */
    CAmount parse(const QString &text, bool *valid_out=0) const
    {
        CAmount val = 0;
        bool valid = GuldenUnits::parse(currentUnit, text, &val);
        if(valid)
        {
            if(val < 0 || val > GuldenUnits::maxMoney())
                valid = false;
        }
        if(valid_out)
            *valid_out = valid;
        return valid ? val : 0;
    }

protected:
    bool event(QEvent *event)
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Comma)
            {
                // Translate a comma into a period
                QKeyEvent periodKeyEvent(event->type(), Qt::Key_Period, keyEvent->modifiers(), ".", keyEvent->isAutoRepeat(), keyEvent->count());
                return QAbstractSpinBox::event(&periodKeyEvent);
            }
        }
        return QAbstractSpinBox::event(event);
    }

    StepEnabled stepEnabled() const
    {
        if (isReadOnly()) // Disable steps when AmountSpinBox is read-only
            return StepNone;
        if (text().isEmpty()) // Allow step-up with empty field
            return StepUpEnabled;

        StepEnabled rv = 0;
        bool valid = false;
        CAmount val = value(&valid);
        if(valid)
        {
            if(val > 0)
                rv |= StepDownEnabled;
            if(val < GuldenUnits::maxMoney())
                rv |= StepUpEnabled;
        }
        return rv;
    }

Q_SIGNALS:
    void valueChanged();
};

#include "amountfield.moc"

// This lookup of short name for currency code could be usefull more general
// and be extended with more currency names.
const QString ShortNameForCurrencyCode(const std::string& currencyCode)
{
    static std::map<std::string, QString> currencyNames = {
        {"NLG", "Gulden"},
        {"EUR", "Euro"},
        {"BTC", "Bitcoin"}
    };

    const auto it = currencyNames.find(currencyCode);
    if ( it != currencyNames.end())
        return it->second;

    // fallback to just using the currency short code
    return QString::fromStdString(currencyCode);
}

GuldenAmountField::GuldenAmountField(QWidget *parent) :
    QWidget(parent),
    primaryCurrency(Currency::Euro),
    amountGulden(0),
    amountEuro(0),
    amountLocal(0),
    optionsModel(nullptr),
    ticker(nullptr)
{
    primaryCurrency = Currency::Gulden;

    QHBoxLayout *layout = new QHBoxLayout(this);

    primaryAmountDisplay = new AmountSpinBox(this);
    primaryAmountDisplay->setLocale(QLocale::c());
    primaryAmountDisplay->installEventFilter(this);
    primaryAmountDisplay->setMaximumWidth(170);
    primaryAmountDisplay->setButtonSymbols(QAbstractSpinBox::NoButtons);
    layout->addWidget(primaryAmountDisplay);

    primaryAmountName = new QLabel(this);
    primaryAmountName->setText(tr("Gulden"));
    layout->addWidget(primaryAmountName);

    amountSeperator = new ClickableLabel(this);
    amountSeperator->setTextFormat( Qt::RichText );
    amountSeperator->setText(GUIUtil::fontAwesomeRegular("\uf0EC"));
    amountSeperator->setObjectName("amountSeperator");
    amountSeperator->setCursor(Qt::PointingHandCursor);
    amountSeperator->setContentsMargins(0,0,0,0);
    amountSeperator->setIndent(0);
    amountSeperator->setVisible(false);
    layout->addWidget(amountSeperator);

    firstAuxAmountDisplay = new ClickableLabel(this);
    firstAuxAmountDisplay->setText(QString("€\u20090.00"));
    firstAuxAmountDisplay->setObjectName("secondaryAmountDisplay");
    firstAuxAmountDisplay->setCursor(Qt::PointingHandCursor);
    firstAuxAmountDisplay->setContentsMargins(0,0,0,0);
    firstAuxAmountDisplay->setIndent(0);
    firstAuxAmountDisplay->setVisible(false);
    layout->addWidget(firstAuxAmountDisplay);

    secondAuxAmountDisplay = new ClickableLabel(this);
    secondAuxAmountDisplay->setText(QString("(\uF15A\u20090.00)"));
    secondAuxAmountDisplay->setObjectName("tertiaryAmountDisplay");
    secondAuxAmountDisplay->setCursor(Qt::PointingHandCursor);
    secondAuxAmountDisplay->setContentsMargins(0,0,0,0);
    secondAuxAmountDisplay->setIndent(0);
    secondAuxAmountDisplay->setVisible(false);
    layout->addWidget(secondAuxAmountDisplay);

    layout->addStretch(1);
    layout->setContentsMargins(0,0,0,0);

    setLayout(layout);

    setFocusPolicy(Qt::TabFocus);
    setFocusProxy(primaryAmountDisplay);

    connect(primaryAmountDisplay, SIGNAL(valueChanged()), this, SLOT(primaryValueChanged()));
    connect(firstAuxAmountDisplay, SIGNAL(clicked()), this, SLOT(changeToFirstAuxCurrency()));
    connect(amountSeperator, SIGNAL(clicked()), this, SLOT(changeToFirstAuxCurrency()));
    connect(secondAuxAmountDisplay, SIGNAL(clicked()), this, SLOT(changeToSecondAuxCurrency()));
}

GuldenAmountField::~GuldenAmountField()
{
    disconnect(primaryAmountDisplay, SIGNAL(valueChanged()), this, SLOT(primaryValueChanged()));
    disconnect(firstAuxAmountDisplay, SIGNAL(clicked()), this, SLOT(changeToFirstAuxCurrency()));
    disconnect(amountSeperator, SIGNAL(clicked()), this, SLOT(changeToFirstAuxCurrency()));
    disconnect(secondAuxAmountDisplay, SIGNAL(clicked()), this, SLOT(changeToSecondAuxCurrency()));

    if (ticker)
        ticker->disconnect(this);

    if (optionsModel)
        optionsModel->guldenSettings->disconnect(this);
}

CAmount GuldenAmountField::amount(const Currency currency) const
{
    switch (currency) {
    case Currency::Gulden:
        return amountGulden;
    case Currency::Euro:
        return amountEuro;
    case Currency::Local:
        return amountLocal;
    default:
        return CAmount(0);
    }
}

void GuldenAmountField::setAmount(const CAmount& value)
{
    amountGulden = value;
    updatePrimaryFromData();
    if (ticker)
    {
        amountEuro = ticker->convertGuldenToForex(amountGulden, CurrencyCode(Currency::Euro));
        amountLocal = ticker->convertGuldenToForex(amountGulden, CurrencyCode(Currency::Local));
        updateAuxilaryFromData();
    }
}

void GuldenAmountField::setSingleStep(const CAmount& step)
{
    primaryAmountDisplay->setSingleStep(step);
}

void GuldenAmountField::setReadOnly(bool fReadOnly)
{
    primaryAmountDisplay->setReadOnly(fReadOnly);
}

void GuldenAmountField::setValid(bool valid)
{
    if (valid)
        primaryAmountDisplay->setStyleSheet("");
    else
        primaryAmountDisplay->setStyleSheet(STYLE_INVALID);
}

void GuldenAmountField::clear()
{
    primaryAmountDisplay->clear();
    amountGulden = amountEuro = amountLocal = 0;
    updateAuxilaryFromData();
}

void GuldenAmountField::setEnabled(bool fEnabled)
{
    primaryAmountDisplay->setEnabled(fEnabled);
    primaryAmountName->setEnabled(fEnabled);
}

void GuldenAmountField::setPrimaryDisplayCurrency(const Currency currency)
{
    primaryCurrency = currency;
    primaryAmountName->setText( ShortNameForCurrencyCode( CurrencyCode(primaryCurrency) ) );
    updatePrimaryFromData();
    updateAuxilaryFromData();
}

void GuldenAmountField::setOptionsModel(OptionsModel* optionsModel_)
{
    if (!optionsModel && optionsModel_ && optionsModel_->getTicker())
    {
        optionsModel = optionsModel_;
        ticker = optionsModel_->getTicker();
        connect( ticker, SIGNAL(destroyed(QObject*)), this, SLOT(tickerOrOptionsDestroyed(QObject*)) );
        connect( optionsModel, SIGNAL(destroyed(QObject*)), this, SLOT(tickerOrOptionsDestroyed(QObject*)) );
        connect( ticker, SIGNAL( exchangeRatesUpdatedLongPoll() ), this, SLOT( exchangeRateUpdate() ) );

        connect(optionsModel->guldenSettings, SIGNAL( localCurrencyChanged(QString)), this, SLOT( localCurrencyChanged(QString) ) );

        firstAuxAmountDisplay->setVisible(true);
        amountSeperator->setVisible(true);
        amountEuro = ticker->convertGuldenToForex(amountGulden, CurrencyCode(Currency::Euro));
        amountLocal = ticker->convertGuldenToForex(amountGulden, CurrencyCode(Currency::Local));
        updateAuxilaryFromData();
    }
}

QWidget *GuldenAmountField::setupTabChain(QWidget *prev)
{
    QWidget::setTabOrder(prev, primaryAmountDisplay);
    QWidget::setTabOrder(primaryAmountDisplay, primaryAmountName);
    return primaryAmountName;
}

bool GuldenAmountField::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        // Clear invalid flag on focus
        setValid(true);
    }
    return QWidget::eventFilter(object, event);
}

void GuldenAmountField::updatePrimaryFromData()
{
    primaryAmountDisplay->setValue(amount(primaryCurrency), DISPLAY_DECIMALS);
}

void GuldenAmountField::updateDataFromPrimary()
{
    CAmount amount = primaryAmountDisplay->value();

    switch (primaryCurrency)
    {
    case Currency::Gulden:
        amountGulden = amount;
        if (ticker) {
            amountEuro = ticker->convertGuldenToForex(amountGulden, CurrencyCode(Currency::Euro));
            amountLocal = ticker->convertGuldenToForex(amountGulden, CurrencyCode(Currency::Local));
        }
        break;
    case Currency::Euro:
        amountEuro = amount;
        if (ticker) {
            amountGulden = ticker->convertForexToGulden(amountEuro, CurrencyCode(Currency::Euro));
            amountLocal = ticker->convertGuldenToForex(amountGulden, CurrencyCode(Currency::Local));
        }
        break;
    case Currency::Local:
        amountLocal = amount;
        if (ticker) {
            amountGulden = ticker->convertForexToGulden(amountLocal, CurrencyCode(Currency::Local));
            amountEuro = ticker->convertGuldenToForex(amountGulden, CurrencyCode(Currency::Euro));
        }
        break;
    default:
        assert(false);
        break;
    }

    updateAuxilaryFromData();
}

void GuldenAmountField::updateAuxilaryFromData()
{
    if (!optionsModel || !ticker)
        return;

    firstAuxAmountDisplay->setText( FormatAuxAmount(firstAuxCurrency()) );
    secondAuxAmountDisplay->setText( QString("(%1)").arg(FormatAuxAmount(secondAuxCurrency())) );
    secondAuxAmountDisplay->setVisible(secondAuxCurrency() != Currency::None);
}

GuldenAmountField::Currency GuldenAmountField::firstAuxCurrency() const
{
    // options and ticker required to display auxilary currencies
    if (!optionsModel || !ticker)
        return Currency::None;

    switch (primaryCurrency) {
    case Currency::Gulden:
        return Currency::Euro;
    case Currency::Euro:
        return Currency::Gulden;
    case Currency::Local:
        return Currency::Gulden;
    default:
        assert(false);
        break;
    }
}

GuldenAmountField::Currency GuldenAmountField::secondAuxCurrency() const
{
    // options and ticker required to display auxilary currencies
    if (!optionsModel || !ticker)
        return Currency::None;

    // no display of 2nd auxilary if local equal to Euro, just first Aux will do
    if (optionsModel->guldenSettings->getLocalCurrency() == "EUR")
        return Currency::None;

    switch (primaryCurrency) {
    case Currency::Gulden:
        return Currency::Local;
    case Currency::Euro:
        return Currency::Local;
    case Currency::Local:
        return Currency::Euro;
    default:
        return Currency::None;
    }
}

std::string GuldenAmountField::CurrencyCode(const Currency currency) const
{
    switch (currency) {
    case Currency::Gulden:
        return "NLG";
    case Currency::Euro:
        return "EUR";
    case Currency::Local:
        if (!optionsModel)
            return "EUR";
        return optionsModel->guldenSettings->getLocalCurrency().toStdString();
    default:
        return "EUR";
    }
}

QString GuldenAmountField::FormatAuxAmount(const Currency currency) const
{
    if (currency == Currency::None)
        return "";

    if (amountGulden > 0)
    {
        return QString::fromStdString(CurrencySymbolForCurrencyCode( CurrencyCode(currency) ))
                                        + QString("\u2009")
                                        + GuldenUnits::format(GuldenUnits::NLG,
                                                              amount(currency),
                                                              false, GuldenUnits::separatorAlways, 2);
    }
    else
    {
        return QString::fromStdString(CurrencySymbolForCurrencyCode( CurrencyCode(currency) ))
                                       + QString("\u2009") + QString("0.00");
    }
}

void GuldenAmountField::changeToFirstAuxCurrency()
{
    setPrimaryDisplayCurrency(firstAuxCurrency());
}

void GuldenAmountField::changeToSecondAuxCurrency()
{
    setPrimaryDisplayCurrency(secondAuxCurrency());
}

void GuldenAmountField::exchangeRateUpdate()
{
    updateDataFromPrimary();
}

void GuldenAmountField::localCurrencyChanged(QString)
{
    std::string code = CurrencyCode(Currency::Local);

    // adjust local amount to new currency
    amountLocal = ticker->convertGuldenToForex(amountGulden, code);

    // When switching local to Euro while local currency is currently primary
    // make Euro primary instead.
    if (code == "EUR" && primaryCurrency == Currency::Local)
        primaryCurrency = Currency::Euro;

    // force update of all display amounts
    setPrimaryDisplayCurrency(primaryCurrency);
}

void GuldenAmountField::tickerOrOptionsDestroyed(QObject*)
{
    optionsModel = nullptr;
    ticker = nullptr;
    setPrimaryDisplayCurrency(Currency::Gulden);
}

void GuldenAmountField::primaryValueChanged()
{
    updateDataFromPrimary();
    Q_EMIT amountChanged();
}
