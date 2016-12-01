// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoinamountfield.h"

#include "bitcoinunits.h"
#include "guiconstants.h"
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
#include "_Gulden/nocksrequest.h"

/** QSpinBox that uses fixed-point numbers internally and uses our own
 * formatting/parsing functions.
 */
class AmountSpinBox: public QAbstractSpinBox
{
    Q_OBJECT

public:
    explicit AmountSpinBox(QWidget *parent):
        QAbstractSpinBox(parent),
        currentUnit(BitcoinUnits::BTC),
        singleStep(100000000) // satoshis
    {
        setAlignment(Qt::AlignRight);

        connect(lineEdit(), SIGNAL(textEdited(QString)), this, SIGNAL(valueChanged()));
    }

    QValidator::State validate(QString &text, int &pos) const
    {
        if(text.isEmpty())
            return QValidator::Intermediate;
        bool valid = false;
        parse(text, &valid);
        /* Make sure we return Intermediate so that fixup() is called on defocus */
        return valid ? QValidator::Intermediate : QValidator::Invalid;
    }
    
    //fixme: GULDEN - hardcoded to point - but so is parse(), we should fix this to be comma for some locales
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
    
    //fixme: GULDEN - hardcoded to point - but so is parse(), we should fix this to be comma for some locales
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
            input = BitcoinUnits::format(currentUnit, val, false, BitcoinUnits::separatorAlways);
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
        QString val = BitcoinUnits::format(currentUnit, value, false, BitcoinUnits::separatorAlways);
        trimTailingZerosToCurrentDecimalPlace(val, limitDecimals);
        lineEdit()->setText(val);
        Q_EMIT valueChanged();
    }

    void stepBy(int steps)
    {
        bool valid = false;
        int currentDecimalPlaces;
        CAmount val = value(currentDecimalPlaces, &valid);
        val = val + steps * singleStep;
        val = qMin(qMax(val, CAmount(0)), BitcoinUnits::maxMoney());
        setValue(val, currentDecimalPlaces);
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
            int w = fm.width(BitcoinUnits::format(BitcoinUnits::BTC, BitcoinUnits::maxMoney(), false, BitcoinUnits::separatorAlways));
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
        bool valid = BitcoinUnits::parse(currentUnit, text, &val);
        if(valid)
        {
            if(val < 0 || val > BitcoinUnits::maxMoney())
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
            if(val < BitcoinUnits::maxMoney())
                rv |= StepUpEnabled;
        }
        return rv;
    }

Q_SIGNALS:
    void valueChanged();
};

#include "bitcoinamountfield.moc"

