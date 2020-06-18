// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_CLICKABLE_QRIMAGE_H
#define GULDEN_CLICKABLE_QRIMAGE_H


#if defined(HAVE_CONFIG_H)
#include "config/build-config.h"
#endif

#include <QObject>
#include <QLabel>
#include <QMenu>


class ClickableQRImage : public QLabel
{
    Q_OBJECT

public:
    explicit ClickableQRImage(QWidget *parent = 0);
    virtual ~ClickableQRImage();
    QImage exportImage();

    void setCode(const QString& qrString);

Q_SIGNALS:
    void clicked();

public Q_SLOTS:
    void saveImage();
    void copyImage();

protected:
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void contextMenuEvent(QContextMenuEvent *event);

private:
    QMenu* contextMenu;
    QString m_qrString;
};

#endif
