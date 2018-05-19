// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "richtextdelegate.h"

#include <QStyleOptionViewItem>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QApplication>

RichTextDelegate::RichTextDelegate(QObject* parent)
: QStyledItemDelegate(parent)
{
}

void RichTextDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem styleOption = option;
    initStyleOption(&styleOption, index);

    QStyle *style = styleOption.widget? styleOption.widget->style() : QApplication::style();

    QTextDocument doc;
    doc.setHtml(styleOption.text);

    /// Painting item without text
    styleOption.text = QString();
    style->drawControl(QStyle::CE_ItemViewItem, &styleOption, painter);

    QAbstractTextDocumentLayout::PaintContext ctx;

    ctx.palette.setColor(QPalette::Text, styleOption.palette.color(QPalette::Normal, QPalette::Text));

    // Highlighting text if item is selected
    if (styleOption.state & QStyle::State_Selected)
        ctx.palette.setColor(QPalette::Text, styleOption.palette.color(QPalette::Active, QPalette::HighlightedText));

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &styleOption);

    // Apply vertical/horizontal alignment if necessary.
    {
        int nMarginLeft = 0;
        int nTotalWidth = styleOption.rect.width();
        int nTextRectWidth = doc.idealWidth();
        if (nTotalWidth > nTextRectWidth)
        {
            if ((styleOption.displayAlignment & Qt::AlignCenter) == Qt::AlignCenter)
                nMarginLeft = (nTotalWidth - nTextRectWidth) / 2;
            else if ((styleOption.displayAlignment & Qt::AlignRight) == Qt::AlignRight)
                nMarginLeft = (nTotalWidth - nTextRectWidth);
        }
        textRect.setLeft(textRect.left() + nMarginLeft);

        int nMarginTop = 0;
        int nTotalHeight = styleOption.rect.height();
        int nTextRectHeight = doc.size().height();
        if (nTotalHeight > nTextRectHeight)
        {
            if ((styleOption.displayAlignment & Qt::AlignVCenter) == Qt::AlignVCenter)
                nMarginTop = (nTotalHeight - nTextRectHeight) / 2;
            else if ((styleOption.displayAlignment & Qt::AlignBottom) == Qt::AlignBottom)
                nMarginTop = (nTotalHeight - nTextRectHeight);
        }
        textRect.setTop(textRect.top() + nMarginTop);
    }

    painter->save();
    painter->translate(textRect.topLeft());
    painter->setClipRect(textRect.translated(-textRect.topLeft()));
    doc.documentLayout()->draw(painter, ctx);
    painter->restore();
}

QSize RichTextDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem styleOption = option;
    initStyleOption(&styleOption, index);

    QTextDocument doc;
    doc.setHtml(styleOption.text);
    doc.setTextWidth(styleOption.rect.width());
    return QSize(doc.idealWidth(), doc.size().height());
}
