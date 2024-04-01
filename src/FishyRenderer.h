#pragma once

#include <QtWidgets/QMainWindow>

#include "ui_Fishy.h"
#include "core/Fishy.h"
#include "core/Scene.h"
#include "core/Film.h"
#include "utilities/FishyTimer.h"
#include "fishy/SceneManager.h"

namespace Fishy
{
    class FishyRenderer : public QMainWindow
    {
    Q_OBJECT

    public:
        FishyRenderer(QWidget *parent = nullptr);
        ~FishyRenderer();

        bool initRendering();
        bool renderQ3D();
        bool renderRT();

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
        void updateCurEntityName();

        void renderRTSlot();
        void updatePropertiesWidget(Qt3DCore::QEntity *entity);

        void printMessage(const QString&);

    private:
        Ui::FishyClass ui;

        std::shared_ptr<Scene> scene;
        std::shared_ptr<Film> film;
        Qt3DCore::QEntity* currentEntity = nullptr;

        std::unique_ptr<SceneManager> sceneManager;
        std::unique_ptr<Integrator> integrator;
        std::unique_ptr<Sampler> originalSampler;

        QString tempName;

        int samplesPerPixel;
        int width = 1000, height = 800;

        FishyTimer timer;
    };
}