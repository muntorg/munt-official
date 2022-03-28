// Copyright (c) 2016-2020 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge(willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "witnessdialog.h"
#include <qt/forms/ui_witnessdialog.h>
#include <unity/appmanager.h>
#include "GuldenGUI.h"
#include <wallet/wallet.h>
#include "ui_interface.h"
#include <wallet/mnemonic.h>
#include <vector>
#include "random.h"
#include "walletmodel.h"
#include "clientmodel.h"
#include "wallet/wallet.h"
#include "wallet/witness_operations.h"
#include "gui.h"
#include "amount.h"

#include <qwt_plot.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_legenditem.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_zoomer.h>
#include <qwt_plot_magnifier.h>
#include <qwt_plot_panner.h>
#include <qwt_curve_fitter.h>
#include <qwt_symbol.h>
#include <qwt_legend.h>
#include <qwt_scale_widget.h>
#include <qwt_picker_machine.h>

#include "transactiontablemodel.h"
#include "transactionrecord.h"
#include "validation/validation.h"
#include "validation/witnessvalidation.h"

#include "witnessutil.h"
#include "GuldenGUI.h"
#include "fundwitnessdialog.h"
#include "optimizewitnessdialog.h"
#include "accounttablemodel.h"
#include "consensus/validation.h"

#include <QMenu>

#include "alert.h"

PlotMouseTracker::PlotMouseTracker( QWidget* canvas )
: QwtPlotPicker( canvas )
{
    setTrackerMode( QwtPlotPicker::ActiveOnly );
    setRubberBand( VLineRubberBand );
    setStateMachine( new QwtPickerTrackerMachine() );
    setRubberBandPen( QPen( ACCENT_COLOR_1 ) );
}

QRect PlotMouseTracker::trackerRect( const QFont &font ) const
{
    QRect r = QwtPlotPicker::trackerRect( font );

    r.moveTop(10);
    r.moveLeft(10);

    return r;
}

QwtText PlotMouseTracker::trackerText( const QPoint &pos ) const
{
    QwtText trackerText;
    trackerText.setRenderFlags (Qt::AlignLeft);

    const QwtPlotItemList curves = plot()->itemList( QwtPlotItem::Rtti_PlotCurve );

    // Figure out whether we are closer to the "earnings to date" curve or the "future earnings forecast" curve.
    double distanceA, distanceB;
    static_cast<const QwtPlotCurve*>(curves[0])->closestPoint(pos, &distanceA);
    static_cast<const QwtPlotCurve*>(curves[2])->closestPoint(pos, &distanceB);

    // Populate popup text based on closest points on the two curves we are interested in.
    QString info;
    info += curveInfoAt(TEXT_COLOR_1, tr("Initial expected earnings:"), static_cast<const QwtPlotCurve*>(curves[2]), pos );
    info += "<br>";
    if (distanceA < distanceB)
    {
        info += curveInfoAt(ACCENT_COLOR_1, tr("Earnings to date:"), static_cast<const QwtPlotCurve*>(curves[0]), pos );
    }
    else
    {
        info += curveInfoAt(ACCENT_COLOR_1, tr("Future earnings forecast:"), static_cast<const QwtPlotCurve*>(curves[4]), pos );
    }

    trackerText.setText( info );
    return trackerText;
}

QString PlotMouseTracker::curveInfoAt(QString legendColour, QString sHeading, const QwtPlotCurve* curve, const QPoint &pos ) const
{
    const int y = curve->sample(curve->closestPoint(pos)).y();
    return QString( "<font color=\"%1\">%2 </font><font color=\"%3\">%4 %5 Gulden</font>" ).arg(legendColour).arg(GUIUtil::fontAwesomeRegular("\uf201")).arg( TEXT_COLOR_1 ).arg(sHeading).arg( y );
}

enum WitnessDialogStates {EMPTY, STATISTICS, EXPIRED, PENDING, FINAL, STATE_ERROR};