BitcoinAmountField::BitcoinAmountField(QWidget *parent) :
    QWidget(parent),
    amount(0),
    optionsModel(NULL),
    ticker(NULL)
{
    nocksRequestBTCtoNLG = NULL;
    nocksRequestEURtoNLG = NULL;
    nocksRequestNLGtoBTC = NULL;
    nocksRequestNLGtoEUR = NULL;
    
    secondaryAmount = CAmount(0);
    tertiaryAmount = CAmount(0);
    quadAmount = CAmount(0);
    
    primaryCurrency = AmountFieldCurrency::CurrencyGulden;
    displayCurrency = AmountFieldCurrency::CurrencyGulden;
    
    amount = new AmountSpinBox(this);
    amount->setLocale(QLocale::c());
    amount->installEventFilter(this);
    amount->setMaximumWidth(170);
    amount->setButtonSymbols(QAbstractSpinBox::NoButtons);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(amount);
    //unit = new QValueComboBox(this);
    //unit->setModel(new BitcoinUnits(this));
    unit = new QLabel(this);
    unit->setText(tr("Gulden"));
    layout->addWidget(unit);
    
    amountSeperator = new ClickableLabel(this);
    amountSeperator->setText(QString("\uf0EC"));
    layout->addWidget(amountSeperator);
    amountSeperator->setObjectName("amountSeperator");
    amountSeperator->setCursor(Qt::PointingHandCursor);
    
    secondaryAmountDisplay = new ClickableLabel(this);
    secondaryAmountDisplay->setText(QString("(€\u20090.00)"));
    layout->addWidget(secondaryAmountDisplay);
    secondaryAmountDisplay->setObjectName("secondaryAmountDisplay");
    secondaryAmountDisplay->setCursor(Qt::PointingHandCursor);
    
    tertiaryAmountDisplay = new ClickableLabel(this);
    tertiaryAmountDisplay->setText(QString("(\uF15A\u20090.00)"));
    layout->addWidget(tertiaryAmountDisplay);
    tertiaryAmountDisplay->setObjectName("tertiaryAmountDisplay");
    tertiaryAmountDisplay->setCursor(Qt::PointingHandCursor);
    
    quadAmountDisplay = new ClickableLabel(this);
    quadAmountDisplay->setText(QString(""));
    layout->addWidget(quadAmountDisplay);
    quadAmountDisplay->setObjectName("quadAmountDisplay");
    quadAmountDisplay->setCursor(Qt::PointingHandCursor);
    quadAmountDisplay->setVisible(false);
       
    forexError = new QLabel(this);
    forexError->setObjectName("forexError");
    forexError->setText("");
    layout->addWidget(forexError);
    
    layout->addStretch(1);
    layout->setContentsMargins(0,0,0,0);

    setLayout(layout);

    setFocusPolicy(Qt::TabFocus);
    setFocusProxy(amount);

    // If one if the widgets changes, the combined content changes as well
    connect(amount, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
    connect(this, SIGNAL(valueChanged()), this, SLOT(update()));
    //connect(unit, SIGNAL(currentIndexChanged(int)), this, SLOT(unitChanged(int)));
    connect(secondaryAmountDisplay, SIGNAL(clicked()), this, SLOT(changeToSecondaryCurrency()));
    connect(amountSeperator, SIGNAL(clicked()), this, SLOT(changeToSecondaryCurrency()));
    connect(tertiaryAmountDisplay, SIGNAL(clicked()), this, SLOT(changeToTertiaryCurrency()));
    connect(quadAmountDisplay, SIGNAL(clicked()), this, SLOT(changeToQuadCurrency()));

    update();
    // Set default based on configuration
    //unitChanged(unit->currentIndex());
}

void BitcoinAmountField::clear()
{
    amount->clear();
    secondaryAmountDisplay->setText(QString("(€\u20090.00)"));
    tertiaryAmountDisplay->setText(QString("(\uF15A\u20090.00)"));
    //unit->setCurrentIndex(0);
}

void BitcoinAmountField::setEnabled(bool fEnabled)
{
    amount->setEnabled(fEnabled);
    unit->setEnabled(fEnabled);
}

bool BitcoinAmountField::validate()
{
    bool valid = false;
    value(&valid);
    setValid(valid);
    return valid;
}

void BitcoinAmountField::setValid(bool valid)
{
    if (valid)
        amount->setStyleSheet("");
    else
        amount->setStyleSheet(STYLE_INVALID);
}

bool BitcoinAmountField::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        // Clear invalid flag on focus
        setValid(true);
    }
    return QWidget::eventFilter(object, event);
}

QWidget *BitcoinAmountField::setupTabChain(QWidget *prev)
{
    QWidget::setTabOrder(prev, amount);
    QWidget::setTabOrder(amount, unit);
    return unit;
}

CAmount BitcoinAmountField::value(bool *valid_out) const
{
    return amount->value(valid_out);
}

