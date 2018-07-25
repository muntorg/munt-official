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
#include "wallet/wallet.h"

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

#include "Gulden/util.h"
#include "GuldenGUI.h"

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

enum WitnessDialogStates {EMPTY, STATISTICS, EXPIRED, PENDING, FINAL};

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
    ui->fundWitnessButton->setCursor(Qt::PointingHandCursor);
    ui->compoundEarningsCheckBox->setCursor(Qt::PointingHandCursor);

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

    // Hide all buttons by default
    ui->emptyWitnessButton->setVisible(false);
    ui->emptyWitnessButton2->setVisible(false);
    ui->withdrawEarningsButton->setVisible(false);
    ui->withdrawEarningsButton2->setVisible(false);
    ui->fundWitnessButton->setVisible(false);
    ui->renewWitnessButton->setVisible(false);
    ui->unitButton->setVisible(false);
    ui->viewWitnessGraphButton->setVisible(false);

    connect(ui->unitButton, SIGNAL(clicked()), this, SLOT(unitButtonClicked()));
    connect(ui->viewWitnessGraphButton, SIGNAL(clicked()), this, SLOT(viewWitnessInfoClicked()));
    connect(ui->emptyWitnessButton, SIGNAL(clicked()), this, SLOT(emptyWitnessClicked()));
    connect(ui->emptyWitnessButton2, SIGNAL(clicked()), this, SLOT(emptyWitnessClicked()));
    connect(ui->withdrawEarningsButton, SIGNAL(clicked()), this, SLOT(emptyWitnessClicked()));
    connect(ui->withdrawEarningsButton2, SIGNAL(clicked()), this, SLOT(emptyWitnessClicked()));
    connect(ui->fundWitnessButton, SIGNAL(clicked()), this, SLOT(fundWitnessClicked()));
    connect(ui->renewWitnessButton, SIGNAL(clicked()), this, SLOT(renewWitnessClicked()));
    connect(ui->compoundEarningsCheckBox, SIGNAL(clicked()), this, SLOT(compoundEarningsCheckboxClicked()));
    connect(unitBlocksAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Blocks); } );
    connect(unitDaysAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Days); } );
    connect(unitWeeksAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Weeks); } );
    connect(unitMonthsAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Months); } );
}

WitnessDialog::~WitnessDialog()
{
    LogPrint(BCLog::QT, "WitnessDialog::~WitnessDialog\n");

    delete ui;
}

void WitnessDialog::viewWitnessInfoClicked()
{
    LogPrint(BCLog::QT, "WitnessDialog::viewWitnessInfoClicked\n");

    ui->unitButton->setVisible(true);
    ui->viewWitnessGraphButton->setVisible(false);
    ui->witnessDialogStackedWidget->setCurrentIndex(WitnessDialogStates::STATISTICS);
}

void WitnessDialog::emptyWitnessClicked()
{
    LogPrint(BCLog::QT, "WitnessDialog::emptyWitnessClicked\n");

    Q_EMIT requestEmptyWitness();
}

