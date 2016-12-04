// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_WELCOMEDIALOG_H
#define GULDEN_QT_WELCOMEDIALOG_H

#include "guiutil.h"

#include <QFrame>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QVariant>

class OptionsModel;
class PlatformStyle;
class WalletModel;

namespace Ui {
    class WelcomeDialog;
}

class WelcomeDialog : public QFrame
{
    Q_OBJECT

public:
    explicit WelcomeDialog(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~WelcomeDialog();

    void showEvent(QShowEvent *ev);
Q_SIGNALS:
    void loadWallet();
    
public Q_SLOTS:
    void recoverWallet();
    void processRecoveryPhrase();
    void newWallet();
    /** Show message and progress */
    void showMessage(const QString &message, int alignment, const QColor &color);
    void showProgress(const QString &message, int nProgress);

protected:

private:
    Ui::WelcomeDialog *ui;
    const PlatformStyle *platformStyle;

private Q_SLOTS:
      
};

#endif // GULDEN_QT_WELCOMEDIALOG_H