WitnessDialog::WitnessDialog(const QStyle* _platformStyle, QWidget* parent)
: QFrame( parent )
, ui( new Ui::WitnessDialog )
, platformStyle( _platformStyle )
, clientModel(NULL)
, model( NULL )
{
    ui->setupUi(this);

    // Set correct cursor for all clickable UI elements.
    ui->unitButton->setCursor(Qt::PointingHandCursor);
    ui->viewWitnessGraphButton->setCursor(Qt::PointingHandCursor);
    ui->renewWitnessButton->setCursor(Qt::PointingHandCursor);
    ui->emptyWitnessButton->setCursor(Qt::PointingHandCursor);
    ui->emptyWitnessButton2->setCursor(Qt::PointingHandCursor);
    ui->withdrawEarningsButton->setCursor(Qt::PointingHandCursor);
    ui->withdrawEarningsButton2->setCursor(Qt::PointingHandCursor);
    ui->compoundEarningsCheckBox->setCursor(Qt::PointingHandCursor);

    // Force qwt graph back to normal cursor instead of cross hair.
    ui->witnessEarningsPlot->canvas()->setCursor(Qt::ArrowCursor);

    // Clear all labels by default
    ui->labelWeightValue->setText(tr("n/a"));
    ui->labelNetworkWeightValue->setText(tr("n/a"));
    ui->labelLockedFromValue->setText(tr("n/a"));
    ui->labelLockedUntilValue->setText(tr("n/a"));
    ui->labelLastEarningsDateValue->setText(tr("n/a"));
    ui->labelWitnessEarningsValue->setText(tr("n/a"));
    ui->labelLockDurationValue->setText(tr("n/a"));
    ui->labelExpectedEarningsDurationValue->setText(tr("n/a"));
    ui->labelEstimatedEarningsDurationValue->setText(tr("n/a"));
    ui->labelLockTimeRemainingValue->setText(tr("n/a"));
    ui->labelLockTimeElapsedValue->setText(tr("n/a"));

    // White background for plot
    ui->witnessEarningsPlot->setStyleSheet("QwtPlotCanvas { background: white; } * { font-size: 10px; font-family: \"Lato\", \"HelveticaNeue-Light\", \"Helvetica Neue Light\", \"Helvetica Neue\", Helvetica, Arial, \"Lucida Grande\",  \"guldensign\", \"'guldensign'\", \"FontAwesome\", \"'FontAwesome'\", sans-serif;}");

    // Only left and top axes are visible for graph
    ui->witnessEarningsPlot->enableAxis(QwtPlot::Axis::yRight, false);
    ui->witnessEarningsPlot->enableAxis(QwtPlot::Axis::xTop, false);
    ui->witnessEarningsPlot->enableAxis(QwtPlot::Axis::yLeft, true);
    ui->witnessEarningsPlot->enableAxis(QwtPlot::Axis::xBottom, true);

    //Turn margins off - let the axes touch one another.
    ui->witnessEarningsPlot->plotLayout()->setAlignCanvasToScales( true );
    for ( int axis = 0; axis < QwtPlot::axisCnt; axis++ )
    {
        ui->witnessEarningsPlot->axisWidget( axis )->setMargin( 0 );
        ui->witnessEarningsPlot->axisScaleDraw( axis )->enableComponent( QwtAbstractScaleDraw::Backbone, false );
    }

    // Setup curve shadow colour/properties for earnings to date.
    {
        currentEarningsCurveShadow = new QwtPlotCurve();
        currentEarningsCurveShadow->setPen( QPen(Qt::NoPen) );

        currentEarningsCurveShadow->setBrush( QBrush(QColor("#c9e3ff")) );
        currentEarningsCurveShadow->setBaseline(0);

        QwtSplineCurveFitter* fitter = new QwtSplineCurveFitter();
        fitter->setFitMode(QwtSplineCurveFitter::ParametricSpline);
        currentEarningsCurveShadow->setCurveFitter(fitter);
        currentEarningsCurveShadow->setRenderHint( QwtPlotItem::RenderAntialiased, true );

        currentEarningsCurveShadow->attach( ui->witnessEarningsPlot );
    }

    // Setup curve colour/properties for earnings to date.
    {
        currentEarningsCurve = new QwtPlotCurve();
        currentEarningsCurve->setTitle( tr("Earnings to date") );
        currentEarningsCurve->setPen( QColor(ACCENT_COLOR_1), 1 );

        currentEarningsCurve->setBrush( QBrush(Qt::NoBrush) );
        currentEarningsCurve->setZ(currentEarningsCurveShadow->z() + 2);

        QwtSplineCurveFitter* fitter = new QwtSplineCurveFitter();
        fitter->setFitMode(QwtSplineCurveFitter::ParametricSpline);
        currentEarningsCurve->setCurveFitter(fitter);
        currentEarningsCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );

        currentEarningsCurve->attach( ui->witnessEarningsPlot );
    }

    // Setup curve shadow colour/properties for future earnings (projected on top of earnings to date)
    {
        currentEarningsCurveForecastShadow = new QwtPlotCurve();
        currentEarningsCurveForecastShadow->setPen( QPen(Qt::NoPen) );

        currentEarningsCurveForecastShadow->setBrush( QBrush(QColor("#c9e3ff")) );
        currentEarningsCurveForecastShadow->setBaseline(0);
        currentEarningsCurveForecastShadow->setZ(currentEarningsCurveShadow->z());

        QwtSplineCurveFitter* fitter = new QwtSplineCurveFitter();
        fitter->setFitMode(QwtSplineCurveFitter::ParametricSpline);
        currentEarningsCurveForecastShadow->setCurveFitter(fitter);
        currentEarningsCurveForecastShadow->setRenderHint( QwtPlotItem::RenderAntialiased, true );

        currentEarningsCurveForecastShadow->attach( ui->witnessEarningsPlot );
    }

    // Setup curve colour/properties for future earnings (projected on top of earnings to date)
    {
        currentEarningsCurveForecast = new QwtPlotCurve();
        currentEarningsCurveForecast->setTitle( tr("Projected earnings") );
        currentEarningsCurveForecast->setPen( QColor(ACCENT_COLOR_1), 1, Qt::DashLine );

        currentEarningsCurveForecast->setBrush( QBrush(Qt::NoBrush) );
        currentEarningsCurveForecast->setZ(currentEarningsCurveShadow->z() + 2);

        QwtSplineCurveFitter* fitter = new QwtSplineCurveFitter();
        fitter->setFitMode(QwtSplineCurveFitter::ParametricSpline);
        currentEarningsCurveForecast->setCurveFitter(fitter);
        currentEarningsCurveForecast->setRenderHint( QwtPlotItem::RenderAntialiased, true );

        currentEarningsCurveForecast->attach( ui->witnessEarningsPlot );
    }

    // Setup curve colour/properties for predicted earnings (based on network weight when account first created)
    {
        expectedEarningsCurve = new QwtPlotCurve();
        expectedEarningsCurve->setTitle( tr("Initial projected earnings") );
        expectedEarningsCurve->setPen( QColor(TEXT_COLOR_1), 1 );

        expectedEarningsCurve->setBrush( QBrush(QColor("#ebebeb")) );
        expectedEarningsCurve->setBaseline(0);
        expectedEarningsCurve->setZ(currentEarningsCurveShadow->z() + 1);

        QwtSplineCurveFitter* fitter = new QwtSplineCurveFitter();
        fitter->setFitMode(QwtSplineCurveFitter::ParametricSpline);
        //fitter->setSplineSize(10);
        expectedEarningsCurve->setCurveFitter(fitter);
        expectedEarningsCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );

        expectedEarningsCurve->attach( ui->witnessEarningsPlot );
    }

    //fixme: (FUT) Possible leak, or does canvas free it?
    new PlotMouseTracker( ui->witnessEarningsPlot->canvas() );

    QAction* unitBlocksAction = new QAction(tr("&Blocks"), this);
    unitBlocksAction->setObjectName("action_unit_blocks");
    QAction* unitDaysAction = new QAction(tr("&Days"), this);
    unitDaysAction->setObjectName("action_days");
    QAction* unitWeeksAction = new QAction(tr("&Weeks"), this);
    unitWeeksAction->setObjectName("action_weeks");
    QAction* unitMonthsAction = new QAction(tr("&Months"), this);
    unitMonthsAction->setObjectName("action_months");

    // Build context menu for unit selection button
    unitSelectionMenu = new QMenu(this);
    unitSelectionMenu->addAction(unitBlocksAction);
    unitSelectionMenu->addAction(unitDaysAction);
    unitSelectionMenu->addAction(unitWeeksAction);
    unitSelectionMenu->addAction(unitMonthsAction);

    // Hide all buttons by default
    ui->emptyWitnessButton->setVisible(false);
    ui->emptyWitnessButton2->setVisible(false);
    ui->withdrawEarningsButton->setVisible(false);
    ui->withdrawEarningsButton2->setVisible(false);
    ui->renewWitnessButton->setVisible(false);
    ui->unitButton->setVisible(false);
    ui->viewWitnessGraphButton->setVisible(false);
    ui->extendButton->setVisible(false);

    connect(ui->unitButton, SIGNAL(clicked()), this, SLOT(unitButtonClicked()));
    connect(ui->viewWitnessGraphButton, SIGNAL(clicked()), this, SLOT(viewWitnessInfoClicked()));
    connect(ui->emptyWitnessButton, SIGNAL(clicked()), this, SLOT(emptyWitnessClicked()));
    connect(ui->emptyWitnessButton2, SIGNAL(clicked()), this, SLOT(emptyWitnessClicked()));
    connect(ui->withdrawEarningsButton, SIGNAL(clicked()), this, SLOT(emptyWitnessClicked()));
    connect(ui->withdrawEarningsButton2, SIGNAL(clicked()), this, SLOT(emptyWitnessClicked()));
    connect(ui->renewWitnessButton, SIGNAL(clicked()), this, SLOT(renewWitnessClicked()));
    connect(ui->compoundEarningsCheckBox, SIGNAL(clicked()), this, SLOT(compoundEarningsCheckboxClicked()));
    connect(ui->extendButton, SIGNAL(clicked()), this, SLOT(extendClicked()));
    connect(ui->optimizeButton, SIGNAL(clicked()), this, SLOT(optimizeClicked()));
    connect(unitBlocksAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Blocks); } );
    connect(unitDaysAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Days); } );
    connect(unitWeeksAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Weeks); } );
    connect(unitMonthsAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Months); } );

    statsUpdateShouldUpdate = false;
    statsUpdateShouldStop = false;
    statsUpdateThread = std::thread(&WitnessDialog::updateStatisticsThreadLoop, this);
}

