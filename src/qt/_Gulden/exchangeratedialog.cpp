// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "exchangeratedialog.h"
#include <qt/_Gulden/forms/ui_exchangeratedialog.h>
#include "optionsmodel.h"

#include <QPainter>
#include <QStyledItemDelegate>
#include <QModelIndex>
#include <QStyleOptionViewItem>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>


class HtmlDelegate : public QStyledItemDelegate
{
protected:
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
};

void HtmlDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 optionV4 = option;
    initStyleOption(&optionV4, index);

    QStyle *style = optionV4.widget? optionV4.widget->style() : QApplication::style();

    QTextDocument doc;
    doc.setHtml(optionV4.text);

    /// Painting item without text
    optionV4.text = QString();
    style->drawControl(QStyle::CE_ItemViewItem, &optionV4, painter);

    QAbstractTextDocumentLayout::PaintContext ctx;

    ctx.palette.setColor(QPalette::Text, optionV4.palette.color(QPalette::Normal, QPalette::Text));
    
    // Highlighting text if item is selected
    if (optionV4.state & QStyle::State_Selected)
        ctx.palette.setColor(QPalette::Text, optionV4.palette.color(QPalette::Active, QPalette::HighlightedText));

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &optionV4);
    painter->save();
    painter->translate(textRect.topLeft());
    painter->setClipRect(textRect.translated(-textRect.topLeft()));
    doc.documentLayout()->draw(painter, ctx);
    painter->restore();
}

QSize HtmlDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 optionV4 = option;
    initStyleOption(&optionV4, index);

    QTextDocument doc;
    doc.setHtml(optionV4.text);
    doc.setTextWidth(optionV4.rect.width());
    return QSize(doc.idealWidth(), doc.size().height());
}

ExchangeRateDialog::ExchangeRateDialog(const PlatformStyle *platformStyle, QWidget *parent, QAbstractTableModel* tableModel)
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
    
    HtmlDelegate* delegate = new HtmlDelegate();
    ui->ExchangeRateTable->setItemDelegate(delegate);
    
    setWindowFlags(windowFlags() ^ Qt::WindowContextHelpButtonHint);
}

ExchangeRateDialog::~ExchangeRateDialog()
{
    delete ui;
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
        
    
    connect(ui->ExchangeRateTable->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),  SLOT(selectionChanged(const QItemSelection &, const QItemSelection &)));
}

void ExchangeRateDialog::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    if (optionsModel)
    {
        QString itemSel = selected.indexes()[0].data().toString();
        optionsModel->guldenSettings->setLocalCurrency(itemSel);
    }
}
