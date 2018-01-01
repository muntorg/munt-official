// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "modaloverlay.h"
#include "ui_modaloverlay.h"

#include "guiutil.h"

#include "chainparams.h"

#include <QResizeEvent>
#include <QPropertyAnimation>

#include "_Gulden/GuldenGUI.h"
#include "validation.h"

ModalOverlay::ModalOverlay(QWidget *parent) :
QWidget(parent),
ui(new Ui::ModalOverlay),
bestHeaderHeight(0),
bestHeaderDate(QDateTime()),
layerIsVisible(false),
userClosed(false)
{
    ui->setupUi(this);
    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(closeClicked()));
    if (parent) {
        parent->installEventFilter(this);
        raise();
    }

    blockProcessTime.clear();
    setVisible(false);

    ui->verticalLayoutIcon->setEnabled(false);
    ui->warningIcon->setVisible(false);
    ui->infoTextStrong->setVisible(false);
    ui->labelNumberOfBlocksLeft->setVisible(false);
    ui->numberOfBlocksLeft->setVisible(false);
    ui->labelLastBlockTime->setVisible(false);
    ui->newestBlockDate->setVisible(false);
    ui->labelProgressIncrease->setVisible(false);
    ui->progressIncreasePerH->setVisible(false);
    ui->labelEstimatedTimeLeft->setVisible(false);
    ui->expectedTimeLeft->setVisible(false);
    
    ui->verticalLayoutSub->removeItem(ui->verticalSpacerAfterText);
    ui->verticalLayoutInfoText->removeItem(ui->verticalSpacerInTextSpace);
    ui->horizontalLayoutIconText->removeItem(ui->verticalLayoutIcon);

    ui->verticalLayoutMain->setSpacing(0);
    ui->verticalLayoutMain->setContentsMargins( 0, 0, 0, 0 );
    
    ui->verticalLayoutSub->setSpacing(0);
    ui->verticalLayoutSub->setContentsMargins( 0, 0, 0, 0 );
    
    ui->verticalLayout->setSpacing(0);
    ui->verticalLayout->setContentsMargins(0,0,0,0);
    
    ui->infoText->setContentsMargins(0,0,0,0);
    ui->infoText->setIndent(0);
    
    ui->closeButton->setContentsMargins(0,0,0,0);
    ui->closeButton->setCursor(Qt::PointingHandCursor);
    
    ui->labelSyncDone->setContentsMargins(0,0,0,0);
    ui->labelSyncDone->setIndent(0);
    
    ui->percentageProgress->setContentsMargins(0,0,0,0);
    ui->percentageProgress->setIndent(0);
    
    ui->progressBar->setVisible(false);

    ui->bgWidget->setStyleSheet("");
    ui->contentWidget->setStyleSheet("");
    
    ui->bgWidget->setPalette( QApplication::palette( ui->bgWidget ) );
    ui->contentWidget->setPalette( QApplication::palette( ui->contentWidget ) );

    /*QFrame* horizontalLine = new QFrame(ui->contentWidget);
    horizontalLine->setFrameStyle(QFrame::HLine);
    horizontalLine->setFixedHeight(1);
    horizontalLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    horizontalLine->setStyleSheet(GULDEN_DIALOG_HLINE_STYLE);
    ui->verticalLayout->addWidget(horizontalLine);*/
    
    ui->verticalLayout->insertStretch(0, 1);
    ui->verticalLayout->setStretch(1, 0);
    ui->verticalLayout->setStretch(2, 0);

    if (IsInitialBlockDownload())
    {
        ui->infoText->setText(tr("<br/><br/><b>Notice</b><br/><br/>Your wallet is now synchronizing with the Gulden network for the first time.<br/>Once your wallet has finished synchronizing, your balance and recent transactions will be visible."));
    }
    else
    {
        ui->infoText->setText(tr("<br/><br/><b>Notice</b><br/><br/>Your wallet is now synchronizing with the Gulden network.<br/>Once your wallet has finished synchronizing, your balance and recent transactions will be visible."));
    }

    ui->formLayout->setLabelAlignment(Qt::AlignLeft);
    ui->formLayout->setHorizontalSpacing(0);
}

ModalOverlay::~ModalOverlay()
{
    delete ui;
}

bool ModalOverlay::eventFilter(QObject * obj, QEvent * ev) {
    if (obj == parent()) {
        if (ev->type() == QEvent::Resize) {
            QResizeEvent * rev = static_cast<QResizeEvent*>(ev);
            resize(rev->size());
            if (!layerIsVisible)
                setGeometry(0, height(), width(), height());

        }
        else if (ev->type() == QEvent::ChildAdded) {
            raise();
        }
    }
    return QWidget::eventFilter(obj, ev);
}