void WitnessDialog::clearLabels()
{
    ui->labelWeightValue->setText(tr("n/a"));
    ui->labelNetworkWeightValue->setText(tr("n/a"));
    ui->labelLockedFromValue->setText(tr("n/a"));
    ui->labelLockedUntilValue->setText(tr("n/a"));
    ui->labelLastEarningsDateValue->setText(tr("n/a"));
    ui->labelWitnessEarningsValue->setText(tr("n/a"));
    ui->labelLockDurationValue->setText(tr("n/a"));
    ui->labelExpectedEarningsDurationValue->setText(tr("n/a"));
    ui->labelEstimatedEarningsDurationValue->setText(tr("n/a"));
    ui->labelLockTimeRemainingValue->setText(tr("n/a"));
}

WitnessDialog::~WitnessDialog()
{
    LOG_QT_METHOD;

    {
        std::lock_guard<std::mutex> lock(statsUpdateMutex);
        statsUpdateShouldStop = true;
    }
    statsUpdateCondition.notify_all();
    statsUpdateThread.join();

    delete ui;
}

void WitnessDialog::viewWitnessInfoClicked()
{
    LOG_QT_METHOD;

    userWidgetIndex = userWidgetIndex < 0 ? WitnessDialogStates::STATISTICS : -1;
    doUpdate(true);
}

void WitnessDialog::emptyWitnessClicked()
{
    LOG_QT_METHOD;

    Q_EMIT requestEmptyWitness();
}

void WitnessDialog::fundWitnessClicked()
{
    LOG_QT_METHOD;

    pushDialog(new FundWitnessDialog(model, platformStyle, this));
}

void WitnessDialog::renewWitnessClicked()
{
    LOG_QT_METHOD;

    CAccount* funderAccount = ui->renewWitnessAccountTableView->selectedAccount();
    if (funderAccount)
        Q_EMIT requestRenewWitness(funderAccount);
}

void WitnessDialog::extendClicked()
{
    LOG_QT_METHOD;

    try {
        LOCK2(cs_main, pactiveWallet->cs_wallet);
        CAccount* witnessAccount = pactiveWallet->activeAccount;

        auto [amounts, duration, lockedAmount] = witnessDistribution(pactiveWallet, witnessAccount);
        (unused)amounts;
        (unused)duration;


        const auto accountStatus = GetWitnessAccountStatus(pactiveWallet, witnessAccount);
        uint64_t durationRemaining = GetPoW2RemainingLockLengthInBlocks(accountStatus.nLockUntilBlock, chainActive.Tip()->nHeight);

        pushDialog(new FundWitnessDialog(lockedAmount, durationRemaining, accountStatus.currentWeight + 1, model, platformStyle, this));
    } catch (std::runtime_error& e) {
        GUI::createDialog(this, e.what(), tr("Okay"), QString(""), 400, 180)->exec();
    }

}

void WitnessDialog::optimizeClicked()
{
    LOG_QT_METHOD;

    pushDialog(new OptimizeWitnessDialog(model, platformStyle, this));
}

void WitnessDialog::pushDialog(QWidget *dialog)
{
    ui->stack->addWidget(dialog);
    ui->stack->setCurrentWidget(dialog);
    connect( dialog, SIGNAL( dismiss(QWidget*) ), this, SLOT( popDialog(QWidget*)) );
}

void WitnessDialog::clearDialogStack()
{
    if (ui->stack->count() <= 1)
        return;
    ui->stack->setCurrentIndex(0);
    for (int i = ui->stack->count() - 1; i > 0; i--) {
        QWidget* widget = ui->stack->widget(i);
        ui->stack->removeWidget(widget);
        widget->deleteLater();
    }
}

void WitnessDialog::popDialog(QWidget* dialog)
{
    int index = ui->stack->indexOf(dialog);
    if (index < 1)
        return;
    ui->stack->setCurrentIndex(index - 1);
    for (int i = ui->stack->count() - 1; i >= index; i--) {
        QWidget* widget = ui->stack->widget(i);
        ui->stack->removeWidget(widget);
        widget->deleteLater();
    }
    if (ui->stack->count() <= 1) {
        doUpdate(true);
    }
}

void WitnessDialog::compoundEarningsCheckboxClicked()
{
    LOG_QT_METHOD;

    CAccount* forAccount = model->getActiveAccount();
    if (forAccount)
    {
        LOCK(pactiveWallet->cs_wallet);

        CWalletDB db(*pactiveWallet->dbw);
        if (!ui->compoundEarningsCheckBox->isChecked())
        {
            forAccount->setCompounding(0, &db);
        }
        else
        {
            forAccount->setCompounding(MAX_MONEY, &db); // Attempt to compound as much as the network will allow.
        }
    }
}

void WitnessDialog::unitButtonClicked()
{
    LOG_QT_METHOD;

    unitSelectionMenu->exec(ui->unitButton->mapToGlobal(QPoint(0,0)));
}

