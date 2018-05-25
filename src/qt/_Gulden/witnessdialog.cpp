// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "witnessdialog.h"
#include <qt/_Gulden/forms/ui_witnessdialog.h>
#include <unity/appmanager.h>
#include "GuldenGUI.h"
#include <wallet/wallet.h>
#include "ui_interface.h"
#include <Gulden/mnemonic.h>
#include <vector>
#include "random.h"
#include "walletmodel.h"
#include "clientmodel.h"
#include "optionsmodel.h"
#include "wallet/wallet.h"

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
#include "validation.h"
#include "witnessvalidation.h"

#include "Gulden/util.h"
#include "GuldenGUI.h"

#include "accounttablemodel.h"
#include "consensus/validation.h"

#include <QMenu>

class PlotMouseTracker: public QwtPlotPicker
{
public:
    PlotMouseTracker( QWidget* );

protected:
    virtual QwtText trackerText( const QPoint & ) const;
    virtual QRect trackerRect( const QFont & ) const;

private:
    QString curveInfoAt(QString legendColor,  QString sHeading, const QwtPlotCurve*, const QPoint & ) const;
};

PlotMouseTracker::PlotMouseTracker( QWidget* canvas ):
    QwtPlotPicker( canvas )
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

enum WitnessDialogStates {EMPTY, STATISTICS, EXPIRED, PENDING};

WitnessDialog::WitnessDialog(const QStyle* _platformStyle, QWidget* parent)
: QFrame( parent )
, ui( new Ui::WitnessDialog )
, platformStyle( _platformStyle )
, clientModel(NULL)
, model( NULL )
{
    ui->setupUi(this);

    // Set correct cursor for all clickable UI elements.
    ui->unitButton->setCursor( Qt::PointingHandCursor );
    ui->viewWitnessGraphButton->setCursor( Qt::PointingHandCursor );
    ui->renewWitnessButton->setCursor( Qt::PointingHandCursor );
    ui->emptyWitnessButton->setCursor( Qt::PointingHandCursor );
    ui->emptyWitnessButton2->setCursor( Qt::PointingHandCursor );
    ui->withdrawEarningsButton->setCursor( Qt::PointingHandCursor );
    ui->withdrawEarningsButton2->setCursor( Qt::PointingHandCursor );
    ui->fundWitnessButton->setCursor( Qt::PointingHandCursor );

    // Force qwt graph back to normal cursor instead of cross hair.
    ui->witnessEarningsPlot->canvas()->setCursor(Qt::ArrowCursor);

    ui->fundWitnessAccountTableView->horizontalHeader()->setStretchLastSection(true);
    ui->fundWitnessAccountTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->fundWitnessAccountTableView->horizontalHeader()->hide();
    ui->renewWitnessAccountTableView->horizontalHeader()->setStretchLastSection(true);
    ui->renewWitnessAccountTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->renewWitnessAccountTableView->horizontalHeader()->hide();

    ui->fundWitnessAccountTableView->setContentsMargins(0, 0, 0, 0);
    ui->renewWitnessAccountTableView->setContentsMargins(0, 0, 0, 0);

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

    //fixme: (2.1) Possible leak, or does canvas free it?
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

    connect(ui->unitButton, SIGNAL( clicked() ), this, SLOT( unitButtonClicked() ) );
    connect(ui->viewWitnessGraphButton, SIGNAL( clicked() ), this, SLOT( viewWitnessInfoClicked() ) );
    connect(ui->emptyWitnessButton,  SIGNAL( clicked() ), this, SLOT( emptyWitnessClicked() ) );
    connect(ui->emptyWitnessButton2, SIGNAL( clicked() ), this, SLOT( emptyWitnessClicked() ) );
    connect(ui->withdrawEarningsButton,  SIGNAL( clicked() ), this, SLOT( emptyWitnessClicked() ) );
    connect(ui->withdrawEarningsButton2, SIGNAL( clicked() ), this, SLOT( emptyWitnessClicked() ) );
    connect(ui->fundWitnessButton,   SIGNAL( clicked() ), this, SLOT( fundWitnessClicked() ) );
    connect(ui->renewWitnessButton,  SIGNAL( clicked() ), this, SLOT( renewWitnessClicked() ) );
    connect(unitBlocksAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Blocks); } );
    connect(unitDaysAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Days); } );
    connect(unitWeeksAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Weeks); } );
    connect(unitMonthsAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Months); } );
}

WitnessDialog::~WitnessDialog()
{
    delete ui;
}

