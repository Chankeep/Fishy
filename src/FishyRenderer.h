#pragma once

#include <QtWidgets/QMainWindow>

#include "ui_Fishy.h"
#include "core/Fishy.h"
#include "core/Scene.h"
#include "core/Film.h"
#include "fishy/SceneManager.h"
#include "utilities/FishyThreadPool.h"

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
        void renderingFinished();

        void loadTransformData();
        void loadMaterialData();
        void loadMaterialName(const QString& name);

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
        void setMaterial(int index);

        void setImageWidth(int value);
        void setImageHeight(int value);

        void setEntityName();
        void updateCurEntityName();
        void updateTempName(const QString&);

        void renderRTSlot();
        void updatePropertiesWidget(Qt3DCore::QEntity *entity);

        void printMessage(const QString&);
        void onTimeout();

    private:
        Ui::FishyClass ui;

        std::shared_ptr<Film> film;
        Qt3DCore::QEntity* curEntity = nullptr;
        Qt3DCore::QTransform* curTransform = nullptr;
        Qt3DRender::QMaterial* curMaterial = nullptr;
        FMaterialType curMaterialType;

        std::shared_ptr<SceneManager> sceneManager;
        std::unique_ptr<Integrator> integrator;
        std::shared_ptr<Sampler> originalSampler;

        QString tempName;

        size_t samplesPerPixel;

        std::shared_ptr<FishyTimer> timer;
        std::shared_ptr<QTimer> paceTimer;

        QThreadPool* pool;

    };
}