void WitnessDialog::updateUnit(int nNewUnit_)
{
    LOG_QT_METHOD;

    if (!model)
        return;

    model->getOptionsModel()->guldenSettings->setWitnessGraphScale(nNewUnit_);
    doUpdate(true);
}

// We assign all earnings to the next fixed point interval.
// e.g. if we are 2 days into week 1 the earning goes to the end of week one.
// The exception being when (roundToFixedPoint) is passed in - which is used to plot the current point in time
// In which case we assign to that exact point instead
static void AddPointToMapWithAdjustedTimePeriod(std::map<double, CAmount>& pointMap, uint64_t nOriginBlock, uint64_t nX, uint64_t nY, uint64_t nDays, int nScale, bool roundToFixedPoint)
{
    double nXF=nX;
    double fDaysModified = nDays;
    if (IsArgSet("-testnet"))
    {
        fDaysModified = (nX - nOriginBlock)/double(DailyBlocksTarget());
    }

    //fixme: (FUT) These various week/month calculations are all very imprecise; they should probably be based on an actual calendar instead.
    switch (nScale)
    {
        case GraphScale::Blocks:
            nXF -= nOriginBlock;
            break;
        case GraphScale::Days:
            nXF = fDaysModified;
            break;
        case GraphScale::Weeks:
            nXF = fDaysModified/7;
            break;
        case GraphScale::Months:
            nXF = fDaysModified/30;
            break;
    }
    nX = std::floor(nXF)+1;
    if (roundToFixedPoint)
        pointMap[nX] += nY;
    else
        pointMap[nXF] += nY;
}

WitnessInfoForAccount WitnessDialog::GetWitnessInfoForAccount(CAccount* forAccount, const CWitnessAccountStatus& accountStatus) const
{
    LOG_QT_METHOD;

    if(!filter || !model)
        throw std::runtime_error("filter && model required");

    WitnessInfoForAccount infoForAccount;

    infoForAccount.accountStatus = accountStatus;

    infoForAccount.nTotalNetworkWeightTip = accountStatus.networkWeight;
    infoForAccount.nOurWeight = accountStatus.currentWeight;

    // the lock period could have been different initially if it was extended, this is not accounted for
    infoForAccount.nOriginLength = accountStatus.nLockPeriodInBlocks;

    // if the witness was extended or rearranged the initial weight will have been different, this is not accounted for
    infoForAccount.nOriginWeight = accountStatus.originWeight;

    LOCK(cs_main);
    infoForAccount.nOriginBlock = accountStatus.nLockFromBlock;
    CBlockIndex* originIndex = chainActive[infoForAccount.nOriginBlock];
    infoForAccount.originDate = QDateTime::fromTime_t(originIndex->GetBlockTime());

    // We take the network weight 100 blocks ahead to give a chance for our own weight to filter into things (and also if e.g. the first time witnessing activated - testnet - then weight will only climb once other people also join)
    CBlockIndex* sampleWeightIndex = chainActive[infoForAccount.nOriginBlock+100 > (uint64_t)chainActive.Tip()->nHeight ? infoForAccount.nOriginBlock : infoForAccount.nOriginBlock+100];
    int64_t nUnused1;
    if (!GetPow2NetworkWeight(sampleWeightIndex, Params(), nUnused1, infoForAccount.nOriginNetworkWeight, chainActive))
    {
        std::string strErrorMessage = "Error in witness dialog, failed to get weight for account";
        CAlert::Notify(strErrorMessage, true, true);
        LogPrintf("%s", strErrorMessage.c_str());
        throw std::runtime_error(strErrorMessage);
    }

    infoForAccount.scale = (GraphScale)model->getOptionsModel()->guldenSettings->getWitnessGraphScale();

    infoForAccount.pointMapForecast[0] = 0;

    // fixme: (PHASE5) Use only rewards of current locked witness amounts, not of previous ones after a re-fund...
    // Extract details for every witness reward we have received.
    filter->setAccountFilter(forAccount);
    int rows = filter->rowCount();
    for (int row = 0; row < rows; ++row)
    {
        QModelIndex index = filter->index(row, 0);

        int nDepth = filter->data(index, TransactionTableModel::DepthRole).toInt();
        if ( nDepth > 0 )
        {
            int nType = filter->data(index, TransactionTableModel::TypeRole).toInt();
            if (nType == TransactionRecord::GeneratedWitness)
            {
                int nX = filter->data(index, TransactionTableModel::TxBlockHeightRole).toInt();
                if (nX > 0)
                {
                    infoForAccount.lastEarningsDate = filter->data(index, TransactionTableModel::DateRole).toDateTime();
                    uint64_t nY = filter->data(index, TransactionTableModel::AmountRole).toLongLong()/COIN;
                    infoForAccount.nEarningsToDate += nY;
                    uint64_t nDays = infoForAccount.originDate.daysTo(infoForAccount.lastEarningsDate);
                    AddPointToMapWithAdjustedTimePeriod(infoForAccount.pointMapGenerated, infoForAccount.nOriginBlock, nX, nY, nDays, infoForAccount.scale, true);
                }
            }
        }
    }

    QDateTime tipTime = QDateTime::fromTime_t(chainActive.Tip()->GetBlockTime());
    // One last datapoint for 'current' block.
    if (!infoForAccount.pointMapGenerated.empty())
    {
        uint64_t nY = infoForAccount.pointMapGenerated.rbegin()->second;
        uint64_t nX = chainActive.Tip()->nHeight;
        uint64_t nDays = infoForAccount.originDate.daysTo(tipTime);
        infoForAccount.pointMapGenerated.erase(--infoForAccount.pointMapGenerated.end());
        AddPointToMapWithAdjustedTimePeriod(infoForAccount.pointMapGenerated, infoForAccount.nOriginBlock, nX, nY, nDays, infoForAccount.scale, false);
    }

    // Using the origin block details gathered from previous loop, generate the points for a 'forecast' of how much the account should earn over its entire existence.
    infoForAccount.nWitnessLength = infoForAccount.nOriginLength;
    if (infoForAccount.nOriginNetworkWeight == 0)
        infoForAccount.nOriginNetworkWeight = gStartingWitnessNetworkWeightEstimate;
    uint64_t nEstimatedWitnessBlockPeriodOrigin = estimatedWitnessBlockPeriod((infoForAccount.nOriginWeight>0)?infoForAccount.nOriginWeight:gMinimumWitnessWeight, infoForAccount.nOriginNetworkWeight);
    infoForAccount.pointMapForecast[0] = 0;
    for (unsigned int i = nEstimatedWitnessBlockPeriodOrigin; i < infoForAccount.nWitnessLength; i += nEstimatedWitnessBlockPeriodOrigin)
    {
        unsigned int nX = i;
        uint64_t nDays = infoForAccount.originDate.daysTo(tipTime.addSecs(i*Params().GetConsensus().nPowTargetSpacing));
        AddPointToMapWithAdjustedTimePeriod(infoForAccount.pointMapForecast, 0, nX, 20, nDays, infoForAccount.scale, true);
    }

    const auto& parts = infoForAccount.accountStatus.parts;
    if (!parts.empty()) {
        uint64_t networkWeight = infoForAccount.nTotalNetworkWeightTip;
        // Worst case all parts witness at latest oppertunity so part with maximum weight will be the first to be required to witness
        infoForAccount.nExpectedWitnessBlockPeriod = expectedWitnessBlockPeriod(*std::max_element(parts.begin(), parts.end()), networkWeight);

        // Combine estimated witness frequency f for part frequencies f1..fN: 1/f = 1/f1 + .. 1/fN
        double fInv = std::accumulate(parts.begin(), parts.end(), 0.0, [=](const double acc, const uint64_t w){
            uint64_t fn = estimatedWitnessBlockPeriod(w, networkWeight);
            return acc + 1.0/fn;
        });
        infoForAccount.nEstimatedWitnessBlockPeriod = uint64_t(1.0/fInv);
    }

    infoForAccount.nLockBlocksRemaining = GetPoW2RemainingLockLengthInBlocks(accountStatus.nLockUntilBlock, chainActive.Tip()->nHeight);

    return infoForAccount;
}

