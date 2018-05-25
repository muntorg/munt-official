// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_SENDCOINSENTRY_H
#define GULDEN_QT_SENDCOINSENTRY_H

#include "walletmodel.h"

#include <QStackedWidget>

class WalletModel;
class QStyle;
class QSortFilterProxyModel;

namespace Ui {
    class GuldenSendCoinsEntry;
}

/**
 * A single entry in the dialog for sending transactions.
 */
class GuldenSendCoinsEntry : public QFrame
{
    Q_OBJECT

public:
    explicit GuldenSendCoinsEntry(const QStyle *platformStyle, QWidget *parent = 0);
    ~GuldenSendCoinsEntry();

    void setModel(WalletModel *model);
    bool validate();
    SendCoinsRecipient::PaymentType getPaymentType(const QString& fromAddress);
    SendCoinsRecipient getValue(bool showWarningDialogs=true);

    /** Return whether the entry is still empty and unedited */
    bool isClear();

    void update();

    void setValue(const SendCoinsRecipient &value);
    void setAmount(const CAmount amount);
    void setAddress(const QString &address);

    /** Set up the tab chain manually, as Qt messes up the tab chain by default in some cases
     *  (issue https://bugreports.qt-project.org/browse/QTBUG-10907).
     */
    QWidget *setupTabChain(QWidget *prev);

    void setFocus();

    bool ShouldShowEditButton() const;
    bool ShouldShowClearButton() const;
    bool ShouldShowDeleteButton() const;;

    void deleteAddressBookEntry();
    void editAddressBookEntry();

    void gotoWitnessTab(CAccount* targetAccount);

public Q_SLOTS:
    void clear();
    void addressChanged();
    void tabChanged();
    void addressBookSelectionChanged();
    void myAccountsSelectionChanged();

Q_SIGNALS:
    void removeEntry(GuldenSendCoinsEntry *entry);
    void subtractFeeFromAmountChanged();
    void sendTabChanged();
    void valueChanged();

private Q_SLOTS:
    void deleteClicked();
    void on_payTo_textChanged(const QString &address);
    void on_addressBookButton_clicked();
    void on_pasteButton_clicked();
    void updateDisplayUnit();
    void searchChangedAddressBook(const QString& searchString);
    void searchChangedMyAccounts(const QString& searchString);
    void witnessSliderValueChanged(int newValue);
    void payAmountChanged();

private:
    CAccount* targetWitnessAccount;

    bool isPoW2WitnessCreation();
    SendCoinsRecipient recipient;
    Ui::GuldenSendCoinsEntry *ui;
    WalletModel *model;
    const QStyle *platformStyle;

    QSortFilterProxyModel* proxyModelRecipients;
    QSortFilterProxyModel* proxyModelAddresses;

    bool updateLabel(const QString &address);
};

#endif // GULDEN_QT_SENDCOINSENTRY_H
