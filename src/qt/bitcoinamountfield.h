// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BITCOINAMOUNTFIELD_H
#define BITCOIN_QT_BITCOINAMOUNTFIELD_H

#include "amount.h"

#include <QWidget>

class AmountSpinBox;
class OptionsModel;
class CurrencyTicker;
class ClickableLabel;
class NocksRequest;

QT_BEGIN_NAMESPACE
class QValueComboBox;
class QLabel;
QT_END_NAMESPACE

/** Widget for entering bitcoin amounts.
  */
class BitcoinAmountField: public QWidget
{
    Q_OBJECT

    // ugly hack: for some unknown reason CAmount (instead of qint64) does not work here as expected
    // discussion: https://github.com/bitcoin/bitcoin/pull/5117
    Q_PROPERTY(qint64 value READ value WRITE setValue NOTIFY valueChanged USER true)

public:
    explicit BitcoinAmountField(QWidget *parent = 0);

    CAmount value(bool *value=0) const;
    CAmount valueForCurrency(bool *value=0) const;
    void setValue(const CAmount& value);
    void setValue(const CAmount& value, int nLimit);

    /** Set single step in satoshis **/
    void setSingleStep(const CAmount& step);

    /** Make read-only **/
    void setReadOnly(bool fReadOnly);

    /** Mark current value as invalid in UI. */
    void setValid(bool valid);
    /** Perform input validation, mark field as invalid if entered value is not valid. */
    bool validate();

    /** Change unit used to display amount. */
    void setDisplayUnit(int unit);

    /** Make field empty and ready for new input. */
    void clear();

    /** Enable/Disable. */
    void setEnabled(bool fEnabled);

    /** Qt messes up the tab chain by default in some cases (issue https://bugreports.qt-project.org/browse/QTBUG-10907),
        in these cases we have to set it up manually.
    */
    QWidget *setupTabChain(QWidget *prev);
    
    enum AmountFieldCurrency
    {
        CurrencyGulden,
        CurrencyBCOIN,
        CurrencyEuro,
        CurrencyLocal
    };
    void setCurrency(OptionsModel* optionsModel_, CurrencyTicker* ticker, AmountFieldCurrency currency_);
    
    void nocksRequestProcessed(NocksRequest*& request, int position);
    
Q_SIGNALS:
    void valueChanged();

protected:
    /** Intercept focus-in event and ',' key presses */
    bool eventFilter(QObject *object, QEvent *event);

private:
    AmountSpinBox *amount;
    QLabel* unit;
    ClickableLabel* secondaryAmountDisplay;
    ClickableLabel* tertiaryAmountDisplay;
    ClickableLabel* quadAmountDisplay;
    ClickableLabel* amountSeperator;
    AmountFieldCurrency primaryCurrency;
    AmountFieldCurrency displayCurrency;
    QLabel* forexError;
    
    OptionsModel* optionsModel;
    CurrencyTicker* ticker;
    
    CAmount secondaryAmount;
    CAmount tertiaryAmount;
    CAmount quadAmount;
    
    NocksRequest* nocksRequestBTCtoNLG;
    NocksRequest* nocksRequestEURtoNLG;
    NocksRequest* nocksRequestNLGtoBTC;
    NocksRequest* nocksRequestNLGtoEUR;

    bool validateEurLimits(CAmount EURAmount);
    bool validateBTCLimits(CAmount EURAmount);
private Q_SLOTS:
    void unitChanged(int idx);
    void update();
    void changeToSecondaryCurrency();
    void changeToTertiaryCurrency();
    void changeToQuadCurrency();
};

#endif // BITCOIN_QT_BITCOINAMOUNTFIELD_H
