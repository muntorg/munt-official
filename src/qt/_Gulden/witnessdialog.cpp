// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "witnessdialog.h"
#include <qt/_Gulden/forms/ui_witnessdialog.h>
#include <Gulden/guldenapplication.h>
#include "GuldenGUI.h"
#include <wallet/wallet.h>
#include "ui_interface.h"
#include <Gulden/mnemonic.h>
#include <vector>
#include "random.h"
#include "walletmodel.h"
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

#include "transactiontablemodel.h"
#include "transactionrecord.h"
#include "validation.h"

#include "Gulden/util.h"
#include "GuldenGUI.h"

#include <QMenu>

WitnessDialog::WitnessDialog(const PlatformStyle* _platformStyle, QWidget* parent)
: QFrame( parent )
, ui( new Ui::WitnessDialog )
, platformStyle( _platformStyle )
, clientModel(NULL)
, model( NULL )
{
    ui->setupUi(this);

    // Set correct cursor for all clickable UI elements
    ui->unitButton->setCursor( Qt::PointingHandCursor );
    ui->renewWitnessButton->setCursor( Qt::PointingHandCursor );
    ui->emptyWitnessButton->setCursor( Qt::PointingHandCursor );

    // Clear all labels by default
    ui->labelWeightValue->setText(tr("n/a"));
    ui->labelLockedFromValue->setText(tr("n/a"));
    ui->labelLockedUntilValue->setText(tr("n/a"));
    ui->labelLastEarningsDateValue->setText(tr("n/a"));
    ui->labelWitnessEarningsValue->setText(tr("n/a"));
    ui->labelLockDurationValue->setText(tr("n/a"));
    ui->labelExpectedEarningsDurationValue->setText(tr("n/a"));
    ui->labelLockTimeRemainingValue->setText(tr("n/a"));

    // White background for plot
    ui->witnessEarningsPlot->setStyleSheet("QwtPlotCanvas { background: white; }");

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

    // Setup curve colour/properties for earnings to date.
    {
        currentEarningsCurve = new QwtPlotCurve();
        currentEarningsCurve->setTitle( tr("Earnings to date") );
        currentEarningsCurve->setPen( QColor(ACCENT_COLOR_1), 1 );

        currentEarningsCurve->setBrush( QBrush(QColor("#c9e3ff")) );
        currentEarningsCurve->setBaseline(0);

        QwtSplineCurveFitter* fitter = new QwtSplineCurveFitter();
        fitter->setFitMode(QwtSplineCurveFitter::ParametricSpline);
        //fitter->setSplineSize(10);
        currentEarningsCurve->setCurveFitter(fitter);
        currentEarningsCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );

        currentEarningsCurve->attach( ui->witnessEarningsPlot );
    }

    // Setup curve colour/properties for future earnings (projected on top of earnings to date)
    {
        currentEarningsCurveForecast = new QwtPlotCurve();
        currentEarningsCurveForecast->setTitle( tr("Projected earnings") );
        currentEarningsCurveForecast->setPen( QColor(ACCENT_COLOR_1), 1, Qt::DashLine );

        currentEarningsCurveForecast->setBrush( QBrush(QColor("#c9e3ff")) );
        currentEarningsCurveForecast->setBaseline(0);

        QwtSplineCurveFitter* fitter = new QwtSplineCurveFitter();
        fitter->setFitMode(QwtSplineCurveFitter::ParametricSpline);
        //fitter->setSplineSize(10);
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
        expectedEarningsCurve->setZ(currentEarningsCurveForecast->z() + 1);

        QwtSplineCurveFitter* fitter = new QwtSplineCurveFitter();
        fitter->setFitMode(QwtSplineCurveFitter::ParametricSpline);
        //fitter->setSplineSize(10);
        expectedEarningsCurve->setCurveFitter(fitter);
        expectedEarningsCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );

        expectedEarningsCurve->attach( ui->witnessEarningsPlot );
    }


    QAction* unitBlocksAction = new QAction(tr("&Blocks"), this);
    QAction* unitDaysAction = new QAction(tr("&Days"), this);
    QAction* unitWeeksAction = new QAction(tr("&Weeks"), this);
    QAction* unitMonthsAction = new QAction(tr("&Months"), this);

    // Build context menu for unit selection button
    unitSelectionMenu = new QMenu(this);
    unitSelectionMenu->addAction(unitBlocksAction);
    unitSelectionMenu->addAction(unitDaysAction);
    unitSelectionMenu->addAction(unitWeeksAction);
    unitSelectionMenu->addAction(unitMonthsAction);

    connect(ui->unitButton, SIGNAL( clicked() ), this, SLOT( unitButtonClicked() ) );
    connect(unitBlocksAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Blocks); } );
    connect(unitDaysAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Days); } );
    connect(unitWeeksAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Weeks); } );
    connect(unitMonthsAction, &QAction::triggered, [this]() { updateUnit(GraphScale::Months); } );
}

