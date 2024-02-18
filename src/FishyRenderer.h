#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Fishy.h"

namespace Fishy
{
    class FishyRenderer : public QMainWindow
    {
    Q_OBJECT

    public:
        FishyRenderer(QWidget *parent = nullptr);
        ~FishyRenderer();

        bool render();

    private:
        Ui::FishyClass ui;
    };
}