CAmount BitcoinAmountField::valueForCurrency(bool *valid_out) const
{
    if (primaryCurrency == displayCurrency)
    {
        return amount->value(valid_out);
    }
    else
    {
        // [...] Gulden <> E... (B...) (R...)
        // [...] Bitcoin <> G... (E...) (R...)
        // [...] Euro <> G... (B...) (R...)
        // [...] ZAR <> G... (E...) (B...)
        switch(displayCurrency)
        {
            case AmountFieldCurrency::CurrencyGulden:
            {
                switch(primaryCurrency)
                {
                    case AmountFieldCurrency::CurrencyEuro:
                        return secondaryAmount;
                    case AmountFieldCurrency::CurrencyBCOIN:
                        return tertiaryAmount;
                    case AmountFieldCurrency::CurrencyLocal:
                        return quadAmount;
                    default:
                        assert(0);
                }
            }
            break;
            case AmountFieldCurrency::CurrencyEuro:
            {
                switch(primaryCurrency)
                {
                    case AmountFieldCurrency::CurrencyGulden:
                        return secondaryAmount;
                    case AmountFieldCurrency::CurrencyBCOIN:
                        return tertiaryAmount;
                    case AmountFieldCurrency::CurrencyLocal:
                        return quadAmount;
                    default:
                        assert(0);
                }
            }
            break;
            case AmountFieldCurrency::CurrencyBCOIN:
            {
                switch(primaryCurrency)
                {
                    case AmountFieldCurrency::CurrencyGulden:
                        return secondaryAmount;
                    case AmountFieldCurrency::CurrencyEuro:
                        return tertiaryAmount;
                    case AmountFieldCurrency::CurrencyLocal:
                        return quadAmount;
                    default:
                        assert(0);
                }
            }
            break;
            case AmountFieldCurrency::CurrencyLocal:
            {
                switch(primaryCurrency)
                {
                    case AmountFieldCurrency::CurrencyGulden:
                        return secondaryAmount;
                    case AmountFieldCurrency::CurrencyEuro:
                        return tertiaryAmount;
                    break;
                    case AmountFieldCurrency::CurrencyBCOIN:
                        return quadAmount;
                    default:
                        assert(0);
                }
            }
            break;
        }
    }
    return amount->value(valid_out);
}

void BitcoinAmountField::setValue(const CAmount& value)
{
    amount->setValue(value);
}

void BitcoinAmountField::setValue(const CAmount& value, int nLimit)
{
    CAmount finalValue;
    BitcoinUnits::parse(BitcoinUnits::BTC, BitcoinUnits::format(BitcoinUnits::BTC, value, false, BitcoinUnits::separatorAlways, nLimit), &finalValue);
    if (finalValue > MAX_MONEY)
    {
        finalValue = MAX_MONEY;
    }
    amount->setValue(finalValue, nLimit);
}

void BitcoinAmountField::setReadOnly(bool fReadOnly)
{
    amount->setReadOnly(fReadOnly);
}

void BitcoinAmountField::unitChanged(int idx)
{
    // Use description tooltip for current unit for the combobox
    /*unit->setToolTip(unit->itemData(idx, Qt::ToolTipRole).toString());

    // Determine new unit ID
    int newUnit = unit->itemData(idx, BitcoinUnits::UnitRole).toInt();

    amount->setDisplayUnit(newUnit);*/
}

void BitcoinAmountField::changeToSecondaryCurrency()
{   
    // [...] Gulden <> E... (B...) (R...)
    // [...] Bitcoin <> G... (E...) (R...)
    // [...] Euro <> G... (B...) (R...)
    // [...] ZAR <> G... (E...) (B...)
    switch(displayCurrency)
    {
        case AmountFieldCurrency::CurrencyGulden:
        {
            displayCurrency = AmountFieldCurrency::CurrencyEuro;
        }
        break;
        case AmountFieldCurrency::CurrencyBCOIN:
        {
            displayCurrency = AmountFieldCurrency::CurrencyGulden;
        }
        break;
        case AmountFieldCurrency::CurrencyEuro:
        {
            displayCurrency = AmountFieldCurrency::CurrencyGulden;
        }
        break;
        case AmountFieldCurrency::CurrencyLocal:
        {
            displayCurrency = AmountFieldCurrency::CurrencyGulden;
        }
        break;
        default:
            assert(0);
    }
    setValue(secondaryAmount, 2);
}

void BitcoinAmountField::changeToTertiaryCurrency()
{
    // [...] Gulden <> E... (B...) (R...)
    // [...] Bitcoin <> G... (E...) (R...)
    // [...] Euro <> G... (B...) (R...)
    // [...] ZAR <> G... (E...) (B...)
    switch(displayCurrency)
    {
        case AmountFieldCurrency::CurrencyGulden:
        {
            displayCurrency = AmountFieldCurrency::CurrencyBCOIN;
        }
        break;
        case AmountFieldCurrency::CurrencyBCOIN:
        {
            displayCurrency = AmountFieldCurrency::CurrencyEuro;
        }
        break;
        case AmountFieldCurrency::CurrencyEuro:
        {
            displayCurrency = AmountFieldCurrency::CurrencyBCOIN;
        }
        break;
        case AmountFieldCurrency::CurrencyLocal:
        {
            displayCurrency = AmountFieldCurrency::CurrencyEuro;
        }
        break;
        default:
            assert(0);
    }
    setValue(tertiaryAmount, 2);
}

