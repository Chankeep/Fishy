#include <QHBoxLayout>
#include <QVBoxLayout>
#include "FishyRenderer.h"

#include "cameras/PerspectiveCamera.h"
#include "core/Integrator.h"
#include "samplers/TrapezoidalSampler.h"
#include "utilities/FishyTimer.h"

namespace Fishy
{


    FishyRenderer::FishyRenderer(QWidget *parent)
            : QMainWindow(parent)
    {
        ui.setupUi(this);
        initRendering();
        renderQ3D();
//        renderRTSlot();
        timer = make_shared<FishyTimer>();

        connect(ui.positionX, SIGNAL(valueChanged(double)), this, SLOT(setTranslationX(double)));
        connect(ui.positionY, SIGNAL(valueChanged(double)), this, SLOT(setTranslationY(double)));
        connect(ui.positionZ, SIGNAL(valueChanged(double)), this, SLOT(setTranslationZ(double)));
        connect(ui.scaleX, SIGNAL(valueChanged(double)), this, SLOT(setScaleX(double)));
        connect(ui.scaleY, SIGNAL(valueChanged(double)), this, SLOT(setScaleY(double)));
        connect(ui.scaleZ, SIGNAL(valueChanged(double)), this, SLOT(setScaleZ(double)));
        connect(ui.rotationX, SIGNAL(valueChanged(double)), this, SLOT(setRotationX(double)));
        connect(ui.rotationY, SIGNAL(valueChanged(double)), this, SLOT(setRotationY(double)));
        connect(ui.rotationZ, SIGNAL(valueChanged(double)), this, SLOT(setRotationZ(double)));
        connect(ui.colorR, SIGNAL(valueChanged(int)), this, SLOT(setColorR(int)));
        connect(ui.colorG, SIGNAL(valueChanged(int)), this, SLOT(setColorG(int)));
        connect(ui.colorB, SIGNAL(valueChanged(int)), this, SLOT(setColorB(int)));
        connect(ui.nameEdit, SIGNAL(editingFinished()), this, SLOT(setEntityName()));
        connect(ui.nameEdit, SIGNAL(textEdited(const QString &)), this, SLOT(updateTempName(const QString &)));

        connect(ui.sceneTreeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem * , int)),
                sceneManager.get(), SLOT(changeCurrentEntity(QTreeWidgetItem * , int)));

        connect(sceneManager.get(), SIGNAL(currentEntityChanged(Qt3DCore::QEntity * )),
                this, SLOT(updatePropertiesWidget(Qt3DCore::QEntity * )));

        connect(ui.renderButton, SIGNAL(clicked()), this, SLOT(renderRTSlot()));

        connect(ui.actionSphere, SIGNAL(triggered(bool)), sceneManager.get(), SLOT(addNewSphere()));
        connect(ui.actionPlane, SIGNAL(triggered(bool)), sceneManager.get(), SLOT(addNewPlane()));
    }

    FishyRenderer::~FishyRenderer()
    {
    }

    void FishyRenderer::renderRTSlot()
    {
        renderRT();
    }

    bool FishyRenderer::renderQ3D()
    {
        ui.renderWidget->show();
        return true;
    }

    bool FishyRenderer::initRendering()
    {
        sceneManager = std::make_unique<SceneManager>(ui.previewWidget, ui.sceneTreeWidget);
        scene = sceneManager->getScene();
        film = sceneManager->getFilm();

        originalSampler = std::make_unique<TrapezoidalSampler>(samplesPerPixel);
        printMessage("sampler successfully initialized!");

        integrator = std::make_unique<PathIntegrator>();
        printMessage("integrator successfully initialized!");

        connect(integrator.get(), SIGNAL(sentMessage(QString)), this, SLOT(printMessage(const QString &)));

        pool = QThreadPool::globalInstance();

        return true;
    }

    void FishyRenderer::setScaleX(double value)
    {
        auto transform = curEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto scale = transform->scale3D();
        scale.setX(value);
        transform->setScale3D(scale);
    }

    void FishyRenderer::setScaleY(double value)
    {
        auto transform = curEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto scale = transform->scale3D();
        scale.setY(value);
        transform->setScale3D(scale);
    }

    void FishyRenderer::setScaleZ(double value)
    {
        auto transform = curEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto scale = transform->scale3D();
        scale.setZ(value);
        transform->setScale3D(scale);
    }

    void FishyRenderer::setTranslationX(double value)
    {
        auto transform = curEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto position = transform->translation();
        position.setX(value);
        transform->setTranslation(position);
    }

    void FishyRenderer::setTranslationY(double value)
    {
        auto transform = curEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto position = transform->translation();
        position.setY(value);
        transform->setTranslation(position);
    }

    void FishyRenderer::setTranslationZ(double value)
    {
        auto transform = curEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto position = transform->translation();
        position.setZ(value);
        transform->setTranslation(position);
    }

    void FishyRenderer::setRotationX(double value)
    {
        auto transform = curEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        transform->setRotationX(value);
    }

    void FishyRenderer::setRotationY(double value)
    {
        auto transform = curEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        transform->setRotationY(value);
    }

    void FishyRenderer::setRotationZ(double value)
    {
        auto transform = curEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        transform->setRotationZ(value);
    }

    void FishyRenderer::updatePropertiesWidget(Qt3DCore::QEntity *entity)
    {
        curEntity = entity;
        curTransform = entity->componentsOfType<Qt3DCore::QTransform>()[0];
        curMaterial = entity->componentsOfType<Qt3DRender::QMaterial>()[0];
        loadTransformData();
        loadMaterialData();

        ui.nameEdit->setText(entity->objectName());
    }

    void FishyRenderer::loadTransformData()
    {
        auto position = curTransform->translation();
        ui.positionX->setValue(position.x());
        ui.positionY->setValue(position.y());
        ui.positionZ->setValue(position.z());

        auto scale = curTransform->scale3D();
        ui.scaleX->setValue(scale.x());
        ui.scaleY->setValue(scale.y());
        ui.scaleZ->setValue(scale.z());

        ui.rotationX->setValue(curTransform->rotationX());
        ui.rotationY->setValue(curTransform->rotationY());
        ui.rotationZ->setValue(curTransform->rotationZ());
    }

    void FishyRenderer::loadMaterialData()
    {
        auto FishyMat = dynamic_cast<Material*>(curMaterial);
        curMaterialType = FishyMat->type;
        if(FishyMat->type == FMaterialType::Matte)
        {
            auto* mat = dynamic_cast<MatteMaterial*>(FishyMat);
            auto diffuse = mat->diffuse();
            ui.colorR->setValue(diffuse.red());
            ui.colorG->setValue(diffuse.green());
            ui.colorB->setValue(diffuse.blue());
        }
        else if(FishyMat->type == FMaterialType::Mirror)
        {
            auto* mat = dynamic_cast<MirrorMaterial*>(FishyMat);

            auto vColor = mat->baseColor();
            auto baseColor = vColor.value<QColor>();
            ui.colorR->setValue(baseColor.red());
            ui.colorG->setValue(baseColor.green());
            ui.colorB->setValue(baseColor.blue());

            auto vRough = mat->roughness();
            auto roughness = vRough.value<double>();

            auto vMetal = mat->metalness();
            auto metalness = vMetal.value<double>();


        }
        else if(FishyMat->type == FMaterialType::Glass)
        {

        }
    }

    void FishyRenderer::printMessage(const QString& msg)
    {
        ui.logMessage->append(msg);
    }

    void FishyRenderer::setColorR(int value)
    {
        auto material = curEntity->componentsOfType<Qt3DExtras::QPhongMaterial>()[0];
        auto diffuse = material->diffuse();
        diffuse.setRed(value);
        material->setDiffuse(diffuse);
    }

    void FishyRenderer::setColorG(int value)
    {
        auto material = curEntity->componentsOfType<Qt3DExtras::QPhongMaterial>()[0];
        auto diffuse = material->diffuse();
        diffuse.setGreen(value);
        material->setDiffuse(diffuse);
    }

    void FishyRenderer::setColorB(int value)
    {
        auto material = curEntity->componentsOfType<Qt3DExtras::QPhongMaterial>()[0];
        auto diffuse = material->diffuse();
        diffuse.setBlue(value);
        material->setDiffuse(diffuse);
    }

    void FishyRenderer::updateTempName(const QString& name)
    {
        tempName = name;
    }

    void FishyRenderer::setEntityName()
    {
        auto curItem = sceneManager->getCurItem();
        curEntity->setObjectName(tempName);
        curItem->setText(0, tempName);
    }

    void FishyRenderer::updateCurEntityName()
    {
    }

    bool FishyRenderer::renderRT()
    {
        samplesPerPixel = ui.sppEdit->text().toInt();
        originalSampler->setSpp(samplesPerPixel);

        scene->updateRenderData();
        printMessage("update rendering data successful!");
        printMessage("begin rendering");

        timer->begin();

        size_t fragWidth = 4;
        size_t fragHeight = 4;

        pool->setMaxThreadCount(14);
        for(size_t x = 0; x < width; x = qMin(width, x+fragWidth))
        {
            for(size_t y = 0; y < height; y = qMin(height, y + fragHeight))
            {
                auto* task = new FragRenderer(vector2(x, y), vector2(x + fragWidth, y+fragHeight), scene, scene->getCamera(), originalSampler, film);
                pool->start(task, 0);
            }
        }
//        pool->waitForDone();

//        integrator->Render(vector2(0, 0), vector2(1000, 800), scene, scene->getCamera(), originalSampler, film);

        timer->end();
        printMessage(QString("The time used to rendering is: %1s").arg(QString::number(timer->elapsedTime())));

        film->store_image();

        auto image = QPixmap::fromImage(*(film->getImage()));
        auto scale = 800 / image.size().width();
        auto h = int(image.size().height() * scale);
        image.scaled(QSize(800, h));
        ui.renderImage->setPixmap(image);
        ui.renderImage->setScaledContents(false);
        return true;
    }




}