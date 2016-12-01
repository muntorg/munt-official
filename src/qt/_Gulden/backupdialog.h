#ifndef GULDEN_QT_BACKUPDIALOG_H
#define GULDEN_QT_BACKUPDIALOG_H

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
    class BackupDialog;
}

class BackupDialog : public QFrame
{
    Q_OBJECT

public:
    explicit BackupDialog(const PlatformStyle* platformStyle, QWidget* parent, WalletModel* model);
    ~BackupDialog();

Q_SIGNALS:
      void cancel();
      void saveBackupFile();
      void dismiss();
    
public Q_SLOTS:

protected:

private:
    Ui::BackupDialog* ui;
    const PlatformStyle* platformStyle;
    WalletModel* walletModel;

private Q_SLOTS:
    void showBackupPhrase();
      
};

#endif // GULDEN_QT_BACKUPDIALOG_H
