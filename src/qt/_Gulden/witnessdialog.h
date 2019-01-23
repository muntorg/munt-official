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
#include <qwt_plot_picker.h>
#include "optionsmodel.h"


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

class PlotMouseTracker: public QwtPlotPicker
{
Q_OBJECT
public:
    PlotMouseTracker( QWidget* );
    virtual ~PlotMouseTracker() {}

protected:
    virtual QwtText trackerText( const QPoint & ) const;
    virtual QRect trackerRect( const QFont & ) const;

private:
    QString curveInfoAt(QString legendColor,  QString sHeading, const QwtPlotCurve*, const QPoint & ) const;
};

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

struct WitnessInfoForAccount
{
    uint64_t nOurWeight = 0;
    uint64_t nTotalNetworkWeightTip = 0;
    uint64_t nWitnessLength = 0;
    uint64_t nExpectedWitnessBlockPeriod = 0;
    uint64_t nEstimatedWitnessBlockPeriod = 0;
    uint64_t nLockBlocksRemaining = 0;
    int64_t nOriginNetworkWeight = 0;
    uint64_t nOriginBlock = 0;
    uint64_t nOriginWeight = 0;
    uint64_t nOriginLength = 0;
    uint64_t nEarningsToDate = 0;
    CAmount nTotal1 = 0;
    CAmount nTotal2 = 0;
    int nXForecast = 0;
    GraphScale scale = GraphScale::Blocks;
    QPolygonF forecastedPoints;
    QPolygonF generatedPoints;
    QPolygonF generatedPointsForecast;
    QDateTime originDate;
    QDateTime lastEarningsDate;
};

class WitnessDialog : public QFrame
{
    Q_OBJECT

public:
    explicit WitnessDialog(const QStyle *platformStyle, QWidget *parent = 0);
    virtual ~WitnessDialog();

    void setClientModel(ClientModel *clientModel);
    void setModel(WalletModel *model);

    void GetWitnessInfoForAccount(CAccount* forAccount, WitnessInfoForAccount& infoForAccount);
    void plotGraphForAccount(CAccount* account, uint64_t nOurWeight, uint64_t nTotalNetworkWeightTip);

    void updateAccountIndicators();

Q_SIGNALS:
    void requestEmptyWitness();
    void requestFundWitness(CAccount* funderAccount);
    void requestRenewWitness(CAccount* funderAccount);

public Q_SLOTS:
    void updateUnit(int nNewUnit_);
    void update();
    void numBlocksChanged(int,QDateTime,double);
    void compoundEarningsCheckboxClicked();
    void unitButtonClicked();
    void viewWitnessInfoClicked();
    void emptyWitnessClicked();
    void fundWitnessClicked();
    void renewWitnessClicked();
protected:

private:
    void clearLabels();
    void doUpdate(bool forceUpdate=false);
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
