// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