WitnessDialog::~WitnessDialog()
{
    delete ui;
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

void WitnessDialog::plotGraphForAccount(CAccount* account)
{
    GraphScale scale = (GraphScale)model->getOptionsModel()->guldenSettings->getWitnessGraphScale();

    std::map<CAmount, CAmount> pointMapForecast; pointMapForecast[0] = 0;
    std::map<CAmount, CAmount> pointMapGenerated;

    CTxOutPoW2Witness witnessDetails;
    QDateTime lastEarningsDate;
    QDateTime originDate;
    uint64_t nOriginNetworkWeight;
    uint64_t nOriginBlock = 0;
    uint64_t nOriginWeight = 0;
    uint64_t nOriginLength = 0;

    // fixme: Make this work for multiple 'origin' blocks.
    // Iterate the transaction history and extract all 'origin' block details.
    // Also extract details for every witness reward we have received.
    filter->setAccountFilter(model->getActiveAccount());
    int rows = filter->rowCount();
    for (int row = 0; row < rows; ++row)
    {
        QModelIndex index = filter->index(row, 0);

        int nType = filter->data(index, TransactionTableModel::TypeRole).toInt();
        if (nType >= TransactionRecord::RecvWithAddress)
        {
            if (nOriginBlock == 0)
            {
                originDate = filter->data(index, TransactionTableModel::DateRole).toDateTime();
                nOriginBlock = filter->data(index, TransactionTableModel::TxBlockHeightRole).toInt();

                // We take the network weight 100 blocks ahead to give a chance for our own weight to filter into things (and also if e.g. the first time witnessing activated - testnet - then weight will only climb once other people also join)
                CBlockIndex* sampleWeightIndex = chainActive[nOriginBlock+100 > chainActive.Tip()->nHeight ? nOriginBlock : nOriginBlock+100];
                int64_t nUnused1, nUnused2;
                nOriginNetworkWeight =  GetPow2NetworkWeight(sampleWeightIndex, Params(), nUnused1, nUnused2, chainActive);
                pointMapGenerated[0] = 0;

                uint256 originHash;
                originHash.SetHex( filter->data(index, TransactionTableModel::TxHashRole).toString().toStdString());

                LOCK2(cs_main, pactiveWallet->cs_wallet);
                auto walletTxIter = pactiveWallet->mapWallet.find(originHash);
                if(walletTxIter == pactiveWallet->mapWallet.end())
                {
                    return;
                }

                for (int i=0; i<walletTxIter->second.tx->vout.size(); ++i)
                {
                    //fixme: Handle multiple in one tx. Check ismine. Handle none (regular transaction)
                    if (GetPow2WitnessOutput(walletTxIter->second.tx->vout[i], witnessDetails))
                    {
                        break;
                    }
                }
                nOriginLength = witnessDetails.lockUntilBlock - (witnessDetails.lockFromBlock > 0 ? witnessDetails.lockFromBlock : nOriginBlock);
                nOriginWeight = GetPoW2RawWeightForAmount(filter->data(index, TransactionTableModel::AmountRole).toLongLong(), nOriginLength);
            }
            //fixme (HIGH): changes in witness weight... (multiple witnesses in single account etc.)
        }
        else if (nType == TransactionRecord::GeneratedWitness)
        {
            int nX = filter->data(index, TransactionTableModel::TxBlockHeightRole).toInt();
            if (nX > 0)
            {
                lastEarningsDate = filter->data(index, TransactionTableModel::DateRole).toDateTime();
                switch (scale)
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
                            nX = originDate.daysTo(lastEarningsDate);
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
                            //fixme:
                            nX = originDate.daysTo(lastEarningsDate)/7;
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
                            //fixme:
                            nX = originDate.daysTo(lastEarningsDate)/30;
                        }
                        break;
                }

                pointMapGenerated[nX] += filter->data(index, TransactionTableModel::AmountRole).toLongLong()/COIN;
            }
        }
    }

    // Using the origin block details gathered from previous loop, generate the points for a 'forecast' of how much the account should earn over its entire existence.
    uint64_t nWitnessLength = nOriginLength;
    uint64_t nExpectedWitnessBlockPeriod = expectedWitnessBlockPeriod(nOriginWeight, nOriginNetworkWeight);
    for (unsigned int i = 0; i < nWitnessLength; i += nExpectedWitnessBlockPeriod)
    {
        unsigned int nX = i;
        switch (scale)
        {
            case GraphScale::Blocks:
                break;
            case GraphScale::Days:
                if (IsArgSet("-testnet"))
                {
                    nX /= 576;
                }
                else
                {
                    //fixme:
                }
                break;
            case GraphScale::Weeks:
                if (IsArgSet("-testnet"))
                {
                    nX /= 576;
                    nX /= 7;
                    ++nX;
                }
                else
                {
                    //fixme:
                }
                break;
            case GraphScale::Months:
                if (IsArgSet("-testnet"))
                {
                    nX /= 576;
                    nX /= 30;
                    ++nX;
                }
                else
                {
                    //fixme:
                }
                break;
        }
        pointMapForecast[nX] += 20;
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
    currentEarningsCurve->setSamples( generatedPoints );

    // Fill in the remaining time on the 'actual earnings' curve with a forecast.
    QPolygonF generatedPointsForecast;
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
    currentEarningsCurveForecast->setSamples( generatedPointsForecast );


    ui->witnessEarningsPlot->setAxisScale( QwtPlot::yLeft, 0, std::max(nTotal1, nTotal2));
    ui->witnessEarningsPlot->replot();


    // Update all the info labels with values calculated above.
    uint64_t nLockBlocksRemaining = witnessDetails.lockUntilBlock - chainActive.Tip()->nHeight;
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
        lockedUntilDate.addSecs(nLockBlocksRemaining*150);
        ui->labelLockedUntilValue->setText(lockedUntilDate.toString("dd/MM/yy hh:mm"));
    }
    ui->labelLastEarningsDateValue->setText(lastEarningsDate.isNull() ? tr("n/a") : lastEarningsDate.toString("dd/MM/yy hh:mm"));
    ui->labelWitnessEarningsValue->setText(generatedPoints.size() == 0 ? tr("n/a") : QString::number(generatedPoints.back().y(), 'f', 2));

    switch (scale)
    {
        case GraphScale::Blocks:
            ui->labelLockDurationValue->setText(nWitnessLength <= 0 ? tr("n/a") : tr("%1 blocks").arg(QString::number(nWitnessLength)));
            ui->labelExpectedEarningsDurationValue->setText(nExpectedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 blocks").arg(QString::number(nExpectedWitnessBlockPeriod)));
            ui->labelLockTimeRemainingValue->setText(nLockBlocksRemaining <= 0 ? tr("n/a") : tr("%1 blocks").arg(QString::number(nLockBlocksRemaining)));
            break;
        case GraphScale::Days:
            if (IsArgSet("-testnet"))
            {
                ui->labelLockDurationValue->setText(nWitnessLength <= 0 ? tr("n/a") : tr("%1 days").arg(QString::number(nWitnessLength/576.0, 'f', 2)));
                ui->labelExpectedEarningsDurationValue->setText(nExpectedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 days").arg(QString::number(nExpectedWitnessBlockPeriod/576.0, 'f', 2)));
                ui->labelLockTimeRemainingValue->setText(nLockBlocksRemaining <= 0 ? tr("n/a") : tr("%1 days").arg(QString::number(nLockBlocksRemaining/576.0, 'f', 2)));
            }
            else
            {
                //fixme:
            }
            break;
        case GraphScale::Weeks:
            if (IsArgSet("-testnet"))
            {
                ui->labelLockDurationValue->setText(nWitnessLength <= 0 ? tr("n/a") : tr("%1 weeks").arg(QString::number(nWitnessLength/576.0/7.0, 'f', 2)));
                ui->labelExpectedEarningsDurationValue->setText(nExpectedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 weeks").arg(QString::number(nExpectedWitnessBlockPeriod/576.0/7.0, 'f', 2)));
                ui->labelLockTimeRemainingValue->setText(nLockBlocksRemaining <= 0 ? tr("n/a") : tr("%1 weeks").arg(QString::number(nLockBlocksRemaining/576.0/7.0, 'f', 2)));
            }
            else
            {
                //fixme:
            }
            break;
        case GraphScale::Months:
            if (IsArgSet("-testnet"))
            {
                ui->labelLockDurationValue->setText(nWitnessLength <= 0 ? tr("n/a") : tr("%1 months").arg(QString::number(nWitnessLength/576.0/30.0, 'f', 2)));
                ui->labelExpectedEarningsDurationValue->setText(nExpectedWitnessBlockPeriod <= 0 ? tr("n/a") : tr("%1 months").arg(QString::number(nExpectedWitnessBlockPeriod/576.0/30.0, 'f', 2)));
                ui->labelLockTimeRemainingValue->setText(nLockBlocksRemaining <= 0 ? tr("n/a") : tr("%1 months").arg(QString::number(nLockBlocksRemaining/576.0/30.0, 'f', 2)));
            }
            else
            {
                //fixme:
            }
            break;
    }
}

void WitnessDialog::update()
{
    ui->receiveCoinsStackedWidget->setCurrentIndex(0);

    if (model && model->getActiveAccount())
    {
        if ( model->getActiveAccount()->IsPoW2Witness() )
        {
            if (pactiveWallet->GetBalance(model->getActiveAccount(), true) > 0)
            {
                ui->receiveCoinsStackedWidget->setCurrentIndex(1);
                plotGraphForAccount(model->getActiveAccount());
            }
        }
    }
}

void WitnessDialog::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;

    if (_clientModel)
    {
        //connect(_clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(updateSmartFeeLabel()));
    }
}

void WitnessDialog::setModel(WalletModel *_model)
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
    }
}
