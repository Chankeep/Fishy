#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Fishy.h"

namespace fishy
{
    class Fishy : public QMainWindow
    {
    Q_OBJECT

    public:
        Fishy(QWidget *parent = nullptr);
        ~Fishy();

        bool render();

    private:
        Ui::FishyClass ui;
    };
}