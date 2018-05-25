// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "exchangeratedialog.h"
#include <qt/_Gulden/forms/ui_exchangeratedialog.h>
#include "optionsmodel.h"
#include "richtextdelegate.h"

#include <QModelIndex>


ExchangeRateDialog::ExchangeRateDialog(const QStyle *platformStyle, QWidget *parent, QAbstractTableModel* tableModel)
: QDialog( parent, Qt::Dialog )
, ui( new Ui::ExchangeRateDialog )
, platformStyle( platformStyle )
, optionsModel( NULL )
{
    ui->setupUi(this);
    ui->ExchangeRateTable->setModel(tableModel);

    ui->ExchangeRateTable->horizontalHeader()->hide();
    ui->ExchangeRateTable->verticalHeader()->hide();
    ui->ExchangeRateTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->ExchangeRateTable->setShowGrid(false);
    ui->ExchangeRateTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->ExchangeRateTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->ExchangeRateTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    RichTextDelegate* delegate = new RichTextDelegate(this);
    ui->ExchangeRateTable->setItemDelegate(delegate);

    setWindowFlags(windowFlags() ^ Qt::WindowContextHelpButtonHint);
}

void ExchangeRateDialog::setOptionsModel(OptionsModel* model)
{
    optionsModel = model;

    const QString& localCurrency = optionsModel->guldenSettings->getLocalCurrency();
    int size = ui->ExchangeRateTable->model()->rowCount();
    for (int i = 0; i < size; i++)
    {
        const QModelIndex& cur=ui->ExchangeRateTable->model()->index(i, 0);
        if (cur.data() == localCurrency)
        {
            ui->ExchangeRateTable->setCurrentIndex(cur);
            ui->ExchangeRateTable->scrollTo(cur);
            break;
        }
    }


    connect(ui->ExchangeRateTable->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(selectionChanged(const QItemSelection &, const QItemSelection &)), (Qt::ConnectionType)(Qt::AutoConnection|Qt::UniqueConnection));
}

void ExchangeRateDialog::disconnectSlots()
{
    disconnect(ui->ExchangeRateTable->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(selectionChanged(const QItemSelection &, const QItemSelection &)));
}

ExchangeRateDialog::~ExchangeRateDialog()
{
    delete ui;
}

void ExchangeRateDialog::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    if (optionsModel)
    {
        QString itemSel = selected.indexes()[0].data().toString();
        optionsModel->guldenSettings->setLocalCurrency(itemSel);
    }
}
