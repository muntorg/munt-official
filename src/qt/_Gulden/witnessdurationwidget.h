// Copyright (c) 2016-2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef WITNESSDURATIONWIDGET_H
#define WITNESSDURATIONWIDGET_H

#include <QWidget>

namespace Ui {
class WitnessDurationWidget;
}

class WitnessDurationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WitnessDurationWidget(QWidget *parent = nullptr);
    ~WitnessDurationWidget();

private:
    Ui::WitnessDurationWidget *ui;
};

#endif // WITNESSDURATIONWIDGET_H
