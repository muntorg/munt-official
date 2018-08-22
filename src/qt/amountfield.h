// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_GULDENAMOUNTFIELD_H
#define GULDEN_QT_GULDENAMOUNTFIELD_H

#include "amount.h"

#include <QWidget>

class AmountSpinBox;
class OptionsModel;
class CurrencyTicker;
class ClickableLabel;

QT_BEGIN_NAMESPACE
class QValueComboBox;
class QLabel;
QT_END_NAMESPACE

/** Widget for entering currency amounts.
  */
class GuldenAmountField: public QWidget
{
    Q_OBJECT

public:
    explicit GuldenAmountField(QWidget *parent = 0);
    virtual ~GuldenAmountField();

    enum class Currency
    {
        None,
        Gulden, // Gulden
        Euro,   // Quote currency, Euro
        Local   // Local currency configured in options
    };

    CAmount amount(const Currency currency = Currency::Gulden) const;

    //! Set amount in Gulden.
    void setAmount(const CAmount& value);

    /** Set single step in satoshis **/
    void setSingleStep(const CAmount& step);

    /** Make read-only **/
    void setReadOnly(bool fReadOnly);

    /** Mark current value as invalid in UI. */
    void setValid(bool valid);

    /** Make field empty and ready for new input. */
    void clear();

    /** Enable/Disable. */
    void setEnabled(bool fEnabled);

    //! Set currency used for main display and editing.
    void setPrimaryDisplayCurrency(const Currency currency);

    /** Set OptionsModel for ticker information and configured local currency.
        Without OptionsModel auxilary currencies will not be displayed.
    */
    void setOptionsModel(OptionsModel* optionsModel_);

    /** Qt messes up the tab chain by default in some cases (issue https://bugreports.qt-project.org/browse/QTBUG-10907),
        in these cases we have to set it up manually.
    */
    QWidget *setupTabChain(QWidget *prev);


Q_SIGNALS:
    void amountChanged();
    void maxButtonClicked();

protected:
    /** Intercept focus-in event and ',' key presses */
    bool eventFilter(QObject *object, QEvent *event);

private:
    // Principal internal state/data members. These are updated by external events such as user
    // entering data and exchange rate updates. Amounts in all currencies are kept so that display
    // values can be swapped without need of repeated conversion which would lead to the values
    // changing due to loss of precision.
    //    The primaryCurrency determines the primary input field and display order of the
    //    auxilary currencies as follows:
    //        Gulden : [Gulden] <-> Euro (Local)
    //        Euro   : [Euro]   <-> Gulden (Local)
    //        Local  : [Local]  <-> Gulden (Euro)
    //    If the local currency is EUR only the first auxilary currency is displayed.
    Currency primaryCurrency; // which currency is used for the input field
    CAmount amountGulden;
    CAmount amountEuro;
    CAmount amountLocal;

    // UI widgets
    AmountSpinBox* primaryAmountDisplay;
    QLabel* primaryAmountName;
    ClickableLabel* firstAuxAmountDisplay;
    ClickableLabel* amountSeperator;
    ClickableLabel* secondAuxAmountDisplay;

    // external sources
    OptionsModel* optionsModel;
    CurrencyTicker* ticker;

    // helpers for syncing data with UI
    void updatePrimaryFromData();
    void updateDataFromPrimary();
    void updateAuxilaryFromData();

    // util and formatting helpers
    Currency firstAuxCurrency() const;
    Currency secondAuxCurrency() const;
    std::string CurrencyCode(const Currency currency) const;
    QString FormatAuxAmount(const Currency currency) const;

private Q_SLOTS:
    void changeToFirstAuxCurrency();
    void changeToSecondAuxCurrency();
    void exchangeRateUpdate();
    void localCurrencyChanged(QString);
    void tickerOrOptionsDestroyed(QObject*);
    void primaryValueChanged();
};

#endif // GULDEN_QT_GULDENAMOUNTFIELD_H
