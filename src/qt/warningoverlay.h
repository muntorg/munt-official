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

#ifndef GULDEN_QT_WARNINGOVERLAY_H
#define GULDEN_QT_WARNINGOVERLAY_H

#include <QWidget>

namespace Ui {
    class WarningOverlay;
}

/** Modal overlay to display information about the chain-sync state */
class WarningOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit WarningOverlay(QWidget *parent);
    ~WarningOverlay();

public Q_SLOTS:
    void setWarning(const QString& icon, const QString& title, const QString& warning);
    void toggleVisibility();
    // will show or hide the modal layer
    void showHide(bool hide = false, bool userRequested = false);
    void closeClicked();
    bool isLayerVisible() { return layerIsVisible; }

protected:
    bool eventFilter(QObject * obj, QEvent * ev);
    bool event(QEvent* ev);

private:
    Ui::WarningOverlay *ui;
    bool layerIsVisible;
    bool userClosed;
};

#endif
