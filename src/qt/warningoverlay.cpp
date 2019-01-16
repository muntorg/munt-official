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

#include "warningoverlay.h"
#include "ui_warningoverlay.h"

#include "guiutil.h"

#include "chainparams.h"

#include <QResizeEvent>
#include <QPropertyAnimation>

#include "_Gulden/GuldenGUI.h"
#include "validation/validation.h"

WarningOverlay::WarningOverlay(QWidget *parent)
: QWidget(parent)
, ui(new Ui::WarningOverlay)
, layerIsVisible(false)
, userClosed(false)
{
    ui->setupUi(this);
    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(closeClicked()));
    if (parent)
    {
        parent->installEventFilter(this);
        raise();
    }

    setVisible(false);

    ui->verticalLayout->setSpacing(0);
    ui->verticalLayout->setContentsMargins(0,0,0,0);

    ui->warningOverlayInfoText->setContentsMargins(0,0,0,0);
    ui->warningOverlayInfoText->setIndent(0);

    // Make URLS clickable (e.g. for updates)
    ui->warningOverlayInfoText->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->warningOverlayInfoText->setOpenExternalLinks(true);

    ui->closeButton->setContentsMargins(0,0,0,0);
    ui->closeButton->setCursor(Qt::PointingHandCursor);
}

WarningOverlay::~WarningOverlay()
{
    delete ui;
}

bool WarningOverlay::eventFilter(QObject * obj, QEvent * ev)
{
    if (obj == parent())
    {
        if (ev->type() == QEvent::Resize)
        {
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
bool WarningOverlay::event(QEvent* ev)
{
    if (ev->type() == QEvent::ParentAboutToChange)
    {
        if (parent()) parent()->removeEventFilter(this);
    }
    else if (ev->type() == QEvent::ParentChange)
    {
        if (parent())
        {
            parent()->installEventFilter(this);
            raise();
        }
    }
    return QWidget::event(ev);
}

void WarningOverlay::toggleVisibility()
{
    showHide(layerIsVisible, true);
    if (!layerIsVisible)
        userClosed = true;
}

void WarningOverlay::showHide(bool hide, bool userRequested)
{
    if ( (layerIsVisible && !hide) || (!layerIsVisible && hide) || (!hide && userClosed && !userRequested))
        return;

    if (!isVisible() && !hide)
        setVisible(true);

    setGeometry(0, hide ? 0 : height(), width(), height());

    int effectDuration = 3500;

    QPropertyAnimation* animation = new QPropertyAnimation(this, "pos");
    animation->setDuration(effectDuration);
    animation->setStartValue(QPoint(0, hide ? 0 : this->height()));
    animation->setEndValue(QPoint(0, hide ? this->height() : 0));
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    layerIsVisible = !hide;

    if (hide)
    {
         QTimer::singleShot(effectDuration, [this]{ setVisible(false); });
    }
    else
    {
        setVisible(true);
    }
}

void WarningOverlay::closeClicked()
{
    showHide(true);
    userClosed = true;
}

void WarningOverlay::setWarning(const QString& icon, const QString& title, const QString& warning)
{
    ui->warningOverlayHeading->setText(icon + "  " + title);
    QString finalWarning = warning;
    finalWarning.replace("\n", "<br/>");
    ui->warningOverlayInfoText->setText(finalWarning);
}
