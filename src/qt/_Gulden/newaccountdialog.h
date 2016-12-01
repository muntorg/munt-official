#ifndef GULDEN_QT_GULDENNEWACCOUNTDIALOG_H
#define GULDEN_QT_GULDENNEWACCOUNTDIALOG_H

#include "guiutil.h"

#include <QFrame>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QVariant>

class PlatformStyle;
class WalletModel;
class CAccountHD;
class WalletModel;

namespace Ui {
    class NewAccountDialog;
}

class NewAccountDialog : public QFrame
{
    Q_OBJECT

public:
    explicit NewAccountDialog(const PlatformStyle *platformStyle, QWidget *parent, WalletModel* model);
    ~NewAccountDialog();

    QString getAccountName();
Q_SIGNALS:
      void cancel();
      void accountAdded();
      void addAccountMobile();
    
public Q_SLOTS:

protected:

private:
    Ui::NewAccountDialog *ui;
    const PlatformStyle *platformStyle;
    CAccountHD* newAccount;
    WalletModel* walletModel;

private Q_SLOTS:
      void connectToMobile();
      void cancelMobile();
      void valueChanged();
      void addAccount();
      void showSyncQr();
};

#endif // GULDEN_QT_GULDENNEWACCOUNTDIALOG_H
