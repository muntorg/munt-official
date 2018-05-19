// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_RICHTEXTDELEGATE_H
#define GULDEN_QT_RICHTEXTDELEGATE_H

#include <QStyledItemDelegate>

class RichTextDelegate : public QStyledItemDelegate
{
public:
    RichTextDelegate(QObject* parent);
protected:
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
};

#endif // GULDEN_QT_RICHTEXTDELEGATE_H
