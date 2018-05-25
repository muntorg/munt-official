// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_EXCHANGERATEDIALOG_H
#define GULDEN_EXCHANGERATEDIALOG_H

#include <guiutil.h>

#include <QDialog>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QVariant>
#include <QAbstractTableModel>

class OptionsModel;
class QStyle;
class WalletModel;

namespace Ui {
    class ExchangeRateDialog;
}

class ExchangeRateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExchangeRateDialog(const QStyle *platformStyle, QWidget *parent, QAbstractTableModel* tableModel);
    ~ExchangeRateDialog();

    void setOptionsModel(OptionsModel* model);

    void disconnectSlots();

protected:
private:
    Ui::ExchangeRateDialog *ui;
    const QStyle *platformStyle;
    OptionsModel* optionsModel;

public Q_SLOTS:
private Q_SLOTS:
    void selectionChanged(const QItemSelection &, const QItemSelection &);
};

#endif // GULDEN_EXCHANGERATEDIALOG_H