void BitcoinAmountField::changeToQuadCurrency()
{
    // [...] Gulden <> E... (B...) (R...)
    // [...] Bitcoin <> G... (E...) (R...)
    // [...] Euro <> G... (B...) (R...)
    // [...] ZAR <> G... (E...) (B...)
    switch(displayCurrency)
    {
        case AmountFieldCurrency::CurrencyGulden:
        {
            displayCurrency = AmountFieldCurrency::CurrencyLocal;
        }
        break;
        case AmountFieldCurrency::CurrencyBCOIN:
        {
            displayCurrency = AmountFieldCurrency::CurrencyLocal;
        }
        break;
        case AmountFieldCurrency::CurrencyEuro:
        {
            displayCurrency = AmountFieldCurrency::CurrencyLocal;
        }
        break;
        case AmountFieldCurrency::CurrencyLocal:
        {
            displayCurrency = AmountFieldCurrency::CurrencyBCOIN;
        }
        break;
        default:
            assert(0);
    }
    setValue(quadAmount, 2);
}

bool BitcoinAmountField::validateEurLimits(CAmount EURAmount)
{
    CAmount currencyMax = optionsModel->getNocksSettings()->getMaximumForCurrency("NLG-EUR");
    CAmount currencyMin = optionsModel->getNocksSettings()->getMinimumForCurrency("NLG-EUR");
    if (EURAmount > currencyMax)
    {
        secondaryAmountDisplay->setText("");
        tertiaryAmountDisplay->setText("");
        quadAmountDisplay->setText("");
        forexError->setText(tr("Maximum allowed %1 payment is %2.").arg("Euro", QString::fromStdString(optionsModel->getNocksSettings()->getMaximumForCurrencyAsString("NLG-EUR"))));
        return false;
    }
    else if(EURAmount < currencyMin)
    {
        secondaryAmountDisplay->setText("");
        tertiaryAmountDisplay->setText("");
        quadAmountDisplay->setText("");
        forexError->setText(tr("Minimum allowed %1 payment is %2.").arg("Euro", QString::fromStdString(optionsModel->getNocksSettings()->getMinimumForCurrencyAsString("NLG-EUR"))));
        return false;
    }
    forexError->setText("");
    return true;
}

bool BitcoinAmountField::validateBTCLimits(CAmount BTCAmount)
{
    CAmount currencyMax = optionsModel->getNocksSettings()->getMaximumForCurrency("NLG-BTC");
    CAmount currencyMin = optionsModel->getNocksSettings()->getMinimumForCurrency("NLG-BTC");
    if (BTCAmount > currencyMax)
    {
        secondaryAmountDisplay->setText("");
        tertiaryAmountDisplay->setText("");
        quadAmountDisplay->setText("");
        forexError->setText(tr("Maximum allowed %1 payment is %2.").arg("Bitcoin", QString::fromStdString(optionsModel->getNocksSettings()->getMaximumForCurrencyAsString("NLG-BTC"))));
        return false;
    }
    else if(BTCAmount < currencyMin)
    {
        secondaryAmountDisplay->setText("");
        tertiaryAmountDisplay->setText("");
        quadAmountDisplay->setText("");
        forexError->setText(tr("Minimum allowed %1 payment is %2.").arg("Bitcoin", QString::fromStdString(optionsModel->getNocksSettings()->getMinimumForCurrencyAsString("NLG-BTC"))));
        return false;
    }
    forexError->setText("");
    return true;
}