void WitnessDialog::viewWitnessInfoClicked()
{
    ui->unitButton->setVisible(true);
    ui->viewWitnessGraphButton->setVisible(false);
    ui->witnessDialogStackedWidget->setCurrentIndex(WitnessDialogStates::STATISTICS);
}

void WitnessDialog::emptyWitnessClicked()
{
    Q_EMIT requestEmptyWitness();
}

void WitnessDialog::fundWitnessClicked()
{
    QModelIndexList selection = ui->fundWitnessAccountTableView->selectionModel()->selectedRows();
    if (selection.count() > 0)
    {
        QModelIndex index = selection.at(0);
        boost::uuids::uuid accountUUID = getUUIDFromString(index.data(AccountTableModel::AccountTableRoles::SelectedAccountRole).toString().toStdString());
        CAccount* funderAccount;
        {
            LOCK(pactiveWallet->cs_wallet);
            funderAccount = pactiveWallet->mapAccounts[accountUUID];
        }
        Q_EMIT requestFundWitness(funderAccount);
    }
}

void WitnessDialog::renewWitnessClicked()
{
    QModelIndexList selection = ui->renewWitnessAccountTableView->selectionModel()->selectedRows();
    if (selection.count() > 0)
    {
        QModelIndex index = selection.at(0);
        boost::uuids::uuid accountUUID = getUUIDFromString(index.data(AccountTableModel::AccountTableRoles::SelectedAccountRole).toString().toStdString());
        CAccount* funderAccount;
        {
            LOCK(pactiveWallet->cs_wallet);
            funderAccount = pactiveWallet->mapAccounts[accountUUID];
        }
        Q_EMIT requestRenewWitness(funderAccount);
    }
}

void WitnessDialog::unitButtonClicked()
{
    unitSelectionMenu->exec(ui->unitButton->mapToGlobal(QPoint(0,0)));
}

void WitnessDialog::updateUnit(int nNewUnit_)
{
    model->getOptionsModel()->guldenSettings->setWitnessGraphScale(nNewUnit_);
    update();
}

void AddPointToMapWithAdjustedTimePeriod(std::map<CAmount, CAmount>& pointMap, uint64_t nOriginBlock, uint64_t nX, uint64_t nY, uint64_t nDays, int nScale)
{
    switch (nScale)
    {
        case GraphScale::Blocks:
            nX -= nOriginBlock;
            break;
        case GraphScale::Days:
            if (IsArgSet("-testnet"))
            {
                nX -= nOriginBlock;
                nX /= 576;
            }
            else
            {
                nX = nDays;
            }
            break;
        case GraphScale::Weeks:
            if (IsArgSet("-testnet"))
            {
                nX -= nOriginBlock;
                nX /= 576;
                nX /= 7;
                ++nX;
            }
            else
            {
                //fixme: (2.0)
                nX = nDays/7;
            }
            break;
        case GraphScale::Months:
            if (IsArgSet("-testnet"))
            {
                nX -= nOriginBlock;
                nX /= 576;
                nX /= 30;
                ++nX;
            }
            else
            {
                //fixme: (2.0)
                nX = nDays/30;
            }
            break;
    }
    pointMap[nX] += nY;
}

