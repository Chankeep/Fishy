#include <QHBoxLayout>
#include <QVBoxLayout>
#include "FishyRenderer.h"

#include "cameras/PerspectiveCamera.h"
#include "core/Integrator.h"
#include "samplers/TrapezoidalSampler.h"
#include "utilities/FishyTimer.h"
#include "materials/Material.h"

namespace Fishy
{
    FishyRenderer::FishyRenderer(QWidget *parent) : QMainWindow(parent)
    {
        ui.setupUi(this);
        initRendering();
        renderQ3D();

        timer = make_shared<FishyTimer>();
        paceTimer = make_shared<QTimer>();


        connect(paceTimer.get(), SIGNAL(timeout()), this, SLOT(onTimeout()));

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
        connect(ui.imageWidthEdit, SIGNAL(valueChanged(int)), this, SLOT(setImageWidth(int value)));
        connect(ui.imageHeightEdit, SIGNAL(valueChanged(int)), this, SLOT(setImageHeight(int value)));
        connect(ui.nameEdit, SIGNAL(editingFinished()), this, SLOT(setEntityName()));
        connect(ui.nameEdit, SIGNAL(textEdited(const QString&)), this, SLOT(updateTempName(const QString&)));
        connect(ui.materialBox, SIGNAL(activated(int)), this, SLOT(setMaterial(int)));

        connect(ui.sceneTreeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            sceneManager.get(), SLOT(changeCurrentEntity(QTreeWidgetItem*,int)));

        connect(sceneManager.get(), SIGNAL(currentEntityChanged(Qt3DCore::QEntity*)),
            this, SLOT(updatePropertiesWidget(Qt3DCore::QEntity*)));
        connect(sceneManager.get(), SIGNAL(sendMessage(const QString&)),
            this, SLOT(printMessage(const QString&)));

        connect(ui.renderButton, SIGNAL(clicked()), this, SLOT(renderRTSlot()));

        connect(ui.actionSphere, SIGNAL(triggered(bool)), sceneManager.get(), SLOT(addNewSphere()));
        connect(ui.actionPlane, SIGNAL(triggered(bool)), sceneManager.get(), SLOT(addNewPlane()));

        connect(ui.actionLoadModel, SIGNAL(triggered(bool)), sceneManager.get(), SLOT(addNewModel()));

    }

    FishyRenderer::~FishyRenderer() {}

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
        sceneManager->initDefaultMaterials(ui.materialBox);
        sceneManager->initQt3DView();
        sceneManager->initCamera();
        sceneManager->initLight();
        sceneManager->displayPrimsProperties();
        sceneManager->bindPrimitivesToScene();

        ui.imageWidthEdit->setValue(sceneManager->getImageWidth());
        ui.imageHeightEdit->setValue(sceneManager->getImageHeight());

        film = sceneManager->getFilm();

        originalSampler = std::make_unique<TrapezoidalSampler>(samplesPerPixel);
        printMessage("sampler successfully initialized!");

        integrator = std::make_unique<PathIntegrator>();
        printMessage("integrator successfully initialized!");

        pool = QThreadPool::globalInstance();
        printMessage("thread pool successfully initialized!");

