// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_WITNESSDIALOG_H
#define GULDEN_QT_WITNESSDIALOG_H

#include "guiutil.h"
#include <transactionfilterproxy.h>


#include <QFrame>
#include <QDateTime>

class QMenu;
class QStyle;
class QwtPlotCurve;
class ClientModel;
class OptionsModel;
class WalletModel;
class CAccount;

namespace Ui {
    class WitnessDialog;
}


class WitnessSortFilterProxyModel : public QSortFilterProxyModel
{
Q_OBJECT
public:
    explicit WitnessSortFilterProxyModel(QObject *parent = 0);
    virtual ~WitnessSortFilterProxyModel();
    void setAmount(uint64_t nAmount_) {nAmount = nAmount_;}
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const;
private:
    uint64_t nAmount;
};

class WitnessDialog : public QFrame
{
    Q_OBJECT

public:
    explicit WitnessDialog(const QStyle *platformStyle, QWidget *parent = 0);
    ~WitnessDialog();

    void setClientModel(ClientModel *clientModel);
    void setModel(WalletModel *model);

Q_SIGNALS:
    void requestEmptyWitness();
    void requestFundWitness(CAccount* funderAccount);
    void requestRenewWitness(CAccount* funderAccount);

public Q_SLOTS:
    void updateUnit(int nNewUnit_);
    void plotGraphForAccount(CAccount* account, uint64_t nTotalNetworkWeightTip);
    void update();
    void numBlocksChanged(int,QDateTime,double,bool);
    void unitButtonClicked();
    void viewWitnessInfoClicked();
    void emptyWitnessClicked();
    void fundWitnessClicked();
    void renewWitnessClicked();
protected:

private:
    Ui::WitnessDialog *ui;
    const QStyle *platformStyle;
    ClientModel *clientModel;
    WalletModel *model;

    QwtPlotCurve* expectedEarningsCurve = nullptr;
    QwtPlotCurve* currentEarningsCurveShadow = nullptr;
    QwtPlotCurve* currentEarningsCurveForecastShadow = nullptr;
    QwtPlotCurve* currentEarningsCurve = nullptr;
    QwtPlotCurve* currentEarningsCurveForecast = nullptr;

    QMenu* unitSelectionMenu;

    std::unique_ptr<TransactionFilterProxy> filter;

private Q_SLOTS:

Q_SIGNALS:
    // Sent when a message should be reported to the user.
    void message(const QString &title, const QString &message, unsigned int style);
};

#endif // GULDEN_QT_WITNESSDIALOG_H