void WitnessDialog::update()
{
    LOG_QT_METHOD;
    doUpdate(false);
}


bool WitnessDialog::doUpdate(bool forceUpdate, WitnessStatus* pWitnessStatus)
{
    // rate limit this expensive UI update when chain tip is still far from known height
    int heightRemaining = clientModel->cachedProbableHeight - chainActive.Height();
    if (!forceUpdate && heightRemaining > 10 && heightRemaining % 100 != 0)
        return true;

    LOG_QT_METHOD;

    DO_BENCHMARK("WIT: WitnessDialog::update", BCLog::BENCH|BCLog::WITNESS);

    // If SegSig is enabled then allow possibility of witness compounding.
    ui->compoundEarningsCheckBox->setVisible(IsSegSigEnabled(chainActive.TipPrev()));

    WitnessDialogStates setWidgetIndex = WitnessDialogStates::EMPTY;
    int computedWidgetIndex = -1;

    bool stateEmptyWitnessButton = false;
    bool stateWithdrawEarningsButton = false;
    bool stateWithdrawEarningsButton2 = false;
    bool stateRenewWitnessButton = false;
    bool stateExtendButton = false;
    bool stateOptimizeButton = false;

    bool succes = false;

    CWitnessAccountStatus accountStatus;
    try
    {
        CAccount* forAccount;
        if (!model || !(forAccount = model->getActiveAccount()) || !forAccount->IsPoW2Witness())
            throw std::runtime_error("Witness account not accessible");

        //fixme: (PHASE5) - Compounding (via RPC at least) is not a binary setting, so display this as semi checked (or something else) when its in a non binary state.
        ui->compoundEarningsCheckBox->setChecked((forAccount->getCompounding() != 0));

        CGetWitnessInfo witnessInfo;
        accountStatus = GetWitnessAccountStatus(pactiveWallet, forAccount, &witnessInfo);
        if (pWitnessStatus != nullptr)
            *pWitnessStatus = accountStatus.status;

        bool hasSpendableBalance = pactiveWallet->GetBalance(forAccount, false, false, true) > 0;

        //Witness only witness account skips all the fancy states for now and just always shows the statistics page
        if (forAccount->m_Type == WitnessOnlyWitnessAccount)
        {
            computedWidgetIndex = WitnessDialogStates::STATISTICS;
            if (accountStatus.status != WitnessStatus::Empty)
            {
                stateWithdrawEarningsButton = true;
            }
        }
        else
        {
            switch (accountStatus.status)
            {
                case WitnessStatus::Empty:
                {
                    computedWidgetIndex = WitnessDialogStates::EMPTY;
                    break;
                }
                case WitnessStatus::EmptyWithRemainder:
                {
                    computedWidgetIndex = WitnessDialogStates::FINAL;
                    stateEmptyWitnessButton = true;
                    break;
                }
                case WitnessStatus::Pending:
                {
                    computedWidgetIndex = WitnessDialogStates::PENDING;
                    break;
                }
                case WitnessStatus::Witnessing:
                {
                    computedWidgetIndex = WitnessDialogStates::STATISTICS;
                    stateOptimizeButton = !isWitnessDistributionNearOptimal(pactiveWallet, forAccount, witnessInfo);
                    stateExtendButton = IsSegSigEnabled(chainActive.TipPrev()) && !accountStatus.hasUnconfirmedWittnessTx;
                    break;
                }
                case WitnessStatus::Ended:
                {
                    computedWidgetIndex = WitnessDialogStates::FINAL;
                    stateEmptyWitnessButton = !accountStatus.hasUnconfirmedWittnessTx;
                    break;
                }
                case WitnessStatus::Expired:
                {
                    computedWidgetIndex = WitnessDialogStates::EXPIRED;
                    stateRenewWitnessButton = !accountStatus.hasUnconfirmedWittnessTx;
                    stateExtendButton = IsSegSigEnabled(chainActive.TipPrev()) && accountStatus.hasUnconfirmedWittnessTx;
                    break;
                }
                case WitnessStatus::Emptying:
                {
                    computedWidgetIndex = WitnessDialogStates::FINAL;
                    break;
                }
            }
        }
        stateWithdrawEarningsButton = hasSpendableBalance && accountStatus.status == WitnessStatus::Witnessing;
        stateWithdrawEarningsButton2 = hasSpendableBalance && accountStatus.status == WitnessStatus::Expired;
        

        setWidgetIndex = WitnessDialogStates(userWidgetIndex >= 0 ? userWidgetIndex : computedWidgetIndex);

        if (computedWidgetIndex == WitnessDialogStates::STATISTICS)
        {
            requestStatisticsUpdate(accountStatus);
        }
        succes = true;
    }
    catch (const std::runtime_error& e)
    {
        computedWidgetIndex = setWidgetIndex = WitnessDialogStates::STATE_ERROR;
        ui->labelErrorInfo->setText(e.what());
    }

    ui->witnessDialogStackedWidget->setCurrentIndex(setWidgetIndex);

    if (setWidgetIndex == WitnessDialogStates::STATISTICS && computedWidgetIndex != WitnessDialogStates::STATISTICS)
    {
        ui->viewWitnessGraphButton->setText(tr("Close graph"));
        ui->viewWitnessGraphButton->setVisible(true);
    }
    else if (setWidgetIndex != WitnessDialogStates::STATISTICS && (computedWidgetIndex == WitnessDialogStates::EXPIRED || computedWidgetIndex == WitnessDialogStates::FINAL))
    {
        ui->viewWitnessGraphButton->setText(tr("Show graph"));
        ui->viewWitnessGraphButton->setVisible(true);
        requestStatisticsUpdate(accountStatus);
    }
    else
    {
        ui->viewWitnessGraphButton->setVisible(false);
    }

    ui->unitButton->setVisible(setWidgetIndex == WitnessDialogStates::STATISTICS);

    ui->emptyWitnessButton->setVisible(stateEmptyWitnessButton);
    ui->withdrawEarningsButton->setVisible(stateWithdrawEarningsButton);
    ui->withdrawEarningsButton2->setVisible(stateWithdrawEarningsButton2);
    ui->renewWitnessButton->setVisible(stateRenewWitnessButton);
    ui->extendButton->setVisible(stateExtendButton);
    ui->optimizeButton->setVisible(stateOptimizeButton);

    // put normal buttons to the right if there are no confirming buttons visible
    bool anyConfirmVisible = stateEmptyWitnessButton || stateWithdrawEarningsButton || stateRenewWitnessButton;
    auto rect = ui->horizontalSpacer->geometry();
    rect.setWidth(anyConfirmVisible ? 40 : 0);
    ui->horizontalSpacer->setGeometry(rect);

    return succes;
}