void WitnessDialog::fundWitnessClicked()
{
    LogPrint(BCLog::QT, "WitnessDialog::fundWitnessClicked\n");

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
    LogPrint(BCLog::QT, "WitnessDialog::renewWitnessClicked\n");

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

void WitnessDialog::compoundEarningsCheckboxClicked()
{
    LogPrint(BCLog::QT, "WitnessDialog::compundEarningsCheckboxClicked\n");

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
    LogPrint(BCLog::QT, "WitnessDialog::unitButtonClicked\n");

    unitSelectionMenu->exec(ui->unitButton->mapToGlobal(QPoint(0,0)));
}

void WitnessDialog::updateUnit(int nNewUnit_)
{
    LogPrint(BCLog::QT, "WitnessDialog::updateUnit\n");

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
        fDaysModified = (nX - nOriginBlock)/576.0;
    }

    //fixme: (2.1) These various week/month calculations are all very imprecise; they should probably be based on an actual calendar instead.
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

void WitnessDialog::GetWitnessInfoForAccount(CAccount* forAccount, WitnessInfoForAccount& infoForAccount)
{
    std::map<double, CAmount> pointMapForecast; pointMapForecast[0] = 0;
    std::map<double, CAmount> pointMapGenerated;

    CTxOutPoW2Witness witnessDetails;

    // fixme: (2.1) Make this work for multiple 'origin' blocks.
    // Iterate the transaction history and extract all 'origin' block details.
    // Also extract details for every witness reward we have received.
    filter->setAccountFilter(forAccount);
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
                //fixme (2.1) - (multiple witnesses in single account etc.)
                if (infoForAccount.nOriginBlock == 0)
                {
                    infoForAccount.originDate = filter->data(index, TransactionTableModel::DateRole).toDateTime();
                    infoForAccount.nOriginBlock = filter->data(index, TransactionTableModel::TxBlockHeightRole).toInt();

                    // We take the network weight 100 blocks ahead to give a chance for our own weight to filter into things (and also if e.g. the first time witnessing activated - testnet - then weight will only climb once other people also join)
                    CBlockIndex* sampleWeightIndex = chainActive[infoForAccount.nOriginBlock+100 > (uint64_t)chainActive.Tip()->nHeight ? infoForAccount.nOriginBlock : infoForAccount.nOriginBlock+100];
                    int64_t nUnused1;
                    if (!GetPow2NetworkWeight(sampleWeightIndex, Params(), nUnused1, infoForAccount.nOriginNetworkWeight, chainActive))
                    {
                        std::string strErrorMessage = "Error in witness dialog, failed to get weight for account";
                        CAlert::Notify(strErrorMessage, true, true);
                        LogPrintf("%s", strErrorMessage.c_str());
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
                        //fixme: (2.1) Handle multiple in one tx. Handle regular transactions that may have somehow been sent to the account.
                        if (IsMine(*forAccount, walletTxIter->second.tx->vout[i]))
                        {
                            if (GetPow2WitnessOutput(walletTxIter->second.tx->vout[i], witnessDetails))
                            {
                                uint64_t nUnused1, nUnused2;
                                infoForAccount.nOriginLength = GetPoW2LockLengthInBlocksFromOutput(walletTxIter->second.tx->vout[i], infoForAccount.nOriginBlock, nUnused1, nUnused2);
                                infoForAccount.nOriginWeight = GetPoW2RawWeightForAmount(filter->data(index, TransactionTableModel::AmountRole).toLongLong(), infoForAccount.nOriginLength);
                                break;
                            }
                        }
                    }
                }
            }
            else if (nType == TransactionRecord::GeneratedWitness)
            {
                int nX = filter->data(index, TransactionTableModel::TxBlockHeightRole).toInt();
                if (nX > 0)
                {
                    infoForAccount.lastEarningsDate = filter->data(index, TransactionTableModel::DateRole).toDateTime();
                    uint64_t nY = filter->data(index, TransactionTableModel::AmountRole).toLongLong()/COIN;
                    infoForAccount.nEarningsToDate += nY;
                    uint64_t nDays = infoForAccount.originDate.daysTo(infoForAccount.lastEarningsDate);
                    AddPointToMapWithAdjustedTimePeriod(pointMapGenerated, infoForAccount.nOriginBlock, nX, nY, nDays, infoForAccount.scale, true);
                }
            }
        }
    }

    QDateTime tipTime = QDateTime::fromTime_t(chainActive.Tip()->GetBlockTime());
    // One last datapoint for 'current' block.
    if (!pointMapGenerated.empty())
    {
        uint64_t nY = pointMapGenerated.rbegin()->second;
        uint64_t nX = chainActive.Tip()->nHeight;
        uint64_t nDays = infoForAccount.originDate.daysTo(tipTime);
        pointMapGenerated.erase(--pointMapGenerated.end());
        AddPointToMapWithAdjustedTimePeriod(pointMapGenerated, infoForAccount.nOriginBlock, nX, nY, nDays, infoForAccount.scale, false);
    }

    // Using the origin block details gathered from previous loop, generate the points for a 'forecast' of how much the account should earn over its entire existence.
    infoForAccount.nWitnessLength = infoForAccount.nOriginLength;
    if (infoForAccount.nOriginNetworkWeight == 0)
        infoForAccount.nOriginNetworkWeight = gStartingWitnessNetworkWeightEstimate;
    uint64_t nEstimatedWitnessBlockPeriodOrigin = estimatedWitnessBlockPeriod(infoForAccount.nOriginWeight, infoForAccount.nOriginNetworkWeight);
    pointMapForecast[0] = 0;
    for (unsigned int i = nEstimatedWitnessBlockPeriodOrigin; i < infoForAccount.nWitnessLength; i += nEstimatedWitnessBlockPeriodOrigin)
    {
        unsigned int nX = i;
        uint64_t nDays = infoForAccount.originDate.daysTo(tipTime.addSecs(i*Params().GetConsensus().nPowTargetSpacing));
        AddPointToMapWithAdjustedTimePeriod(pointMapForecast, 0, nX, 20, nDays, infoForAccount.scale, true);
    }

    // Populate the 'expected earnings' curve
    for (const auto& pointIter : pointMapForecast)
    {
        infoForAccount.nTotal1 += pointIter.second;
        infoForAccount.nXForecast = pointIter.first;
        infoForAccount.forecastedPoints << QPointF(infoForAccount.nXForecast, infoForAccount.nTotal1);
    }

    // Populate the 'actual earnings' curve.
    int nXGenerated = 0;
    for (const auto& pointIter : pointMapGenerated)
    {
        infoForAccount.nTotal2 += pointIter.second;
        nXGenerated = pointIter.first;
        infoForAccount.generatedPoints << QPointF(pointIter.first, infoForAccount.nTotal2);
    }
    if (infoForAccount.generatedPoints.size() > 1)
    {
        (infoForAccount.generatedPoints.rbegin())->setY(infoForAccount.nEarningsToDate);
    }

    //fixme: (2.1) This is a bit broken - use nOurWeight etc.
    // Fill in the remaining time on the 'actual earnings' curve with a forecast.
    int nXGeneratedForecast = 0;
    if (infoForAccount.generatedPoints.size() > 0)
    {
        infoForAccount.generatedPointsForecast << infoForAccount.generatedPoints.back();
        for (const auto& pointIter : pointMapForecast)
        {
            nXGeneratedForecast = pointIter.first;
            if (nXGeneratedForecast > nXGenerated)
            {
                infoForAccount.nTotal2 += pointIter.second;
                infoForAccount.generatedPointsForecast << QPointF(nXGeneratedForecast, infoForAccount.nTotal2);
            }
        }
    }

    infoForAccount.nExpectedWitnessBlockPeriod = expectedWitnessBlockPeriod(infoForAccount.nOurWeight, infoForAccount.nTotalNetworkWeightTip);
    infoForAccount.nEstimatedWitnessBlockPeriod = estimatedWitnessBlockPeriod(infoForAccount.nOurWeight, infoForAccount.nTotalNetworkWeightTip);
    infoForAccount.nLockBlocksRemaining = GetPoW2RemainingLockLengthInBlocks(witnessDetails.lockUntilBlock, chainActive.Tip()->nHeight);

    return;
}

