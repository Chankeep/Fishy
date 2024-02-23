#pragma once

#include <QtWidgets/QMainWindow>

#include "ui_Fishy.h"
#include "core/Fishy.h"
#include "core/Scene.h"
#include "core/Film.h"
#include "fishy/SceneManager.h"

namespace Fishy
{
    class FishyRenderer : public QMainWindow
    {
    Q_OBJECT

    public:
        FishyRenderer(QWidget *parent = nullptr);
        ~FishyRenderer();

        bool initRender();
        bool renderQ3D();
        void resizeEvent(QResizeEvent *event) override;

    public slots:
        void setScaleX(double value);
        void setScaleY(double value);
        void setScaleZ(double value);
        void setTranslationX(double value);
        void setTranslationY(double value);
        void setTranslationZ(double value);
        void setRotationX(double value);
        void setRotationY(double value);
        void setRotationZ(double value);
        void setColorR(int value);
        void setColorG(int value);
        void setColorB(int value);

        void updateTempName(const QString&);
        void setEntityName();

        void renderRT();

        void updatePropertiesWidget(Qt3DCore::QEntity *entity);

        void printMessage(const QString&);

    private:
        Ui::FishyClass ui;

        Scene *scene;
        Film *film;
        Qt3DExtras::Qt3DWindow *view;
        Qt3DCore::QEntity* currentEntity = nullptr;

        std::unique_ptr<SceneManager> sceneManager;

        int samplersPerPixel = 2;

        QString tempName;
    };
}