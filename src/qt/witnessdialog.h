// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_WITNESSDIALOG_H
#define GULDEN_QT_WITNESSDIALOG_H

#include "guiutil.h"
#include <transactionfilterproxy.h>

#include <thread>
#include <condition_variable>
#include <QFrame>
#include <QDateTime>
#include <qwt_plot_picker.h>
#include "optionsmodel.h"
#include "wallet/witness_operations.h"


class QMenu;
class QStyle;
class QwtPlotCurve;
class ClientModel;
class OptionsModel;
class WalletModel;
class CAccount;
enum class WitnessStatus;

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

struct WitnessInfoForAccount
{
    CWitnessAccountStatus accountStatus;

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
    GraphScale scale = GraphScale::Blocks;
    std::map<double, CAmount> pointMapForecast;
    std::map<double, CAmount> pointMapGenerated;
    QDateTime originDate;
    QDateTime lastEarningsDate;
};

Q_DECLARE_METATYPE(WitnessInfoForAccount)

class WitnessDialog : public QFrame
{
    Q_OBJECT

public:
    explicit WitnessDialog(const QStyle *platformStyle, QWidget *parent = 0);
    virtual ~WitnessDialog();

    void setClientModel(ClientModel *clientModel);
    void setModel(WalletModel *model);

    WitnessInfoForAccount GetWitnessInfoForAccount(CAccount* forAccount, const CWitnessAccountStatus& accountStatus) const;

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
    void extendClicked();
    void optimizeClicked();
    void activeAccountChanged(CAccount* account);

protected:

private:
    void clearLabels();
    bool doUpdate(bool forceUpdate = false, WitnessStatus* pWitnessStatus = nullptr);
    Ui::WitnessDialog *ui;
    const QStyle *platformStyle;
    ClientModel *clientModel;
    WalletModel *model;
    CAccount* prevActiveAccount = nullptr;
    bool lastCheckWasDuringInitialBlockDownload = false;

    int userWidgetIndex = -1;
    int prevWitnessedTipHeight = 0;

    QwtPlotCurve* expectedEarningsCurve = nullptr;
    QwtPlotCurve* currentEarningsCurveShadow = nullptr;
    QwtPlotCurve* currentEarningsCurveForecastShadow = nullptr;
    QwtPlotCurve* currentEarningsCurve = nullptr;
    QwtPlotCurve* currentEarningsCurveForecast = nullptr;

    QMenu* unitSelectionMenu;

    std::unique_ptr<TransactionFilterProxy> filter;

    void updateStatisticsThreadLoop();
    void requestStatisticsUpdate(const CWitnessAccountStatus& accountStatus);
    std::thread statsUpdateThread;
    std::mutex statsUpdateMutex;
    std::condition_variable statsUpdateCondition;
    bool statsUpdateShouldUpdate;
    bool statsUpdateShouldStop;
    CWitnessAccountStatus statsUpdateAccountStatus;

    /** Show dialog widget, hiding current.
     * Ownership of the dialog is transferred.
    */
    void pushDialog(QWidget* dialog);

    /** Pop all dialog widgets pushed if any, showing the root WitnessDialog.
     * All dialog widgets popped will be deletedLater!
    */
    void clearDialogStack();

private Q_SLOTS:
    /** Pop dialog stack up to and including dialog.
     * If dialog is not on the stack nothing is popped.
     * All dialog widgets popped will be deletedLater!
    */
    void popDialog(QWidget* dialog);
    void displayUpdatedStatistics(const WitnessInfoForAccount& infoForAccount);

Q_SIGNALS:
    // Sent when a message should be reported to the user.
    void message(const QString &title, const QString &message, unsigned int style);
};

#endif // GULDEN_QT_WITNESSDIALOG_H