        return true;
    }

    void FishyRenderer::renderingFinished()
    {
        timer->end();
        paceTimer->stop();
        printMessage(QString("The time used to rendering is: %1s").arg(QString::number(timer->elapsedTime())));

        film->store_image();
        auto image = QPixmap::fromImage(*(film->getImage()));
        auto scale = 800 / image.size().width();
        auto h = int(image.size().height() * scale);
        image.scaled(QSize(800, h));
        ui.renderImage->setPixmap(image);
        ui.renderImage->setScaledContents(false);
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
        qDebug() << transform->matrix();
    }

    void FishyRenderer::setTranslationY(double value)
    {
        auto transform = curEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto position = transform->translation();
        position.setY(value);
        transform->setTranslation(position);
        qDebug() << transform->matrix();
    }

    void FishyRenderer::setTranslationZ(double value)
    {
        auto transform = curEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto position = transform->translation();
        position.setZ(value);
        transform->setTranslation(position);
        qDebug() << transform->matrix();
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
        if (entity->objectName() == "Camera") {}
        else
        {
            ui.propertyWidget->setEnabled(true);
            curEntity = entity;
            curTransform = entity->componentsOfType<Qt3DCore::QTransform>()[0];
            curMaterial = entity->componentsOfType<Qt3DRender::QMaterial>()[0];
            loadTransformData();
            loadMaterialData();
        }
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
        auto FishyMat = dynamic_cast<Material *>(curMaterial);
        loadMaterialName(FishyMat->objectName());
        curMaterialType = FishyMat->type;
        if (FishyMat->type == FMaterialType::Matte)
        {
            auto *mat = dynamic_cast<MatteMaterial *>(FishyMat);
            auto diffuse = mat->diffuse();
            ui.colorR->setValue(diffuse.red());
            ui.colorG->setValue(diffuse.green());
            ui.colorB->setValue(diffuse.blue());
        }
        else if (FishyMat->type == FMaterialType::Mirror)
        {
            auto *mat = dynamic_cast<MirrorMaterial *>(FishyMat);

            auto vColor = mat->baseColor();
            auto baseColor = vColor.value<QColor>();
            ui.colorR->setValue(baseColor.red());
            ui.colorG->setValue(baseColor.green());
            ui.colorB->setValue(baseColor.blue());
        }
        else if (FishyMat->type == FMaterialType::Glass)
        {
            auto *mat = dynamic_cast<GlassMaterial *>(FishyMat);

            auto vColor = mat->diffuse();
            auto baseColor = vColor.value<QColor>();
            ui.colorR->setValue(baseColor.red());
            ui.colorG->setValue(baseColor.green());
            ui.colorB->setValue(baseColor.blue());
        }
    }

    void FishyRenderer::loadMaterialName(const QString &name)
    {
        ui.materialBox->setCurrentIndex(ui.materialBox->findText(name));
    }

    void FishyRenderer::printMessage(const QString &msg)
    {
        ui.logMessage->append(msg);
        qDebug() << msg;
    }

    void FishyRenderer::setColorR(int value)
    {
        auto FishyMat = dynamic_cast<Material *>(curMaterial);
        if (curMaterialType == FMaterialType::Mirror)
        {
            auto *mat = dynamic_cast<MirrorMaterial *>(FishyMat);
            auto vColor = mat->baseColor();
            auto baseColor = vColor.value<QColor>();
            baseColor.setRed(value);
            mat->setBaseColor(baseColor);
        }
        else if (curMaterialType == FMaterialType::Glass)
        {
            auto *mat = dynamic_cast<GlassMaterial *>(FishyMat);
            auto vColor = mat->diffuse();
            auto baseColor = vColor.value<QColor>();
            baseColor.setRed(value);
            mat->setDiffuse(baseColor);
        }
        else
        {
            auto *mat = dynamic_cast<MatteMaterial *>(FishyMat);
            auto diffuse = mat->diffuse();
            diffuse.setRed(value);
            mat->setDiffuse(diffuse);
        }
    }

    void FishyRenderer::setColorG(int value)
    {
        auto FishyMat = dynamic_cast<Material *>(curMaterial);
        if (curMaterialType == FMaterialType::Mirror)
        {
            auto *mat = dynamic_cast<MirrorMaterial *>(FishyMat);
            auto vColor = mat->baseColor();
            auto baseColor = vColor.value<QColor>();
            baseColor.setGreen(value);
            mat->setBaseColor(baseColor);
        }
        else if (curMaterialType == FMaterialType::Glass)
        {
            auto *mat = dynamic_cast<GlassMaterial *>(FishyMat);
            auto vColor = mat->diffuse();
            auto baseColor = vColor.value<QColor>();
            baseColor.setGreen(value);
            mat->setDiffuse(baseColor);
        }
        else
        {
            auto *mat = dynamic_cast<MatteMaterial *>(FishyMat);
            auto diffuse = mat->diffuse();
            diffuse.setGreen(value);
            mat->setDiffuse(diffuse);
        }
    }

    void FishyRenderer::setColorB(int value)
    {
        auto FishyMat = dynamic_cast<Material *>(curMaterial);
        if (curMaterialType == FMaterialType::Mirror)
        {
            auto *mat = dynamic_cast<MirrorMaterial *>(FishyMat);
            auto vColor = mat->baseColor();
            auto baseColor = vColor.value<QColor>();
            baseColor.setBlue(value);
            mat->setBaseColor(baseColor);
        }
        else if (curMaterialType == FMaterialType::Glass)
        {
            auto *mat = dynamic_cast<GlassMaterial *>(FishyMat);
            auto vColor = mat->diffuse();
            auto baseColor = vColor.value<QColor>();
            baseColor.setBlue(value);
            mat->setDiffuse(baseColor);
        }
        else
        {
            auto *mat = dynamic_cast<MatteMaterial *>(FishyMat);
            auto diffuse = mat->diffuse();
            diffuse.setBlue(value);
            mat->setDiffuse(diffuse);
        }
    }

    void FishyRenderer::setMaterial(int index)
    {
        curEntity->removeComponent(curMaterial);
        auto matName = ui.materialBox->itemText(index);
        if (matName == QString("Phong(white)"))
        {
            auto *castMat = dynamic_cast<Qt3DExtras::QPhongMaterial *>(white.get());
            curEntity->addComponent(castMat);
        }
        else if (matName == QString("Phong(red)"))
        {
            auto *castMat = dynamic_cast<Qt3DExtras::QPhongMaterial *>(red.get());
            curEntity->addComponent(castMat);
        }
        else if (matName == QString("Phong(blue)"))
        {
            auto *castMat = dynamic_cast<Qt3DExtras::QPhongMaterial *>(blue.get());
            curEntity->addComponent(castMat);
        }
        else if (matName == QString("Phong(grey)"))
        {
            auto *castMat = dynamic_cast<Qt3DExtras::QPhongMaterial *>(grey.get());
            curEntity->addComponent(castMat);
        }
        else if (matName == QString("Mirror"))
        {
            auto *castMat = dynamic_cast<Qt3DExtras::QPhongMaterial *>(mirrorMat.get());
            curEntity->addComponent(castMat);
        }
        else if (matName == QString("Glass"))
        {
            auto *castMat = dynamic_cast<Qt3DExtras::QPhongMaterial *>(glassMat.get());
            curEntity->addComponent(castMat);
        }
    }

    void FishyRenderer::setImageWidth(int value)
    {
        sceneManager->initQt3DView();
        sceneManager->initCamera();
        sceneManager->initLight();
        sceneManager->displayPrimsProperties();
        sceneManager->bindPrimitivesToScene();
        sceneManager->setResolution(value, sceneManager->getImageHeight());
    }
    void FishyRenderer::setImageHeight(int value)
    {
        sceneManager->initQt3DView();
        sceneManager->initCamera();
        sceneManager->initLight();
        sceneManager->displayPrimsProperties();
        sceneManager->bindPrimitivesToScene();
        sceneManager->setResolution(sceneManager->getImageWidth(), value);
    }


    void FishyRenderer::updateTempName(const QString &name)
    {
        tempName = name;
    }

    void FishyRenderer::setEntityName()
    {
        auto curItem = sceneManager->getCurItem();
        curEntity->setObjectName(tempName);
        curItem->setText(0, tempName);
    }

    void FishyRenderer::updateCurEntityName() {}

    bool FishyRenderer::renderRT()
    {
        ui.propertyWidget->setEnabled(false);

        auto scene = sceneManager->getScene();

        renderSignal = 0;

        samplesPerPixel = ui.sppEdit->value();
        originalSampler->setSpp(samplesPerPixel);

        sceneManager->updateRenderData();
        printMessage("update rendering data successful!");
        scene->createBVH();

        printMessage("begin rendering");

        timer->begin();
        paceTimer->start(1000);

        size_t fragWidth = 4;
        size_t fragHeight = 4;

        auto imageWidth = sceneManager->getImageWidth();
        auto imageHeight = sceneManager->getImageHeight();

        pool->setMaxThreadCount(14);
        for (size_t x = 0; x < imageWidth; x = qMin(imageWidth, x + fragWidth))
        {
            for (size_t y = 0; y < imageHeight; y = qMin(imageHeight, y + fragHeight))
            {
                auto *task = new FragRenderer(vector2(x, y), vector2(x + fragWidth, y + fragHeight), scene,
                    scene->getCamera(), originalSampler, film);
                task->setAutoDelete(true);
                pool->start(task, 0);
            }
        }
        return true;
    }

    void FishyRenderer::onTimeout()
    {
        size_t totalPixel = sceneManager->getImageWidth() * sceneManager->getImageHeight();
        if (renderSignal == totalPixel)
        {
            printMessage("Rendering finished");
            renderingFinished();
        }
        else
        {
            auto msg = QString("Rendering (%1 spp) %2%").arg(originalSampler->SamplesPerPixel()).arg(
                100. * renderSignal / totalPixel);
            printMessage(msg);
        }
    }

}