void BitcoinAmountField::update()
{
    CAmount amount = value();
    if (!optionsModel)
        return;
    if (!ticker)
        return;
    
    if (nocksRequestBTCtoNLG)
    {
        nocksRequestBTCtoNLG->deleteLater();
        nocksRequestBTCtoNLG = NULL;
    }
    if (nocksRequestEURtoNLG)
    {
        nocksRequestEURtoNLG->deleteLater();
        nocksRequestEURtoNLG = NULL;
    }
    if (nocksRequestNLGtoBTC)
    {
        nocksRequestNLGtoBTC->deleteLater();
        nocksRequestNLGtoBTC = NULL;
    }
    if (nocksRequestNLGtoEUR)
    {
        nocksRequestNLGtoEUR->deleteLater();
        nocksRequestNLGtoEUR = NULL;
    }
    
    if ( optionsModel->guldenSettings->getLocalCurrency().toStdString() == "BTC" ) 
    {
        quadAmountDisplay->setVisible(false);
    }
    else if (optionsModel->guldenSettings->getLocalCurrency().toStdString() == "EUR")
    {
        quadAmountDisplay->setVisible(false);
    }
    else
    {
        quadAmountDisplay->setVisible(true);
    }
    
    
    if (displayCurrency == AmountFieldCurrency::CurrencyGulden)
    {
        // [...] Gulden <> E... (B...) (R...)
        unit->setText("Gulden");
        forexError->setText("");
        
        
        if (amount > 0)
        {
            if (primaryCurrency == AmountFieldCurrency::CurrencyEuro)
            {
                CAmount EURAmount = ticker->convertGuldenToForex(amount, "EUR");
                if (!validateEurLimits(EURAmount))
                    return;
            }
            else if (primaryCurrency == AmountFieldCurrency::CurrencyBCOIN)
            {
                CAmount BTCAmount = ticker->convertGuldenToForex(amount, "BTC");
                if (!validateBTCLimits(BTCAmount))
                    return;
            }
            
            secondaryAmount = ticker->convertGuldenToForex(amount, "EUR");
            nocksRequestNLGtoEUR = new NocksRequest(this, NULL, NocksRequest::RequestType::Quotation, "EUR", "NLG", BitcoinUnits::format(BitcoinUnits::Unit::BTC, amount, false, BitcoinUnits::separatorNever, 2));
            connect(nocksRequestNLGtoEUR, &NocksRequest::requestProcessed, [this]() { nocksRequestProcessed(nocksRequestNLGtoEUR, 2); });
            secondaryAmountDisplay->setText(QString::fromStdString(CurrencySymbolForCurrencyCode("EUR")) + QString("\u2009") + BitcoinUnits::format(BitcoinUnits::Unit::BTC, secondaryAmount, false, BitcoinUnits::separatorAlways, 2));
            tertiaryAmount = ticker->convertGuldenToForex(amount, "BTC");
            nocksRequestNLGtoBTC = new NocksRequest(this, NULL, NocksRequest::RequestType::Quotation, "BTC", "NLG", BitcoinUnits::format(BitcoinUnits::Unit::BTC, amount, false, BitcoinUnits::separatorNever, 2));
            connect(nocksRequestNLGtoBTC, &NocksRequest::requestProcessed, [this]() { nocksRequestProcessed(nocksRequestNLGtoBTC, 3); });
            tertiaryAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode("BTC")) + QString("\u2009") + BitcoinUnits::format(BitcoinUnits::Unit::BTC, tertiaryAmount, false, BitcoinUnits::separatorAlways, 2) + QString(")"));
            quadAmount = ticker->convertGuldenToForex(amount, optionsModel->guldenSettings->getLocalCurrency().toStdString());
            quadAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + BitcoinUnits::format(BitcoinUnits::Unit::BTC, quadAmount, false, BitcoinUnits::separatorAlways, 2) + QString(")"));
        }
        else
        {
            secondaryAmount = CAmount(0);
            tertiaryAmount = CAmount(0);
            quadAmount = CAmount(0);
            secondaryAmountDisplay->setText(QString::fromStdString(CurrencySymbolForCurrencyCode("EUR")) + QString("\u2009") + QString("0.00"));
            tertiaryAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode("BTC")) + QString("\u2009") + QString("0.00)"));
            quadAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + QString("0.00)"));
        }
    }
    else if(displayCurrency == AmountFieldCurrency::CurrencyBCOIN)
    {
        // [...] Bitcoin <> G... (E...) (R...)
        
        unit->setText("Bitcoin");
        forexError->setText("");

        if (amount > 0)
        {
            CAmount guldenAmount = ticker->convertForexToGulden(amount, "BTC");
            if (primaryCurrency == AmountFieldCurrency::CurrencyEuro)
            {
                CAmount EURAmount = ticker->convertGuldenToForex(guldenAmount, "EUR");
                if (!validateEurLimits(EURAmount))
                    return;
            }
            else if (primaryCurrency == AmountFieldCurrency::CurrencyBCOIN)
            {
                if (!validateBTCLimits(amount))
                    return;
            }
            
            secondaryAmount = guldenAmount;
            nocksRequestBTCtoNLG= new NocksRequest(this, NULL, NocksRequest::RequestType::Quotation, "NLG", "BTC", BitcoinUnits::format(BitcoinUnits::Unit::BTC, amount, false, BitcoinUnits::separatorNever, 2));
            connect(nocksRequestBTCtoNLG, &NocksRequest::requestProcessed, [this]() { nocksRequestProcessed(nocksRequestBTCtoNLG, 2); });
            secondaryAmountDisplay->setText(QString::fromStdString(CurrencySymbolForCurrencyCode("NLG")) + BitcoinUnits::format(BitcoinUnits::Unit::BTC, secondaryAmount, false, BitcoinUnits::separatorAlways, 2));
            tertiaryAmount = ticker->convertGuldenToForex(guldenAmount, "EUR");
            tertiaryAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode("EUR")) + QString("\u2009") + BitcoinUnits::format(BitcoinUnits::Unit::BTC, tertiaryAmount, false, BitcoinUnits::separatorAlways, 2) + QString(")"));
            quadAmount = ticker->convertGuldenToForex(guldenAmount, optionsModel->guldenSettings->getLocalCurrency().toStdString());
            quadAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + BitcoinUnits::format(BitcoinUnits::Unit::BTC, quadAmount, false, BitcoinUnits::separatorAlways, 2) + QString(")"));
        }
        else
        {
            secondaryAmount = CAmount(0);
            tertiaryAmount = CAmount(0);
            quadAmount = CAmount(0);
            secondaryAmountDisplay->setText(QString::fromStdString(CurrencySymbolForCurrencyCode("NLG")) + QString("0.00"));
            tertiaryAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode("EUR")) + QString("\u2009") + QString("0.00)"));
            quadAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + QString("0.00)"));
        }
    }
    else if(displayCurrency == AmountFieldCurrency::CurrencyEuro)
    {
        // [...] Euro <> G... (B...) (R...)
        unit->setText("Euro");
        forexError->setText("");
        
        if (amount > 0)
        {
            CAmount guldenAmount = ticker->convertForexToGulden(amount, "EUR");
            if (primaryCurrency == AmountFieldCurrency::CurrencyEuro)
            {            
                if (!validateEurLimits(amount))
                    return;
            }
            else if (primaryCurrency == AmountFieldCurrency::CurrencyBCOIN)
            {
                CAmount BTCAmount = ticker->convertGuldenToForex(guldenAmount, "BTC");
                
                if (!validateBTCLimits(BTCAmount))
                    return;
            }
            
            secondaryAmount = guldenAmount;
            nocksRequestEURtoNLG= new NocksRequest(this, NULL, NocksRequest::RequestType::Quotation, "NLG", "EUR", BitcoinUnits::format(BitcoinUnits::Unit::BTC, amount, false, BitcoinUnits::separatorNever, 2));
            connect(nocksRequestEURtoNLG, &NocksRequest::requestProcessed, [this]() { nocksRequestProcessed(nocksRequestEURtoNLG, 2); });
            secondaryAmountDisplay->setText(QString::fromStdString(CurrencySymbolForCurrencyCode("NLG"))  + BitcoinUnits::format(BitcoinUnits::Unit::BTC, secondaryAmount, false, BitcoinUnits::separatorAlways, 2));
            tertiaryAmount = ticker->convertGuldenToForex(guldenAmount, "BTC");
            tertiaryAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode("BTC")) + QString("\u2009") + BitcoinUnits::format(BitcoinUnits::Unit::BTC, tertiaryAmount, false, BitcoinUnits::separatorAlways, 2) + QString(")"));
            quadAmount = ticker->convertGuldenToForex(guldenAmount, optionsModel->guldenSettings->getLocalCurrency().toStdString());
            quadAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + BitcoinUnits::format(BitcoinUnits::Unit::BTC, quadAmount, false, BitcoinUnits::separatorAlways, 2) + QString(")"));
        }
        else
        {
            secondaryAmount = CAmount(0);
            tertiaryAmount = CAmount(0);
            quadAmount = CAmount(0);
            secondaryAmountDisplay->setText(QString::fromStdString(CurrencySymbolForCurrencyCode("NLG")) + QString("0.00"));
            tertiaryAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode("BTC")) + QString("\u2009") + QString("0.00)"));
            quadAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode(optionsModel->guldenSettings->getLocalCurrency().toStdString())) + QString("\u2009") + QString("0.00)"));
        }
    }
    else if(displayCurrency == AmountFieldCurrency::CurrencyLocal)
    {
        // [...] ZAR <> G... (E...) (B...)
        unit->setText(optionsModel->guldenSettings->getLocalCurrency());
        forexError->setText("");
        
        if (amount > 0)
        {
            CAmount guldenAmount = ticker->convertForexToGulden(amount, optionsModel->guldenSettings->getLocalCurrency().toStdString());
            if (primaryCurrency == AmountFieldCurrency::CurrencyEuro)
            {
                CAmount EURAmount = ticker->convertGuldenToForex(guldenAmount, "EUR");
            
                if (!validateEurLimits(EURAmount))
                    return;
            }
            else if (primaryCurrency == AmountFieldCurrency::CurrencyBCOIN)
            {
                CAmount BTCAmount = ticker->convertGuldenToForex(guldenAmount, "BTC");
                
                if (!validateBTCLimits(BTCAmount))
                    return;
            }
            
            secondaryAmount = guldenAmount;
            secondaryAmountDisplay->setText(QString::fromStdString(CurrencySymbolForCurrencyCode("NLG")) + BitcoinUnits::format(BitcoinUnits::Unit::BTC, secondaryAmount, false, BitcoinUnits::separatorAlways, 2));
            tertiaryAmount = ticker->convertGuldenToForex(guldenAmount, "EUR");
            tertiaryAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode("EUR")) + QString("\u2009") + BitcoinUnits::format(BitcoinUnits::Unit::BTC, tertiaryAmount, false, BitcoinUnits::separatorAlways, 2) + QString(")"));
            quadAmount = ticker->convertGuldenToForex(guldenAmount, "BTC");
            quadAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode("BTC")) + QString("\u2009") + BitcoinUnits::format(BitcoinUnits::Unit::BTC, quadAmount, false, BitcoinUnits::separatorAlways, 2) + QString(")"));
        }
        else
        {
            secondaryAmount = CAmount(0);
            tertiaryAmount = CAmount(0);
            quadAmount = CAmount(0);
            secondaryAmountDisplay->setText(QString::fromStdString(CurrencySymbolForCurrencyCode("NLG")) + QString("0.00"));
            tertiaryAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode("EUR")) + QString("\u2009") + QString("0.00)"));
            quadAmountDisplay->setText(QString("(") + QString::fromStdString(CurrencySymbolForCurrencyCode("BTC")) + QString("\u2009") + QString("0.00)"));
        }
    }    
}

