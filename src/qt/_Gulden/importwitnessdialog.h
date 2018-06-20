// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_QT_IMPORTWITNESSDIALOG_H
#define GULDEN_QT_IMPORTWITNESSDIALOG_H

#include <QDialog>
#include "support/allocators/secure.h"

namespace Ui {
    class ImportWitnessDialog;
}

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

//! Dialog for importing a witness-only URL.
class ImportWitnessDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ImportWitnessDialog(QWidget *parent);
    virtual ~ImportWitnessDialog();

    SecureString getWitnessURL() const;
public Q_SLOTS:
    void accept();

private:

    Ui::ImportWitnessDialog *ui;
};

#endif // GULDEN_QT_IMPORTPRIVKEYDIALOG_H
