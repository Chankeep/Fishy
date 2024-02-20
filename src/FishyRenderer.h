#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Fishy.h"

#include "core/Fishy.h"
#include "core/Scene.h"
#include "core/Film.h"

namespace Fishy
{
    class FishyRenderer : public QMainWindow
    {
    Q_OBJECT

    public:
        FishyRenderer(QWidget *parent = nullptr);
        ~FishyRenderer();

        bool initRender();
        bool renderRT();
        bool renderQ3D();

    private:
        Ui::FishyClass ui;

        Scene *scene;
        Film *film;
        std::unique_ptr<Camera> cam;
        Qt3DExtras::Qt3DWindow *view;

        int width = 1000;
        int height = 800;
    };
}