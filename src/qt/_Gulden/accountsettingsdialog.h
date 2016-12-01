#ifndef GULDEN_QT_ACCOUNTSETTINGSDIALOG_H
#define GULDEN_QT_ACCOUNTSETTINGSDIALOG_H

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
class CAccount;

namespace Ui {
    class AccountSettingsDialog;
}

class AccountSettingsDialog : public QFrame
{
    Q_OBJECT

public:
    explicit AccountSettingsDialog(const PlatformStyle *platformStyle, QWidget *parent, CAccount* activeAccount, WalletModel* model);
    ~AccountSettingsDialog();

Q_SIGNALS:
      void cancel();
      void dismissAccountSettings();
    
public Q_SLOTS:
    void activeAccountChanged(CAccount* account);
    void applyChanges();
    void deleteAccount();
    void showSyncQr();

protected:

private:
    WalletModel* walletModel;
    Ui::AccountSettingsDialog* ui;
    const PlatformStyle* platformStyle;
    CAccount* activeAccount;

private Q_SLOTS:
    
      
};

#endif // GULDEN_QT_GULDENNEWACCOUNTDIALOG_H