void WitnessDialog::plotGraphForAccount(CAccount* forAccount, uint64_t nOurWeight, uint64_t nTotalNetworkWeightTip)
{
    LogPrint(BCLog::QT, "WitnessDialog::plotGraphForAccount\n");

    DO_BENCHMARK("WIT: WitnessDialog::plotGraphForAccount", BCLog::BENCH|BCLog::WITNESS);

    if(!filter || !model)
        return;

    // Calculate all the account info
    WitnessInfoForAccount witnessInfoForAccount;
    {
        witnessInfoForAccount.nOurWeight = nOurWeight;
        witnessInfoForAccount.nTotalNetworkWeightTip = nTotalNetworkWeightTip;
        witnessInfoForAccount.scale = (GraphScale)model->getOptionsModel()->guldenSettings->getWitnessGraphScale();
        GetWitnessInfoForAccount(forAccount, witnessInfoForAccount);
    }

    // Populate graph with info
    {
        expectedEarningsCurve->setSamples( witnessInfoForAccount.forecastedPoints );
        currentEarningsCurveShadow->setSamples( witnessInfoForAccount.generatedPoints );
        currentEarningsCurve->setSamples( witnessInfoForAccount.generatedPoints );
        currentEarningsCurveForecastShadow->setSamples( witnessInfoForAccount.generatedPointsForecast );
        currentEarningsCurveForecast->setSamples( witnessInfoForAccount.generatedPointsForecast );
        ui->witnessEarningsPlot->setAxisScale( QwtPlot::xBottom, 0, witnessInfoForAccount.nXForecast);
        ui->witnessEarningsPlot->setAxisScale( QwtPlot::yLeft, 0, std::max(witnessInfoForAccount.nTotal1, witnessInfoForAccount.nTotal2));
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
        QString labelWeightValue = lastEarningsDateLabel;
        QString lockedUntilValue = lastEarningsDateLabel;
        QString lockedFromValue = lastEarningsDateLabel;

        if (witnessInfoForAccount.nOurWeight > 0)
            labelWeightValue = QString::number(witnessInfoForAccount.nOurWeight);
        if (!witnessInfoForAccount.originDate.isNull())
            lockedFromValue = witnessInfoForAccount.originDate.toString("dd/MM/yy hh:mm");
        if (chainActive.Tip())
        {
            QDateTime lockedUntilDate;
            lockedUntilDate.setTime_t(chainActive.Tip()->nTime);
            lockedUntilDate = lockedUntilDate.addSecs(witnessInfoForAccount.nLockBlocksRemaining*150);
            lockedUntilValue = lockedUntilDate.toString("dd/MM/yy hh:mm");
        }

        if (!witnessInfoForAccount.lastEarningsDate.isNull())
            lastEarningsDateLabel = witnessInfoForAccount.lastEarningsDate.toString("dd/MM/yy hh:mm");
        if (witnessInfoForAccount.generatedPoints.size() != 0)
            earningsToDateLabel = QString::number(witnessInfoForAccount.nEarningsToDate);
        if (witnessInfoForAccount.nTotalNetworkWeightTip > 0)
            networkWeightLabel = QString::number(witnessInfoForAccount.nTotalNetworkWeightTip);

        //fixme: (2.1) The below uses "dumb" conversion - i.e. it assumes 30 days in a month, it doesn't look at how many hours in current day etc.
        //Ideally this should be improved.
        //Note if we do improve it we may want to keep the "dumb" behaviour for testnet.
        {
            QString formatStr;
            double divideBy=1;
            int roundTo = 2;
            switch (witnessInfoForAccount.scale)
            {
                case GraphScale::Blocks:
                    formatStr = tr("%1 blocks");
                    divideBy = 1;
                    roundTo = 0;
                    break;
                case GraphScale::Days:
                    formatStr = tr("%1 days");
                    divideBy = 576;
                    break;
                case GraphScale::Weeks:
                    formatStr = tr("%1 weeks");
                    divideBy = 576*7;
                    break;
                case GraphScale::Months:
                    formatStr = tr("%1 months");
                    divideBy = 576*30;
                    break;
            }
            if (witnessInfoForAccount.nWitnessLength > 0)
                lockDurationLabel = formatStr.arg(QString::number(witnessInfoForAccount.nWitnessLength/divideBy, 'f', roundTo));
            if (witnessInfoForAccount.nExpectedWitnessBlockPeriod > 0)
                expectedEarningsDurationLabel = formatStr.arg(QString::number(witnessInfoForAccount.nExpectedWitnessBlockPeriod/divideBy, 'f', roundTo));
            if (witnessInfoForAccount.nEstimatedWitnessBlockPeriod > 0)
                estimatedEarningsDurationLabel = formatStr.arg(QString::number(witnessInfoForAccount.nEstimatedWitnessBlockPeriod/divideBy, 'f', roundTo));
            if (witnessInfoForAccount.nLockBlocksRemaining > 0)
                lockTimeRemainingLabel = formatStr.arg(QString::number(witnessInfoForAccount.nLockBlocksRemaining/divideBy, 'f', roundTo));
        }

        ui->labelLastEarningsDateValue->setText(lastEarningsDateLabel);
        ui->labelWitnessEarningsValue->setText(earningsToDateLabel);
        ui->labelNetworkWeightValue->setText(networkWeightLabel);
        ui->labelLockDurationValue->setText(lockDurationLabel);
        ui->labelExpectedEarningsDurationValue->setText(expectedEarningsDurationLabel);
        ui->labelEstimatedEarningsDurationValue->setText(estimatedEarningsDurationLabel);
        ui->labelLockTimeRemainingValue->setText(lockTimeRemainingLabel);
        ui->labelWeightValue->setText(labelWeightValue);
        ui->labelLockedFromValue->setText(lockedFromValue);
        ui->labelLockedUntilValue->setText(lockedUntilValue);
    }
}