//! Tracks parent widget changes
bool ModalOverlay::event(QEvent* ev) {
    if (ev->type() == QEvent::ParentAboutToChange) {
        if (parent()) parent()->removeEventFilter(this);
    }
    else if (ev->type() == QEvent::ParentChange) {
        if (parent()) {
            parent()->installEventFilter(this);
            raise();
        }
    }
    return QWidget::event(ev);
}

void ModalOverlay::setKnownBestHeight(int count, const QDateTime& blockDate)
{
    if (count > bestHeaderHeight) {
        bestHeaderHeight = count;
        bestHeaderDate = blockDate;
        //We use a bit of hackery to not have the progress sit on 0% whens yncing headers.
        //fixme: (GULDEN) (FUT) (MED) Improve this behaviour
        if (ui->percentageProgress->text() == "0.00%")
        {
            ui->percentageProgress->setText(QString::number((count/50000)/100, 'f', 2)+"%");
        }
    }
}

void ModalOverlay::tipUpdate(int count, const QDateTime& blockDate, double nVerificationProgress)
{
    QDateTime currentDate = QDateTime::currentDateTime();

    // keep a vector of samples of verification progress at height
    blockProcessTime.push_front(qMakePair(currentDate.toMSecsSinceEpoch(), nVerificationProgress));

    // show progress speed if we have more then one sample
    if (blockProcessTime.size() >= 2)
    {
        double progressStart = blockProcessTime[0].second;
        double progressDelta = 0;
        double progressPerHour = 0;
        qint64 timeDelta = 0;
        qint64 remainingMSecs = 0;
        double remainingProgress = 1.0 - nVerificationProgress;
        for (int i = 1; i < blockProcessTime.size(); i++)
        {
            QPair<qint64, double> sample = blockProcessTime[i];

            // take first sample after 500 seconds or last available one
            if (sample.first < (currentDate.toMSecsSinceEpoch() - 500 * 1000) || i == blockProcessTime.size() - 1) {
                progressDelta = progressStart-sample.second;
                timeDelta = blockProcessTime[0].first - sample.first;
                progressPerHour = progressDelta/(double)timeDelta*1000*3600;
                remainingMSecs = remainingProgress / progressDelta * timeDelta;
                break;
            }
        }
        // show progress increase per hour
        ui->progressIncreasePerH->setText(QString::number(progressPerHour*100, 'f', 2)+"%");

        // show expected remaining time
        ui->expectedTimeLeft->setText(GUIUtil::formatNiceTimeOffset(remainingMSecs/1000.0));

        static const int MAX_SAMPLES = 5000;
        if (blockProcessTime.count() > MAX_SAMPLES)
            blockProcessTime.remove(MAX_SAMPLES, blockProcessTime.count()-MAX_SAMPLES);
    }

    // show the last block date
    ui->newestBlockDate->setText(blockDate.toString());

    // show the percentage done according to nVerificationProgress
    ui->percentageProgress->setText(QString::number(nVerificationProgress*100, 'f', 2)+"%");
    ui->progressBar->setValue(nVerificationProgress*100);

    if (!bestHeaderDate.isValid())
        // not syncing
        return;

    // estimate the number of headers left based on nPowTargetSpacing
    // and check if the gui is not aware of the the best header (happens rarely)
    int estimateNumHeadersLeft = bestHeaderDate.secsTo(currentDate) / Params().GetConsensus().nPowTargetSpacing;
    bool hasBestHeader = bestHeaderHeight >= count;

    // show remaining number of blocks
    if (estimateNumHeadersLeft < HEADER_HEIGHT_DELTA_SYNC && hasBestHeader) {
        ui->numberOfBlocksLeft->setText(QString::number(bestHeaderHeight - count));
    } else {
        ui->numberOfBlocksLeft->setText(tr("Unknown. Syncing Headers (%1)...").arg(bestHeaderHeight));
        ui->expectedTimeLeft->setText(tr("Unknown..."));
    }
}

void ModalOverlay::toggleVisibility()
{
    showHide(layerIsVisible, true);
    if (!layerIsVisible)
        userClosed = true;
}

void ModalOverlay::showHide(bool hide, bool userRequested)
{
    if ( (layerIsVisible && !hide) || (!layerIsVisible && hide) || (!hide && userClosed && !userRequested))
        return;

    if (!isVisible() && !hide)
        setVisible(true);

    setGeometry(0, hide ? 0 : height(), width(), height());

    QPropertyAnimation* animation = new QPropertyAnimation(this, "pos");
    animation->setDuration(3500);
    animation->setStartValue(QPoint(0, hide ? 0 : this->height()));
    animation->setEndValue(QPoint(0, hide ? this->height() : 0));
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    layerIsVisible = !hide;
}

void ModalOverlay::closeClicked()
{
    showHide(true);
    userClosed = true;
}
