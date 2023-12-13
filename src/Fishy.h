#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Fishy.h"

class Fishy : public QMainWindow
{
    Q_OBJECT

public:
    Fishy(QWidget *parent = nullptr);
    ~Fishy();

private:
    Ui::FishyClass ui;
};
