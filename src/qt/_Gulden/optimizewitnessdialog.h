// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_OPTIMIZEWITNESSDIALOG_H
#define GULDEN_QT_OPTIMIZEWITNESSDIALOG_H

#include "guiutil.h"

#include <QFrame>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QVariant>

class OptionsModel;
class QStyle;
class WalletModel;

namespace Ui {
    class OptimizeWitnessDialog;
}

class OptimizeWitnessDialog : public QFrame
{
    Q_OBJECT

public:
    explicit OptimizeWitnessDialog(WalletModel* walletModel_, const QStyle *platformStyle, QWidget *parent = 0);
    ~OptimizeWitnessDialog();

Q_SIGNALS:
    void dismiss(QWidget*);

protected:

private:
    Ui::OptimizeWitnessDialog *ui;
    const QStyle *platformStyle;
    WalletModel* walletModel;

private Q_SLOTS:
    void cancelClicked();
    void confirmClicked();
};

#endif // GULDEN_QT_OPTIMIZEWITNESSDIALOG_H
