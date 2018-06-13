// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GULDEN_QT_UTILITYDIALOG_H
#define GULDEN_QT_UTILITYDIALOG_H

#include <QDialog>
#include <QObject>

class GUI;
class ClientModel;

namespace Ui {
    class HelpMessageDialog;
}

/** "Help message" dialog box */
class HelpMessageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpMessageDialog(QWidget *parent, bool about);
    ~HelpMessageDialog();

    void printToConsole();
    void showOrPrint();

private:
    Ui::HelpMessageDialog *ui;
    QString text;

private Q_SLOTS:
    void on_okButton_accepted();
};


/** Small utility window that is shown during shutdown once rest of app is hidden, so th at user can be aware that shutdown is still ongoing. */
class ShutdownWindow : public QWidget
{
    Q_OBJECT
public:
    static QWidget* showShutdownWindow(GUI* centerOnWindow);
    static void destroyInstance();
protected:
    void closeEvent(QCloseEvent *event);
private:
    ShutdownWindow(QWidget *parent=0, Qt::WindowFlags f=0);
    static ShutdownWindow* spInstance;
};

#endif // GULDEN_QT_UTILITYDIALOG_H