void WitnessDialog::update()
{
    LogPrint(BCLog::QT, "WitnessDialog::update\n");
    doUpdate(false);
}

void WitnessDialog::doUpdate(bool forceUpdate)
{
    // rate limit this expensive UI update when chain tip is still far from known height
    int heightRemaining = clientModel->cachedProbableHeight - chainActive.Height();
    if (!forceUpdate && heightRemaining > 10 && heightRemaining % 100 != 0)
        return;

    LogPrint(BCLog::QT, "WitnessDialog::doUpdate\n");

    DO_BENCHMARK("WIT: WitnessDialog::update", BCLog::BENCH|BCLog::WITNESS);

    // If SegSig is enabled then allow possibility of witness compounding.
    ui->compoundEarningsCheckBox->setVisible(IsSegSigEnabled(chainActive.TipPrev()));

    static WitnessDialogStates cachedIndex = WitnessDialogStates::EMPTY;
    static CAccount* cachedIndexForAccount = nullptr;

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
            static CAccount* cachedForAccount = nullptr;
            static uint256 cachedHashTip;
            static AccountStatus cachedWarningState = AccountStatus::Default;
            if ( !forAccount->IsPoW2Witness() )
            {
                cachedForAccount = forAccount;
            }
            else
            {
                ui->compoundEarningsCheckBox->setChecked((forAccount->getCompounding() != 0));

                uint64_t nTotalNetworkWeight = 0;
                uint64_t nOurWeight = 0;
                bool bAnyExpired = false;
                bool bAnyFinished = false;
                bool bAnyAreMine = false;
                if (chainActive.Tip() && chainActive.Tip()->pprev)
                {
                    // Prevent same update being called repeatedly for same account/tip (can happen in some event sequences)
                    if (!forceUpdate && cachedForAccount == forAccount && cachedHashTip == chainActive.Tip()->GetBlockHashPoW2() && cachedWarningState == forAccount->GetWarningState())
                        return;
                    cachedForAccount = forAccount;
                    cachedHashTip = chainActive.Tip()->GetBlockHashPoW2();
                    cachedWarningState = forAccount->GetWarningState();

                    CGetWitnessInfo witnessInfo;
                    CBlock block;
                    {
                        LOCK(cs_main); // Required for ReadBlockFromDisk as well as GetWitnessInfo.
                        if (!ReadBlockFromDisk(block, chainActive.Tip(), Params()))
                        {
                            std::string strErrorMessage = "Error in witness dialog, failed to read block from disk";
                            CAlert::Notify(strErrorMessage, true, true);
                            LogPrintf("%s", strErrorMessage.c_str());
                            return;
                        }
                        if (!GetWitnessInfo(chainActive, Params(), nullptr, chainActive.Tip()->pprev, block, witnessInfo, chainActive.Tip()->nHeight))
                        {
                            std::string strErrorMessage = "Error in witness dialog, failed to retrieve witness info";
                            CAlert::Notify(strErrorMessage, true, true);
                            LogPrintf("%s", strErrorMessage.c_str());
                            return;
                        }
                        nTotalNetworkWeight = witnessInfo.nTotalWeight;
                    }
                    for (const auto& witCoin : witnessInfo.witnessSelectionPoolUnfiltered)
                    {
                        if (IsMine(*forAccount, witCoin.coin.out))
                        {
                            bAnyAreMine = true;
                            CTxOutPoW2Witness witnessDetails;
                            if ( (GetPow2WitnessOutput(witCoin.coin.out, witnessDetails)) && !IsPoW2WitnessLocked(witnessDetails, chainActive.Tip()->nHeight) )
                            {
                                bAnyFinished = true;
                            }
                            else
                            {
                                if (witnessHasExpired(witCoin.nAge, witCoin.nWeight, witnessInfo.nTotalWeight))
                                {
                                    bAnyExpired = true;
                                }
                                nOurWeight += witCoin.nWeight;
                            }
                        }
                    }

                    //Witness only witness account skips all the fancy states for now and just always shows the statistics page
                    if (forAccount->m_Type == WitnessOnlyWitnessAccount)
                    {
                        setIndex = WitnessDialogStates::STATISTICS;
                        stateWithdrawEarningsButton = stateUnitButton = true;
                        stateEmptyWitnessButton = stateEmptyWitnessButton2 = false;
                        stateWithdrawEarningsButton2 = stateFundWitnessButton = false;
                        stateRenewWitnessButton = stateViewWitnessGraphButton = false;
                        plotGraphForAccount(forAccount, nOurWeight, nTotalNetworkWeight);
                    }
                    else
                    {
                        // We have to check for immature balance as well - otherwise accounts that have just witnessed get incorrectly marked as "empty".
                        if ((pactiveWallet->GetBalance(forAccount, true, true) > 0) || (pactiveWallet->GetImmatureBalance(forAccount, true, true) > 0) || (pactiveWallet->GetUnconfirmedBalance(forAccount, true, true) > 0))
                        {
                            stateFundWitnessButton = false;
                            if (bAnyFinished)
                            {
                                stateRenewWitnessButton = false;
                                stateEmptyWitnessButton = true;
                                stateEmptyWitnessButton2 = false;
                                stateWithdrawEarningsButton = stateWithdrawEarningsButton2 = false;
                                setIndex = WitnessDialogStates::STATISTICS;
                            }
                            else if (bAnyExpired || !bAnyAreMine)
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
                                    // If the full account balance is spendable than show an "empty account" button otherwise an "empty earnings" button.
                                    if ((pactiveWallet->GetLockedBalance(forAccount, true) == 0) && (pactiveWallet->GetUnconfirmedBalance(forAccount, false, true) == 0) && (pactiveWallet->GetImmatureBalance(forAccount, false, true) == 0))
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
                                    plotGraphForAccount(forAccount, nOurWeight, nTotalNetworkWeight);
                                }
                                // Second check for pending is required because first check might fail (when calling update during funding an account for first time) - while second check might not be right in some cases either so we rather have both.
                                else if(cachedWarningState == AccountStatus::WitnessPending)
                                {
                                    setIndex = WitnessDialogStates::PENDING;
                                }
                            }
                            else
                            {
                                if ((pactiveWallet->GetLockedBalance(forAccount, true) == 0) && (pactiveWallet->GetUnconfirmedBalance(forAccount, false, true) == 0) && (pactiveWallet->GetImmatureBalance(forAccount, false, true) == 0))
                                    stateEmptyWitnessButton = true;
                                else
                                    stateWithdrawEarningsButton = true;
                                stateUnitButton = true;
                                setIndex = WitnessDialogStates::STATISTICS;
                                plotGraphForAccount(forAccount, nOurWeight, nTotalNetworkWeight);
                            }
                        }
                        else
                        {
                            if (bAnyFinished)
                            {
                                stateRenewWitnessButton = false;
                                stateEmptyWitnessButton = false;
                                stateEmptyWitnessButton2 = false;
                                stateWithdrawEarningsButton = stateWithdrawEarningsButton2 = false;
                                setIndex = WitnessDialogStates::STATISTICS;
                            }
                            else
                            {
                                filter->setAccountFilter(model->getActiveAccount());
                                int rows = filter->rowCount();
                                bool bAnyConfirmed = false;
                                for (int row = 0; row < rows; ++row)
                                {
                                    QModelIndex index = filter->index(row, 0);

                                    int nStatus = filter->data(index, TransactionTableModel::StatusRole).toInt();
                                    if (nStatus == TransactionStatus::Status::Confirmed)
                                    {
                                        bAnyConfirmed = true;
                                        break;
                                    }
                                }
                                if (bAnyConfirmed)
                                {
                                    setIndex = WitnessDialogStates::FINAL;
                                    plotGraphForAccount(forAccount, nOurWeight, nTotalNetworkWeight);
                                    stateFundWitnessButton = false;
                                    stateRenewWitnessButton = false;
                                    stateRenewWitnessButton = false;
                                    stateEmptyWitnessButton = stateEmptyWitnessButton2 = false;
                                    stateWithdrawEarningsButton = stateWithdrawEarningsButton2 = false;
                                    stateEmptyWitnessButton2 = stateWithdrawEarningsButton2 = false;
                                    stateViewWitnessGraphButton = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Stop view jumping back and forth if user has pushed "view statistics" and a new block comes in.
        if (cachedIndexForAccount && cachedIndexForAccount == forAccount && cachedIndex == setIndex)
        {
            setIndex = (WitnessDialogStates)ui->witnessDialogStackedWidget->currentIndex();
            if (setIndex == WitnessDialogStates::STATISTICS)
            {
                // Prevent these two buttons from flipping as well.
                stateUnitButton = true;
                stateViewWitnessGraphButton = false;
            }
        }
        else
        {
            cachedIndex = setIndex;
            cachedIndexForAccount = forAccount;
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

void WitnessDialog::updateAccountIndicators()
{
    LogPrint(BCLog::QT, "WitnessDialog::updateAccountIndicators\n");

    if(!filter || !model)
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
    //fixme: (2.1) - Some of this is redundant and can possibly be removed; as we set a lot of therse states now from within ::AddToWalletIfInvolvingMe
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
                        else if (witnessHasExpired(witCoin.nAge, witCoin.nWeight, witnessInfo.nTotalWeight))
                        {
                            newState = AccountStatus::WitnessExpired;

                            //fixme: (2.1) I think this is only needed for (ended) accounts - and not expired ones? Double check
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
                    static_cast<const CGuldenWallet*>(pactiveWallet)->NotifyAccountWarningChanged(pactiveWallet, account);
                }
            }
        }
    }
}

void WitnessDialog::numBlocksChanged(int,QDateTime,double,bool)
{
    LogPrint(BCLog::QT, "WitnessDialog::numBlocksChanged\n");

    //Update account state indicators for the latest block
    updateAccountIndicators();

    // Update graph and witness info for current account to latest block.
    doUpdate(false);
}

void WitnessDialog::setClientModel(ClientModel* clientModel_)
{
    LogPrint(BCLog::QT, "WitnessDialog::setClientModel\n");

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
    if (sSubType == GetAccountTypeString(AccountType::WitnessOnlyWitnessAccount))
        return false;

    // Must have sufficient balance to fund the operation.
    uint64_t nCompare = sourceModel()->data(index0, AccountTableModel::AvailableBalanceRole).toLongLong();
    if (nCompare < nAmount)
        return false;
    return true;
}


void WitnessDialog::setModel(WalletModel* _model)
{
    LogPrint(BCLog::QT, "WitnessDialog::setModel\n");

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
            WitnessSortFilterProxyModel* proxyFilterByBalanceFund = new WitnessSortFilterProxyModel(this);
            proxyFilterByBalanceFund->setSourceModel(model->getAccountTableModel());
            proxyFilterByBalanceFund->setDynamicSortFilter(true);
            proxyFilterByBalanceFund->setAmount((gMinimumWitnessAmount*COIN));

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

            connect( _model, SIGNAL( activeAccountChanged(CAccount*) ), this , SLOT( update() ), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection) );
        }
    }
    else if(model)
    {
        filter.reset(nullptr);
        disconnect( model, SIGNAL( activeAccountChanged(CAccount*) ), this , SLOT( update() ));
        model = nullptr;
    }
}