void BitcoinAmountField::nocksRequestProcessed(NocksRequest*& request, int position)
{
    if (position == 2)
    {
        BitcoinUnits::parse(BitcoinUnits::BTC, request->nativeAmount, &secondaryAmount);
        QString oldText = secondaryAmountDisplay->text();
        oldText = oldText.replace(QRegExp("[0-9].*"), BitcoinUnits::format(BitcoinUnits::Unit::BTC, secondaryAmount, false, BitcoinUnits::separatorAlways, 2));
        secondaryAmountDisplay->setText(oldText);
    }
    else if (position == 3)
    {
        BitcoinUnits::parse(BitcoinUnits::BTC, request->nativeAmount, &tertiaryAmount);
        tertiaryAmountDisplay->setText(tertiaryAmountDisplay->text().replace(QRegExp("[0-9].*"), BitcoinUnits::format(BitcoinUnits::Unit::BTC, tertiaryAmount, false, BitcoinUnits::separatorAlways, 2) + QString(")")));
    }
    else if (position == 4)
    {
        BitcoinUnits::parse(BitcoinUnits::BTC, request->nativeAmount, &quadAmount);
        quadAmountDisplay->setText(quadAmountDisplay->text().replace(QRegExp("[0-9].*"), BitcoinUnits::format(BitcoinUnits::Unit::BTC, quadAmount, false, BitcoinUnits::separatorAlways, 2) + QString(")")));
    }
    request->deleteLater();
    request = NULL;
}

