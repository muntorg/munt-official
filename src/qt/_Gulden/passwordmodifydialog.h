#ifndef GULDEN_QT_PASSWORDMODIFYDIALOG_H
#define GULDEN_QT_PASSWORDMODIFYDIALOG_H

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
    class PasswordModifyDialog;
}

class PasswordModifyDialog : public QFrame
{
    Q_OBJECT

public:
    explicit PasswordModifyDialog(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~PasswordModifyDialog();

Q_SIGNALS:
    void dismiss();
    
public Q_SLOTS:

protected:

private:
    Ui::PasswordModifyDialog *ui;
    const PlatformStyle *platformStyle;

private Q_SLOTS:
    void setPassword();
    void oldPasswordChanged();
    void newPasswordChanged();
    void newPasswordRepeatChanged();
};

#endif // GULDEN_QT_PASSWORDMODIFYDIALOG_H