void WitnessDialog::requestStatisticsUpdate(const CWitnessAccountStatus& accountStatus)
{
    {
        std::lock_guard<std::mutex> lock(statsUpdateMutex);
        statsUpdateAccountStatus = accountStatus;
        statsUpdateShouldUpdate = true;
    }
    statsUpdateCondition.notify_one();
}

void WitnessDialog::updateStatisticsThreadLoop()
{
    while (true) {
        std::unique_lock<std::mutex> lock(statsUpdateMutex);
        statsUpdateCondition.wait(lock, [this] { return statsUpdateShouldStop || statsUpdateShouldUpdate; });

        if (statsUpdateShouldStop)
            break;

        CWitnessAccountStatus accountStatus = std::move(statsUpdateAccountStatus);
        statsUpdateShouldUpdate = false;

        lock.unlock();

        WitnessInfoForAccount witnessInfoForAccount = GetWitnessInfoForAccount(accountStatus.account, accountStatus);


        QMetaObject::invokeMethod(this, "displayUpdatedStatistics",
                                  Qt::QueuedConnection,
                                  Q_ARG(WitnessInfoForAccount, witnessInfoForAccount));
    }
}

void WitnessDialog::displayUpdatedStatistics(const WitnessInfoForAccount& infoForAccount)
{
    LOG_QT_METHOD;

    // Populate graph with info
    {

        // Populate the 'expected earnings' curve
        QPolygonF forecastedPoints;
        CAmount forecastTotal = 0;
        int nXForecast = 0;
        for (const auto& pointIter : infoForAccount.pointMapForecast)
        {
            forecastTotal += pointIter.second;
            nXForecast = pointIter.first;
            forecastedPoints << QPointF(nXForecast, forecastTotal);
        }

        // Populate the 'actual earnings' curve.
        QPolygonF generatedPoints;
        CAmount generatedTotal = 0;
        for (const auto& pointIter : infoForAccount.pointMapGenerated)
        {
            generatedTotal += pointIter.second;
            generatedPoints << QPointF(pointIter.first, generatedTotal);
        }
        if (generatedPoints.size() > 1)
        {
            (generatedPoints.rbegin())->setY(infoForAccount.nEarningsToDate);
        }

        //fixme: (PHASE5) This is a bit broken - use nOurWeight etc.
        // Fill in the remaining time on the 'actual earnings' curve with a forecast.
        QPolygonF generatedPointsForecast;
        if (generatedPoints.size() > 0)
        {
            generatedPointsForecast << generatedPoints.back();
            for (const auto& pointIter : infoForAccount.pointMapForecast)
            {
                double forecastOffset = generatedPoints.back().x();
                generatedTotal += pointIter.second;
                generatedPointsForecast << QPointF(pointIter.first+forecastOffset, generatedTotal);
            }
        }

        expectedEarningsCurve->setSamples( forecastedPoints );
        currentEarningsCurveShadow->setSamples( generatedPoints );
        currentEarningsCurve->setSamples( generatedPoints );
        currentEarningsCurveForecastShadow->setSamples( generatedPointsForecast );
        currentEarningsCurveForecast->setSamples( generatedPointsForecast );
        ui->witnessEarningsPlot->setAxisScale( QwtPlot::xBottom, 0, nXForecast);
        ui->witnessEarningsPlot->setAxisScale( QwtPlot::yLeft, 0, std::max(forecastTotal, generatedTotal));
        ui->witnessEarningsPlot->replot();
    }


    // Populate stats table with info
    {
        QString lastEarningsDateLabel = tr("n/a");
        QString earningsToDateLabel = lastEarningsDateLabel;
        QString networkWeightLabel = lastEarningsDateLabel;
        QString lockDurationLabel = lastEarningsDateLabel;
        QString expectedEarningsDurationLabel = lastEarningsDateLabel;
        QString estimatedEarningsDurationLabel = lastEarningsDateLabel;
        QString lockTimeRemainingLabel = lastEarningsDateLabel;
        QString lockTimeElapsedLabel = lastEarningsDateLabel;
        QString labelWeightValue = lastEarningsDateLabel;
        QString lockedUntilValue = lastEarningsDateLabel;
        QString lockedFromValue = lastEarningsDateLabel;
        QString partCountValue = QString::number(infoForAccount.accountStatus.parts.size());

        if (infoForAccount.nOurWeight > 0)
            labelWeightValue = QString::number(infoForAccount.nOurWeight);
        if (!infoForAccount.originDate.isNull())
            lockedFromValue = infoForAccount.originDate.toString("dd/MM/yy hh:mm");
        if (chainActive.Tip())
        {
            QDateTime lockedUntilDate;
            lockedUntilDate.setTime_t(chainActive.Tip()->nTime);
            lockedUntilDate = lockedUntilDate.addSecs(infoForAccount.nLockBlocksRemaining*150);
            lockedUntilValue = lockedUntilDate.toString("dd/MM/yy hh:mm");
        }

        if (!infoForAccount.lastEarningsDate.isNull())
            lastEarningsDateLabel = infoForAccount.lastEarningsDate.toString("dd/MM/yy hh:mm");
        if (infoForAccount.pointMapGenerated.size() != 0)
            earningsToDateLabel = QString::number(infoForAccount.nEarningsToDate);
        if (infoForAccount.nTotalNetworkWeightTip > 0)
            networkWeightLabel = QString::number(infoForAccount.nTotalNetworkWeightTip);

        //fixme: (PHASE5) The below uses "dumb" conversion - i.e. it assumes 30 days in a month, it doesn't look at how many hours in current day etc.
        //Ideally this should be improved.
        //Note if we do improve it we may want to keep the "dumb" behaviour for testnet.
        {
            QString formatStr;
            double divideBy=1;
            int roundTo = 2;
            switch (infoForAccount.scale)
            {
            case GraphScale::Blocks:
                formatStr = tr("%1 blocks");
                divideBy = 1;
                roundTo = 0;
                break;
            case GraphScale::Days:
                formatStr = tr("%1 days");
                divideBy = DailyBlocksTarget();
                break;
            case GraphScale::Weeks:
                formatStr = tr("%1 weeks");
                divideBy = DailyBlocksTarget()*7;
                break;
            case GraphScale::Months:
                formatStr = tr("%1 months");
                divideBy = DailyBlocksTarget()*30;
                break;
            }
            if (infoForAccount.nWitnessLength > 0)
                lockDurationLabel = formatStr.arg(QString::number(infoForAccount.nWitnessLength/divideBy, 'f', roundTo));
            if (infoForAccount.nExpectedWitnessBlockPeriod > 0)
                expectedEarningsDurationLabel = formatStr.arg(QString::number(infoForAccount.nExpectedWitnessBlockPeriod/divideBy, 'f', roundTo));
            if (infoForAccount.nEstimatedWitnessBlockPeriod > 0)
                estimatedEarningsDurationLabel = formatStr.arg(QString::number(infoForAccount.nEstimatedWitnessBlockPeriod/divideBy, 'f', roundTo));
            if (infoForAccount.nLockBlocksRemaining > 0)
                lockTimeRemainingLabel = formatStr.arg(QString::number(infoForAccount.nLockBlocksRemaining/divideBy, 'f', roundTo));
            if (infoForAccount.nLockBlocksRemaining > 0 && infoForAccount.nWitnessLength > 0)
                lockTimeElapsedLabel = formatStr.arg(QString::number(((infoForAccount.nWitnessLength-infoForAccount.nLockBlocksRemaining)/divideBy), 'f', roundTo));
        }

        ui->labelLastEarningsDateValue->setText(lastEarningsDateLabel);
        ui->labelWitnessEarningsValue->setText(earningsToDateLabel);
        ui->labelNetworkWeightValue->setText(networkWeightLabel);
        ui->labelLockDurationValue->setText(lockDurationLabel);
        ui->labelExpectedEarningsDurationValue->setText(expectedEarningsDurationLabel);
        ui->labelEstimatedEarningsDurationValue->setText(estimatedEarningsDurationLabel);
        ui->labelLockTimeRemainingValue->setText(lockTimeRemainingLabel);
        ui->labelLockTimeElapsedValue->setText(lockTimeElapsedLabel);
        ui->labelWeightValue->setText(labelWeightValue);
        ui->labelLockedFromValue->setText(lockedFromValue);
        ui->labelLockedUntilValue->setText(lockedUntilValue);
        ui->labelPartCountValue->setText(partCountValue);
    }

}