void BitcoinAmountField::setDisplayUnit(int unit)
{
    //unit->setValue(newUnit);
}

void BitcoinAmountField::setSingleStep(const CAmount& step)
{
    amount->setSingleStep(step);
}

void BitcoinAmountField::setCurrency(OptionsModel* optionsModel_, CurrencyTicker* ticker_, AmountFieldCurrency currency_)
{
    if (!optionsModel)
    {
        optionsModel = optionsModel_;
        ticker = ticker_;
        
        connect( ticker, SIGNAL( exchangeRatesUpdatedLongPoll() ), this, SLOT( update() ) );
    }
    
    if (displayCurrency != currency_)
    {
        // [...] Gulden <> E... (B...) (R...)
        // [...] Bitcoin <> G... (E...) (R...)
        // [...] Euro <> G... (B...) (R...)
        // [...] ZAR <> G... (E...) (B...)
        switch(displayCurrency)
        {
            case AmountFieldCurrency::CurrencyGulden:
            {
                switch(currency_)
                {
                    case AmountFieldCurrency::CurrencyGulden:
                    {
                    }
                    break;
                    case AmountFieldCurrency::CurrencyEuro:
                    {
                        changeToSecondaryCurrency();
                    }
                    break;
                    case AmountFieldCurrency::CurrencyBCOIN:
                    {
                        changeToTertiaryCurrency();
                    }
                    break;
                    case AmountFieldCurrency::CurrencyLocal:
                    {
                        changeToQuadCurrency();
                    }
                    break;
                }
            }
            break;
            case AmountFieldCurrency::CurrencyEuro:
            {
                switch(currency_)
                {
                    case AmountFieldCurrency::CurrencyGulden:
                    {
                        changeToSecondaryCurrency();
                    }
                    break;
                    case AmountFieldCurrency::CurrencyEuro:
                    {
                    }
                    break;
                    case AmountFieldCurrency::CurrencyBCOIN:
                    {
                        changeToTertiaryCurrency();
                    }
                    break;
                    case AmountFieldCurrency::CurrencyLocal:
                    {
                        changeToQuadCurrency();
                    }
                    break;
                }
            }
            break;
            case AmountFieldCurrency::CurrencyBCOIN:
            {
                switch(currency_)
                {
                    case AmountFieldCurrency::CurrencyGulden:
                    {
                        changeToSecondaryCurrency();
                    }
                    break;
                    case AmountFieldCurrency::CurrencyEuro:
                    {
                        changeToTertiaryCurrency();
                    }
                    break;
                    case AmountFieldCurrency::CurrencyBCOIN:
                    {
                    }
                    break;
                    case AmountFieldCurrency::CurrencyLocal:
                    {
                        changeToQuadCurrency();
                    }
                    break;
                }
            }
            break;
            case AmountFieldCurrency::CurrencyLocal:
            {
                switch(currency_)
                {
                    case AmountFieldCurrency::CurrencyGulden:
                    {
                        changeToSecondaryCurrency();
                    }
                    break;
                    case AmountFieldCurrency::CurrencyEuro:
                    {
                        changeToTertiaryCurrency();
                    }
                    break;
                    case AmountFieldCurrency::CurrencyBCOIN:
                    {
                        changeToQuadCurrency();
                    }
                    break;
                    case AmountFieldCurrency::CurrencyLocal:
                    {
                    }
                    break;
                }
            }
            break;
        }
        displayCurrency = currency_;
    }
    if (primaryCurrency != currency_)
    {
        primaryCurrency = currency_;
        update();
    }    
}