void WitnessDialog::plotGraphForAccount(CAccount* account, uint64_t nTotalNetworkWeightTip)
{
    DO_BENCHMARK("WIT: WitnessDialog::plotGraphForAccount", BCLog::BENCH|BCLog::WITNESS);

    GraphScale scale = (GraphScale)model->getOptionsModel()->guldenSettings->getWitnessGraphScale();

    std::map<CAmount, CAmount> pointMapForecast; pointMapForecast[0] = 0;
    std::map<CAmount, CAmount> pointMapGenerated;

    CTxOutPoW2Witness witnessDetails;
    QDateTime lastEarningsDate;
    QDateTime originDate;
    int64_t nOriginNetworkWeight = 0;
    uint64_t nOriginBlock = 0;
    uint64_t nOriginWeight = 0;
    uint64_t nOriginLength = 0;

    // fixme: (2.1) Make this work for multiple 'origin' blocks.
    // Iterate the transaction history and extract all 'origin' block details.
    // Also extract details for every witness reward we have received.
    filter->setAccountFilter(model->getActiveAccount());
    int rows = filter->rowCount();
    for (int row = 0; row < rows; ++row)
    {
        QModelIndex index = filter->index(row, 0);

        int nDepth = filter->data(index, TransactionTableModel::DepthRole).toInt();
        if ( nDepth > 0 )
        {
            int nType = filter->data(index, TransactionTableModel::TypeRole).toInt();
            if ( (nType >= TransactionRecord::WitnessFundRecv) )
            {
                if (nOriginBlock == 0)
                {
                    originDate = filter->data(index, TransactionTableModel::DateRole).toDateTime();
                    nOriginBlock = filter->data(index, TransactionTableModel::TxBlockHeightRole).toInt();

                    // We take the network weight 100 blocks ahead to give a chance for our own weight to filter into things (and also if e.g. the first time witnessing activated - testnet - then weight will only climb once other people also join)
                    CBlockIndex* sampleWeightIndex = chainActive[nOriginBlock+100 > (uint64_t)chainActive.Tip()->nHeight ? nOriginBlock : nOriginBlock+100];
                    int64_t nUnused1;
                    if (!GetPow2NetworkWeight(sampleWeightIndex, Params(), nUnused1, nOriginNetworkWeight, chainActive))
                    {
                        //fixme: (2.0) Error handling
                        return;
                    }
                    pointMapGenerated[0] = 0;

                    uint256 originHash;
                    originHash.SetHex( filter->data(index, TransactionTableModel::TxHashRole).toString().toStdString());

                    LOCK2(cs_main, pactiveWallet->cs_wallet);
                    auto walletTxIter = pactiveWallet->mapWallet.find(originHash);
                    if(walletTxIter == pactiveWallet->mapWallet.end())
                    {
                        return;
                    }

                    for (unsigned int i=0; i<walletTxIter->second.tx->vout.size(); ++i)
                    {
                        //fixme: (2.0) Handle multiple in one tx. Check ismine. Handle none (regular transaction)
                        if (GetPow2WitnessOutput(walletTxIter->second.tx->vout[i], witnessDetails))
                        {
                            break;
                        }
                    }
                    nOriginLength = witnessDetails.lockUntilBlock - (witnessDetails.lockFromBlock > 0 ? witnessDetails.lockFromBlock : nOriginBlock);
                    nOriginWeight = GetPoW2RawWeightForAmount(filter->data(index, TransactionTableModel::AmountRole).toLongLong(), nOriginLength);
                }
                //fixme (2.0) (HIGH): changes in witness weight... (multiple witnesses in single account etc.)
            }
            else if (nType == TransactionRecord::GeneratedWitness)
            {
                int nX = filter->data(index, TransactionTableModel::TxBlockHeightRole).toInt();
                if (nX > 0)
                {
                    lastEarningsDate = filter->data(index, TransactionTableModel::DateRole).toDateTime();
                    uint64_t nY = filter->data(index, TransactionTableModel::AmountRole).toLongLong()/COIN;
                    uint64_t nDays = originDate.daysTo(lastEarningsDate);
                    AddPointToMapWithAdjustedTimePeriod(pointMapGenerated, nOriginBlock, nX, nY, nDays, scale);
                }
            }
        }
    }

    // One last datapoint for 'current' block.
    if (!pointMapGenerated.empty())
    {
        uint64_t nY = pointMapGenerated.rbegin()->second;
        uint64_t nX = chainActive.Tip()->nHeight;
        QDateTime tipTime = QDateTime::fromTime_t(chainActive.Tip()->GetBlockTime());
        uint64_t nDays = originDate.daysTo(tipTime);
        AddPointToMapWithAdjustedTimePeriod(pointMapGenerated, nOriginBlock, nX, nY, nDays, scale);
    }

    // Using the origin block details gathered from previous loop, generate the points for a 'forecast' of how much the account should earn over its entire existence.
    uint64_t nWitnessLength = nOriginLength;
    if (nOriginNetworkWeight == 0)
        nOriginNetworkWeight = nStartingWitnessNetworkWeightEstimate;
    uint64_t nEstimatedWitnessBlockPeriodOrigin = estimatedWitnessBlockPeriod(nOriginWeight, nOriginNetworkWeight);
    pointMapForecast[0] = 0;
    for (unsigned int i = nEstimatedWitnessBlockPeriodOrigin; i < nWitnessLength; i += nEstimatedWitnessBlockPeriodOrigin)
    {
        unsigned int nX = i;
        //fixme: (2.0) (HIGH)
        uint64_t nDays = 0;
        AddPointToMapWithAdjustedTimePeriod(pointMapForecast, 0, nX, 20, nDays, scale);
    }

    // Populate the 'expected earnings' curve
    CAmount nTotal1 = 0;
    int nXForecast = 0;
    QPolygonF forecastedPoints;
    for (const auto& pointIter : pointMapForecast)
    {
        nTotal1 += pointIter.second;
        nXForecast = pointIter.first;
        forecastedPoints << QPointF(nXForecast, nTotal1);
    }
    expectedEarningsCurve->setSamples( forecastedPoints );
    ui->witnessEarningsPlot->setAxisScale( QwtPlot::xBottom, 0, nXForecast);

    // Populate the 'actual earnings' curve.
    CAmount nTotal2 = 0;
    int nXGenerated = 0;
    QPolygonF generatedPoints;
    for (const auto& pointIter : pointMapGenerated)
    {
        nTotal2 += pointIter.second;
        nXGenerated = pointIter.first;
        generatedPoints << QPointF(pointIter.first, nTotal2);
    }
    if (generatedPoints.size() > 1)
    {
        generatedPoints.back().setY(generatedPoints[generatedPoints.size()-2].y());
    }
    currentEarningsCurveShadow->setSamples( generatedPoints );
    currentEarningsCurve->setSamples( generatedPoints );

    // Fill in the remaining time on the 'actual earnings' curve with a forecast.
    QPolygonF generatedPointsForecast;
    if (generatedPoints.size() > 0)
    {
        generatedPointsForecast << generatedPoints.back();
        for (const auto& pointIter : pointMapForecast)
        {
            nXForecast = pointIter.first;
            if (nXForecast > nXGenerated)
            {
                nTotal2 += pointIter.second;
                generatedPointsForecast << QPointF(nXForecast, nTotal2);
            }
        }
    }
    currentEarningsCurveForecastShadow->setSamples( generatedPointsForecast );
    currentEarningsCurveForecast->setSamples( generatedPointsForecast );


    ui->witnessEarningsPlot->setAxisScale( QwtPlot::yLeft, 0, std::max(nTotal1, nTotal2));
    ui->witnessEarningsPlot->replot();


    // Update all the info labels with values calculated above.
    uint64_t nLockBlocksRemaining = witnessDetails.lockUntilBlock <= (uint64_t)chainActive.Tip()->nHeight ? 0 : witnessDetails.lockUntilBlock - chainActive.Tip()->nHeight;
    ui->labelWeightValue->setText(nOriginWeight<=0 ? tr("n/a") : QString::number(nOriginWeight));
    ui->labelLockedFromValue->setText(originDate.isNull() ? tr("n/a") : originDate.toString("dd/MM/yy hh:mm"));
    if (!chainActive.Tip())
    {
        ui->labelLockedUntilValue->setText( tr("n/a") );
    }
    else
    {
        QDateTime lockedUntilDate;
        lockedUntilDate.setTime_t(chainActive.Tip()->nTime);
        lockedUntilDate = lockedUntilDate.addSecs(nLockBlocksRemaining*150);
        ui->labelLockedUntilValue->setText(lockedUntilDate.toString("dd/MM/yy hh:mm"));
    }
    ui->labelLastEarningsDateValue->setText(lastEarningsDate.isNull() ? tr("n/a") : lastEarningsDate.toString("dd/MM/yy hh:mm"));
    ui->labelWitnessEarningsValue->setText(generatedPoints.size() == 0 ? tr("n/a") : QString::number(generatedPoints.back().y(), 'f', 2));

    uint64_t nExpectedWitnessBlockPeriod = expectedWitnessBlockPeriod(nOriginWeight, nTotalNetworkWeightTip);
    uint64_t nEstimatedWitnessBlockPeriod = estimatedWitnessBlockPeriod(nOriginWeight, nTotalNetworkWeightTip);
    ui->labelNetworkWeightValue->setText(nTotalNetworkWeightTip<=0 ? tr("n/a") : QString::number(nTotalNetworkWeightTip));
    switch (scale)
    {
        case GraphScale::Blocks:
            ui->labelLockDurationValue->setText(nWitnessLength <= 0 ? tr("n/a") : tr("%1 blocks").arg(QString::number(nWitnessLength)));
            ui->labelExpectedEarningsDurationValue->setText(nExpectedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 blocks").arg(QString::number(nExpectedWitnessBlockPeriod)));
            ui->labelEstimatedEarningsDurationValue->setText(nEstimatedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 blocks").arg(QString::number(nEstimatedWitnessBlockPeriod)));
            ui->labelLockTimeRemainingValue->setText(nLockBlocksRemaining <= 0 ? tr("n/a") : tr("%1 blocks").arg(QString::number(nLockBlocksRemaining)));
            break;
        case GraphScale::Days:
            if (IsArgSet("-testnet"))
            {
                ui->labelLockDurationValue->setText(nWitnessLength <= 0 ? tr("n/a") : tr("%1 days").arg(QString::number(nWitnessLength/576.0, 'f', 2)));
                ui->labelExpectedEarningsDurationValue->setText(nExpectedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 days").arg(QString::number(nExpectedWitnessBlockPeriod/576.0, 'f', 2)));
                ui->labelEstimatedEarningsDurationValue->setText(nEstimatedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 days").arg(QString::number(nEstimatedWitnessBlockPeriod/576.0, 'f', 2)));
                ui->labelLockTimeRemainingValue->setText(nLockBlocksRemaining <= 0 ? tr("n/a") : tr("%1 days").arg(QString::number(nLockBlocksRemaining/576.0, 'f', 2)));
            }
            else
            {
                //fixme: (2.0) - Implement
            }
            break;
        case GraphScale::Weeks:
            if (IsArgSet("-testnet"))
            {
                ui->labelLockDurationValue->setText(nWitnessLength <= 0 ? tr("n/a") : tr("%1 weeks").arg(QString::number(nWitnessLength/576.0/7.0, 'f', 2)));
                ui->labelExpectedEarningsDurationValue->setText(nExpectedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 weeks").arg(QString::number(nExpectedWitnessBlockPeriod/576.0/7.0, 'f', 2)));
                ui->labelEstimatedEarningsDurationValue->setText(nEstimatedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 weeks").arg(QString::number(nEstimatedWitnessBlockPeriod/576.0/7.0, 'f', 2)));
                ui->labelLockTimeRemainingValue->setText(nLockBlocksRemaining <= 0 ? tr("n/a") : tr("%1 weeks").arg(QString::number(nLockBlocksRemaining/576.0/7.0, 'f', 2)));
            }
            else
            {
                //fixme: (2.0) - Implement
            }
            break;
        case GraphScale::Months:
            if (IsArgSet("-testnet"))
            {
                ui->labelLockDurationValue->setText(nWitnessLength <= 0 ? tr("n/a") : tr("%1 months").arg(QString::number(nWitnessLength/576.0/30.0, 'f', 2)));
                ui->labelExpectedEarningsDurationValue->setText(nExpectedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 months").arg(QString::number(nExpectedWitnessBlockPeriod/576.0/30.0, 'f', 2)));
                ui->labelEstimatedEarningsDurationValue->setText(nEstimatedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 months").arg(QString::number(nEstimatedWitnessBlockPeriod/576.0/30.0, 'f', 2)));
                ui->labelLockTimeRemainingValue->setText(nLockBlocksRemaining <= 0 ? tr("n/a") : tr("%1 months").arg(QString::number(nLockBlocksRemaining/576.0/30.0, 'f', 2)));
            }
            else
            {
                //fixme: (2.0) - Implement
            }
            break;
    }
}

void WitnessDialog::update()
{
    DO_BENCHMARK("WIT: WitnessDialog::update", BCLog::BENCH|BCLog::WITNESS);

    WitnessDialogStates setIndex = WitnessDialogStates::EMPTY;
    bool stateEmptyWitnessButton = false;
    bool stateEmptyWitnessButton2 = false;
    bool stateWithdrawEarningsButton = false;
    bool stateWithdrawEarningsButton2 = false;
    bool stateFundWitnessButton = true;
    bool stateRenewWitnessButton = false;
    bool stateUnitButton = false;
    bool stateViewWitnessGraphButton = false;

    ui->fundWitnessAccountTableView->update();
    ui->renewWitnessAccountTableView->update();
    // Perform a default selection, so that for simple cases (e.g. wallet with only 1 account) user does not need to manually select.
    if (ui->fundWitnessAccountTableView->selectionModel()->selectedRows().count() == 0)
    {
        ui->fundWitnessAccountTableView->setSelectionMode(QAbstractItemView::SingleSelection);
        ui->fundWitnessAccountTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->fundWitnessAccountTableView->selectRow(0);
    }
    if (ui->renewWitnessAccountTableView->selectionModel()->selectedRows().count() == 0)
    {
        ui->renewWitnessAccountTableView->setSelectionMode(QAbstractItemView::SingleSelection);
        ui->renewWitnessAccountTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->renewWitnessAccountTableView->selectRow(0);
    }

    if (model)
    {
        CAccount* forAccount = model->getActiveAccount();
        if (forAccount)
        {
            if ( forAccount->IsPoW2Witness() )
            {
                uint64_t nTotalNetworkWeight = 0;
                bool bAnyExpired = false;
                bool bAnyAreMine = false;
                if (chainActive.Tip() && chainActive.Tip()->pprev)
                {
                    static CAccount* cachedForAccount = nullptr;
                    static uint256 cachedHashTip;
                    static AccountStatus cachedWarningState = AccountStatus::Default;

                    // Prevent same update being called repeatedly for same account/tip (can happen in some event sequences)
                    if (cachedForAccount == forAccount && cachedHashTip == chainActive.Tip()->GetBlockHashPoW2() && cachedWarningState == forAccount->GetWarningState())
                        return;
                    cachedForAccount = forAccount;
                    cachedHashTip = chainActive.Tip()->GetBlockHashPoW2();
                    cachedWarningState = forAccount->GetWarningState();

                    CGetWitnessInfo witnessInfo;
                    CBlock block;
                    {
                        LOCK(cs_main); // Required for ReadBlockFromDisk as well as GetWitnessInfo.
                        //fixme: (2.0) Error handling
                        if (!ReadBlockFromDisk(block, chainActive.Tip(), Params().GetConsensus()))
                            return;
                        if (!GetWitnessInfo(chainActive, Params(), nullptr, chainActive.Tip()->pprev, block, witnessInfo, chainActive.Tip()->nHeight))
                            return;
                        nTotalNetworkWeight = witnessInfo.nTotalWeight;
                    }
                    for (const auto& witCoin : witnessInfo.witnessSelectionPoolUnfiltered)
                    {
                        if (IsMine(*forAccount, witCoin.coin.out))
                        {
                            bAnyAreMine = true;
                            if (witnessHasExpired(witCoin.nAge, witCoin.nWeight, witnessInfo.nTotalWeight))
                            {
                                bAnyExpired = true;
                            }
                        }
                    }

                    // We have to check for immature balance as well - otherwise accounts that have just witnessed get incorrectly marked as "empty".
                    if (pactiveWallet->GetBalance(forAccount, true, true) > 0 || pactiveWallet->GetImmatureBalance(forAccount) > 0)
                    {
                        stateFundWitnessButton = false;
                        if (bAnyExpired || !bAnyAreMine)
                        {
                            filter->setAccountFilter(model->getActiveAccount());
                            int rows = filter->rowCount();
                            for (int row = 0; row < rows; ++row)
                            {
                                QModelIndex index = filter->index(row, 0);

                                int nStatus = filter->data(index, TransactionTableModel::StatusRole).toInt();
                                if (nStatus == TransactionStatus::Status::Unconfirmed)
                                {
                                    setIndex = WitnessDialogStates::PENDING;
                                    break;
                                }
                            }
                            if (bAnyExpired)
                            {
                                if (pactiveWallet->GetBalance(forAccount, true, true) == pactiveWallet->GetBalance(forAccount, false, true))
                                    stateEmptyWitnessButton2 = true;
                                else
                                    stateWithdrawEarningsButton2 = true;

                                // If the account is not flagged as expired then we might have already renewed, or we might have ended our lock period.
                                // We could also just be waiting for the next block to come in for the flag to update.
                                // Keep the button visible, so the user is at least aware that it exists, but disable it so that it can't be used until we are sure.
                                if (cachedWarningState == AccountStatus::WitnessExpired)
                                    stateRenewWitnessButton = true;
                                else
                                {
                                    stateRenewWitnessButton = false;
                                    stateEmptyWitnessButton = stateEmptyWitnessButton2;
                                    stateWithdrawEarningsButton = stateWithdrawEarningsButton2;
                                    stateEmptyWitnessButton2 = stateWithdrawEarningsButton2 = false;
                                }
                                stateViewWitnessGraphButton = true;
                                setIndex = WitnessDialogStates::EXPIRED;
                                plotGraphForAccount(forAccount, nTotalNetworkWeight);
                            }
                            // Second check for pending is required because first check might fail (when calling update during funding an account for first time) - while second check might not be right in some cases either so we rather have both.
                            else if(cachedWarningState == AccountStatus::WitnessPending)
                            {
                                setIndex = WitnessDialogStates::PENDING;
                            }
                        }
                        else
                        {
                            if (pactiveWallet->GetBalance(forAccount, true, true) == pactiveWallet->GetBalance(forAccount, false, true))
                                stateEmptyWitnessButton = true;
                            else
                                stateWithdrawEarningsButton = true;
                            stateUnitButton = true;
                            setIndex = WitnessDialogStates::STATISTICS;
                            plotGraphForAccount(forAccount, nTotalNetworkWeight);
                        }
                    }
                }
            }
        }
    }

    ui->witnessDialogStackedWidget->setCurrentIndex(setIndex);
    ui->emptyWitnessButton->setVisible(stateEmptyWitnessButton);
    ui->emptyWitnessButton2->setVisible(stateEmptyWitnessButton2);
    ui->withdrawEarningsButton->setVisible(stateWithdrawEarningsButton);
    ui->withdrawEarningsButton2->setVisible(stateWithdrawEarningsButton2);
    ui->fundWitnessButton->setVisible(stateFundWitnessButton);
    ui->renewWitnessButton->setVisible(stateRenewWitnessButton);
    ui->unitButton->setVisible(stateUnitButton);
    ui->viewWitnessGraphButton->setVisible(stateViewWitnessGraphButton);
}

void WitnessDialog::numBlocksChanged(int,QDateTime,double,bool)
{
    //Don't update for every single block change if we are on testnet and have them streaming in at a super fast speed.
    static uint64_t nUpdateTimerStart = 0;
    if (IsArgSet("-testnet"))
    {
        // Only update at most once every 20 seconds (prevent this from being a bottleneck on testnet)
        if (nUpdateTimerStart == 0 || (GetTimeMillis() - nUpdateTimerStart > (IsArgSet("-testnet") ? 20000 : 10000)))
        {
            nUpdateTimerStart = GetTimeMillis();
        }
        else
        {
            return;
        }
    }

    //Update account states to the latest block
    //fixme: (2.1) - Some of this is redundant and can possibly be removed; as we set a lot of therse states now from within ::AddToWalletIfInvolvingMe
    //However - we would have to update account serialization to serialist the warning state and/or test some things before removing this.
    {
        LOCK(cs_main); // Required for ReadBlockFromDisk as well as account access.
        nUpdateTimerStart = GetTimeMillis();

        std::unique_ptr<TransactionFilterProxy> filter;
        filter.reset(new TransactionFilterProxy);
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::AscendingOrder);

        CGetWitnessInfo witnessInfo;
        CBlock block;
        //fixme: (2.0) Error handling.
        if (!ReadBlockFromDisk(block, chainActive.Tip(), Params().GetConsensus()))
            return;
        if (!GetWitnessInfo(chainActive, Params(), nullptr, chainActive.Tip()->pprev, block, witnessInfo, chainActive.Tip()->nHeight))
            return;

        LOCK(pactiveWallet->cs_wallet);

        for ( const auto& accountPair : pactiveWallet->mapAccounts )
        {
            AccountStatus prevState = accountPair.second->GetWarningState();
            AccountStatus newState = AccountStatus::Default;
            bool bAnyAreMine = false;
            for (const auto& witCoin : witnessInfo.witnessSelectionPoolUnfiltered)
            {
                if (IsMine(*accountPair.second, witCoin.coin.out))
                {
                    bAnyAreMine = true;
                    CTxOutPoW2Witness details;
                    GetPow2WitnessOutput(witCoin.coin.out, details);
                    if (details.lockUntilBlock < (unsigned int)chainActive.Tip()->nHeight)
                    {
                        newState = AccountStatus::WitnessEnded;
                    }
                    else if (witnessHasExpired(witCoin.nAge, witCoin.nWeight, witnessInfo.nTotalWeight))
                    {
                        newState = AccountStatus::WitnessExpired;

                        // Due to lock changing cached balance for certain transactions will now be invalidated.
                        // Technically we should find those specific transactions and invalidate them, but it's simpler to just invalidate them all.
                        if (prevState != AccountStatus::WitnessExpired)
                        {
                            for(auto& wtxIter : pactiveWallet->mapWallet)
                            {
                                wtxIter.second.clearAllCaches();
                            }
                        }
                    }
                }
            }
            if (!bAnyAreMine)
            {
                newState = AccountStatus::WitnessEmpty;
                filter->setAccountFilter(accountPair.second);
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
            if (newState != prevState)
            {
                accountPair.second->SetWarningState(newState);
                static_cast<const CGuldenWallet*>(pactiveWallet)->NotifyAccountWarningChanged(pactiveWallet, accountPair.second);
            }
        }
    }

    // Update graph and witness info for current account to latest block.
    update();
}

void WitnessDialog::setClientModel(ClientModel* clientModel_)
{
    clientModel = clientModel_;
    if (clientModel)
    {
        connect(clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(numBlocksChanged(int,QDateTime,double,bool)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
    }
}



WitnessSortFilterProxyModel::WitnessSortFilterProxyModel(QObject *parent)
: QSortFilterProxyModel(parent)
{
}

WitnessSortFilterProxyModel::~WitnessSortFilterProxyModel()
{
}

bool WitnessSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);

    // Must be a 'normal' type account.
    std::string sState = sourceModel()->data(index0, AccountTableModel::StateRole).toString().toStdString();
    if (sState != GetAccountStateString(AccountState::Normal))
        return false;

    // Must not be the current account.
    QString sActive = sourceModel()->data(index0, AccountTableModel::ActiveAccountRole).toString();
    if (sActive != AccountTableModel::Inactive)
        return false;

    // Must not be a witness account itself.
    std::string sSubType = sourceModel()->data(index0, AccountTableModel::TypeRole).toString().toStdString();
    if (sSubType == GetAccountTypeString(AccountType::PoW2Witness))
        return false;

    // Must have sufficient balance to fund the operation.
    uint64_t nCompare = sourceModel()->data(index0, AccountTableModel::AvailableBalanceRole).toLongLong();
    if (nCompare < nAmount)
        return false;
    return true;
}


void WitnessDialog::setModel(WalletModel* _model)
{
    this->model = _model;

    if(_model && _model->getOptionsModel())
    {
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::AscendingOrder);

        {
            WitnessSortFilterProxyModel* proxyFilterByBalanceFund = new WitnessSortFilterProxyModel(this);
            proxyFilterByBalanceFund->setSourceModel(model->getAccountTableModel());
            proxyFilterByBalanceFund->setDynamicSortFilter(true);
            proxyFilterByBalanceFund->setAmount(nMinimumWitnessAmount * COIN);

            QSortFilterProxyModel* proxyFilterByBalanceFundSorted = new QSortFilterProxyModel(this);
            proxyFilterByBalanceFundSorted->setSourceModel(proxyFilterByBalanceFund);
            proxyFilterByBalanceFundSorted->setDynamicSortFilter(true);
            proxyFilterByBalanceFundSorted->setSortCaseSensitivity(Qt::CaseInsensitive);
            proxyFilterByBalanceFundSorted->setFilterFixedString("");
            proxyFilterByBalanceFundSorted->setFilterCaseSensitivity(Qt::CaseInsensitive);
            proxyFilterByBalanceFundSorted->setFilterKeyColumn(AccountTableModel::ColumnIndex::Label);
            proxyFilterByBalanceFundSorted->setSortRole(Qt::DisplayRole);
            proxyFilterByBalanceFundSorted->sort(0);

            WitnessSortFilterProxyModel* proxyFilterByBalanceRenew = new WitnessSortFilterProxyModel(this);
            proxyFilterByBalanceRenew->setSourceModel(model->getAccountTableModel());
            proxyFilterByBalanceRenew->setDynamicSortFilter(true);
            proxyFilterByBalanceRenew->setAmount(1 * COIN);

            QSortFilterProxyModel* proxyFilterByBalanceRenewSorted = new QSortFilterProxyModel(this);
            proxyFilterByBalanceRenewSorted->setSourceModel(proxyFilterByBalanceRenew);
            proxyFilterByBalanceRenewSorted->setDynamicSortFilter(true);
            proxyFilterByBalanceRenewSorted->setSortCaseSensitivity(Qt::CaseInsensitive);
            proxyFilterByBalanceRenewSorted->setFilterFixedString("");
            proxyFilterByBalanceRenewSorted->setFilterCaseSensitivity(Qt::CaseInsensitive);
            proxyFilterByBalanceRenewSorted->setFilterKeyColumn(AccountTableModel::ColumnIndex::Label);
            proxyFilterByBalanceRenewSorted->setSortRole(Qt::DisplayRole);
            proxyFilterByBalanceRenewSorted->sort(0);

            ui->fundWitnessAccountTableView->setModel(proxyFilterByBalanceFundSorted);
            ui->renewWitnessAccountTableView->setModel(proxyFilterByBalanceRenewSorted);

            connect( _model, SIGNAL( accountAdded(CAccount*) ), this , SLOT( update() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        }
    }
}