void WitnessDialog::updateAccountIndicators()
{
    LOG_QT_METHOD;

    if(!filter || !model)
        return;
    
    // Don't do this during initial block sync i - it can be very slow on a wallet with lots of transactions.
    if (IsInitialBlockDownload())
        return;

    //Don't update for every single block change if we are on testnet and have them streaming in at a super fast speed.
    static uint64_t nUpdateTimerStart = 0;
    static uint64_t nUpdateThrottle = IsArgSet("-testnet") ? 4000 : 1000;
    // Only update at most once every four seconds (prevent this from being a bottleneck on testnet when blocks are coming in fast)
    if (nUpdateTimerStart == 0 || (GetTimeMillis() - nUpdateTimerStart > nUpdateThrottle))
    {
        nUpdateTimerStart = GetTimeMillis();
    }
    else
    {
        return;
    }

    // If we don't yet have any active witness accounts then bypass the expensive GetWitnessInfo() calls.
    // This should make syncing a lot faster for new wallets (for instance)
    bool haveAnyNonShadowWitnessAccounts = false;
    for ( const auto& [uuid, account] : pactiveWallet->mapAccounts )
    {
        (unused)uuid;
        if (account->m_State != AccountState::Shadow && account->IsPoW2Witness())
        {
            haveAnyNonShadowWitnessAccounts = true;
            break;
        }
    }
    if (!haveAnyNonShadowWitnessAccounts)
        return;

    //Update account states to the latest block
    //fixme: (PHASE5) - Some of this is redundant and can possibly be removed; as we set a lot of therse states now from within ::AddToWalletIfInvolvingMe
    //However - we would have to update account serialization to serialise the warning state and/or test some things before removing this.
    {
        LOCK(cs_main); // Required for ReadBlockFromDisk as well as account access.

        std::unique_ptr<TransactionFilterProxy> filter;
        filter.reset(new TransactionFilterProxy);
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::AscendingOrder);

        CGetWitnessInfo witnessInfo;
        CBlock block;
        if (!ReadBlockFromDisk(block, chainActive.Tip(), Params()))
        {
            std::string strErrorMessage = "Error in witness dialog, failed to read block from disk";
            CAlert::Notify(strErrorMessage, true, true);
            LogPrintf("%s", strErrorMessage.c_str());
            return;
        }
        if (!GetWitnessInfo(chainActive, Params(), nullptr, chainActive.Tip()->pprev, block, witnessInfo, chainActive.Tip()->nHeight))
        {
            std::string strErrorMessage = "Error in witness dialog, failed to read witness info";
            CAlert::Notify(strErrorMessage, true, true);
            LogPrintf("%s", strErrorMessage.c_str());
            return;
        }

        LOCK(pactiveWallet->cs_wallet);

        for ( const auto& [uuid, account] : pactiveWallet->mapAccounts )
        {
            (unused)uuid;
            AccountStatus prevState = account->GetWarningState();
            AccountStatus newState = AccountStatus::Default;
            bool bAnyAreMine = false;
            bool bAnyFinished = false;
            if (account->m_State != AccountState::Shadow && account->IsPoW2Witness())
            {
                for (const auto& witCoin : witnessInfo.witnessSelectionPoolUnfiltered)
                {
                    if (IsMine(*account, witCoin.coin.out))
                    {
                        bAnyAreMine = true;
                        CTxOutPoW2Witness details;
                        GetPow2WitnessOutput(witCoin.coin.out, details);
                        if (!IsPoW2WitnessLocked(details, chainActive.Tip()->nHeight))
                        {
                            newState = AccountStatus::WitnessEnded;
                            bAnyFinished = true;
                            break;
                        }
                        else if (witnessHasExpired(witCoin.nAge, witCoin.nWeight, witnessInfo.nTotalWeightRaw))
                        {
                            newState = AccountStatus::WitnessExpired;

                            //fixme: (PHASE5) I think this is only needed for (ended) accounts - and not expired ones? Double check
                            // Due to lock changing cached balance for certain transactions will now be invalidated.
                            // Technically we should find those specific transactions and invalidate them, but it's simpler to just invalidate them all.
                            if (prevState != AccountStatus::WitnessExpired)
                            {
                                for(auto& wtxIter : pactiveWallet->mapWallet)
                                {
                                    wtxIter.second.MarkDirty();
                                }
                            }

                            // If it has pending transactions then it gets the pending flag instead.
                            filter->setAccountFilter(account);
                            int rows = filter->rowCount();
                            for (int row = 0; row < rows; ++row)
                            {
                                QModelIndex index = filter->index(row, 0);

                                int nStatus = filter->data(index, TransactionTableModel::StatusRole).toInt();
                                if (nStatus == TransactionStatus::Status::Unconfirmed)
                                {
                                    newState = AccountStatus::WitnessPending;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!bAnyAreMine && !bAnyFinished)
                {
                    newState = AccountStatus::WitnessEmpty;
                    filter->setAccountFilter(account);
                    int rows = filter->rowCount();

                    for (int row = 0; row < rows; ++row)
                    {
                        QModelIndex index = filter->index(row, 0);

                        int nStatus = filter->data(index, TransactionTableModel::StatusRole).toInt();
                        // If it has pending transactions then it gets the pending flag instead.
                        if (nStatus == TransactionStatus::Status::Unconfirmed)
                        {
                            newState = AccountStatus::WitnessPending;
                            break;
                        }
                        // If it has no balance but does have previous transactions then it is probably a "finished" withness account.
                        if (nStatus == TransactionStatus::Status::Confirmed)
                        {
                            // Due to lock changing cached balance for certain transactions will now be invalidated.
                            // Technically we should find those specific transactions and invalidate them, but it's simpler to just invalidate them all.
                            if (prevState != AccountStatus::WitnessEnded)
                            {
                                for(auto& wtxIter : pactiveWallet->mapWallet)
                                {
                                    wtxIter.second.MarkDirty();
                                }
                            }
                            newState = AccountStatus::WitnessEnded;
                            break;
                        }
                    }
                }
                if (newState != prevState)
                {
                    account->SetWarningState(newState);
                    static_cast<const CExtWallet*>(pactiveWallet)->NotifyAccountWarningChanged(pactiveWallet, account);
                }
            }
        }
    }
}

void WitnessDialog::numBlocksChanged(int,QDateTime)
{
    LOG_QT_METHOD;

    //Update account state indicators for the latest block
    updateAccountIndicators();

    // Update graph and witness info for current account to latest block, skipping updates for blocks that have not been witnessed yet
    bool haveNewWitnessTipHeight = false;
    {
        LOCK(cs_main);
        CBlockIndex* index = chainActive.Tip();
        while (index && index->nVersionPoW2Witness == 0)
            index = index->pprev;
        if (index && prevWitnessedTipHeight != index->nHeight) {
            prevWitnessedTipHeight = index->nHeight;
            haveNewWitnessTipHeight = true;
        }
    }
    if (haveNewWitnessTipHeight)
        doUpdate(false);
}

void WitnessDialog::setClientModel(ClientModel* clientModel_)
{
    LOG_QT_METHOD;

    clientModel = clientModel_;
    if (clientModel)
    {
        connect(clientModel, SIGNAL(numBlocksChanged(int,QDateTime)), this, SLOT(numBlocksChanged(int,QDateTime)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
    }
}

void WitnessDialog::setModel(WalletModel* _model)
{
    LOG_QT_METHOD;

    this->model = _model;

    if(_model && _model->getOptionsModel())
    {
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setDynamicSortFilter(true);
        filter->setSortRole(TransactionTableModel::RoleIndex::DateRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::AscendingOrder);

        {
            ui->renewWitnessAccountTableView->setWalletModel(_model, 1 * COIN);

            connect( _model, SIGNAL( activeAccountChanged(CAccount*) ), this , SLOT( activeAccountChanged(CAccount*) ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
            connect( _model, SIGNAL( accountCompoundingChanged(CAccount*) ), this , SLOT( update() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        }

        activeAccountChanged(model->getActiveAccount());
    }
    else if(model)
    {
        filter.reset(nullptr);
        disconnect( model, SIGNAL( activeAccountChanged(CAccount*) ), this , SLOT( activeAccountChanged(CAccount*) ));
        disconnect( model, SIGNAL( accountCompoundingChanged(CAccount*) ), this , SLOT( update() ));
        model = nullptr;
    }
}

void WitnessDialog::activeAccountChanged(CAccount* account)
{
    if (prevActiveAccount == account || !account || !account->IsPoW2Witness())
    {
        return;
    }

    prevActiveAccount = account;

    userWidgetIndex = -1;
    clearDialogStack();
    WitnessStatus witnessStatus;
    
    // Only show fund dialog for regular witness accounts, and only if we are not still busy syncing
    // Otherwise users may accidentally fund an account twice
    if (doUpdate(true, &witnessStatus))
    {
        if (witnessStatus == WitnessStatus::Empty)
        {
            if (!account->IsWitnessOnly())
            {
                if (!IsInitialBlockDownload())
                {
                    pushDialog(new FundWitnessDialog(model, platformStyle, this));
                }
            }
        }
    }

    if (model)
    {
        ui->renewWitnessAccountTableView->setWalletModel(model, 1 * COIN);
    }
}
