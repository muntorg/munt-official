// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

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

    setVisible(false);


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
        //We use a bit of hackery to not have the progress sit on 0% whens syncing headers.
        //fixme: (2.1)
        if (ui->percentageProgress->text() == "0.00%")
        {
            ui->percentageProgress->setText(QString::number((count/50000)/100, 'f', 2)+"%");
        }
    }
}

void ModalOverlay::tipUpdate(int count, const QDateTime& blockDate, double nSyncProgress)
{
    QDateTime currentDate = QDateTime::currentDateTime();

    // show the percentage of blocks done
    ui->percentageProgress->setText(QString::number(nSyncProgress*100, 'f', 2)+"%");
    ui->progressBar->setValue(nSyncProgress*100);
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
