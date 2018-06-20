// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_IMPORTPRIVKEYDIALOG_H
#define GULDEN_QT_IMPORTPRIVKEYDIALOG_H

#include <QDialog>
#include "support/allocators/secure.h"

namespace Ui {
    class ImportPrivKeyDialog;
}

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

/** Dialog for importing a private key.
 */
class ImportPrivKeyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportPrivKeyDialog(QWidget *parent);
    ~ImportPrivKeyDialog();

    SecureString getPrivKey() const;

public Q_SLOTS:
    void accept();

private:

    Ui::ImportPrivKeyDialog *ui;
};

#endif // GULDEN_QT_IMPORTPRIVKEYDIALOG_H
