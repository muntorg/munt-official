// Copyright (c) 2016 The Gulden developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_CLICKABLE_LABEL_H
#define GULDEN_CLICKABLE_LABEL_H

#include <QObject>
#include <QLabel>

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    ClickableLabel( QWidget* parent );
    ~ClickableLabel(){}
    void setChecked(bool checked);
    bool isChecked();
    
Q_SIGNALS:
    void clicked();
    
public Q_SLOTS:
    
protected:
    void mousePressEvent ( QMouseEvent * event );
    void mouseReleaseEvent ( QMouseEvent * event );
    void enterEvent(QEvent * event);
    void leaveEvent(QEvent * event);
    bool mouseIn;
};

#endif
