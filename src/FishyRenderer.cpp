#include <QHBoxLayout>
#include <QVBoxLayout>
#include "FishyRenderer.h"

#include "cameras/PerspectiveCamera.h"
#include "core/Integrator.h"
#include "samplers/TrapezoidalSampler.h"

namespace Fishy
{


    FishyRenderer::FishyRenderer(QWidget *parent)
            : QMainWindow(parent)
    {
        ui.setupUi(this);
        initRendering();
        renderQ3D();
//        renderRTSlot();

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
        film = sceneManager->getFilm();

        sceneManager = std::make_unique<SceneManager>(ui.previewWidget, ui.sceneTreeWidget);
        scene = sceneManager->getScene();

        originalSampler = std::make_unique<TrapezoidalSampler>(samplesPerPixel);
        printMessage("sampler successfully initialized!");

        integrator = std::make_unique<PathIntegrator>();
        printMessage("integrator successfully initialized!");

        connect(integrator.get(), SIGNAL(sentMessage(QString)), this, SLOT(printMessage(const QString &)));


        return true;
    }

    void FishyRenderer::setScaleX(double value)
    {
        auto transform = currentEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto scale = transform->scale3D();
        scale.setX(value);
        transform->setScale3D(scale);
    }

    void FishyRenderer::setScaleY(double value)
    {
        auto transform = currentEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto scale = transform->scale3D();
        scale.setY(value);
        transform->setScale3D(scale);
    }

    void FishyRenderer::setScaleZ(double value)
    {
        auto transform = currentEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto scale = transform->scale3D();
        scale.setZ(value);
        transform->setScale3D(scale);
    }

    void FishyRenderer::setTranslationX(double value)
    {
        auto transform = currentEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto position = transform->translation();
        position.setX(value);
        transform->setTranslation(position);
    }

    void FishyRenderer::setTranslationY(double value)
    {
        auto transform = currentEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto position = transform->translation();
        position.setY(value);
        transform->setTranslation(position);
    }

    void FishyRenderer::setTranslationZ(double value)
    {
        auto transform = currentEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        auto position = transform->translation();
        position.setZ(value);
        transform->setTranslation(position);
    }

    void FishyRenderer::setRotationX(double value)
    {
        auto transform = currentEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        transform->setRotationX(value);
    }

    void FishyRenderer::setRotationY(double value)
    {
        auto transform = currentEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        transform->setRotationY(value);
    }

    void FishyRenderer::setRotationZ(double value)
    {
        auto transform = currentEntity->componentsOfType<Qt3DCore::QTransform>()[0];
        transform->setRotationZ(value);
    }

    void FishyRenderer::updatePropertiesWidget(Qt3DCore::QEntity *entity)
    {
        currentEntity = entity;
        auto transform = entity->componentsOfType<Qt3DCore::QTransform>()[0];

        auto position = transform->translation();
        ui.positionX->setValue(position.x());
        ui.positionY->setValue(position.y());
        ui.positionZ->setValue(position.z());

        auto scale = transform->scale3D();
        ui.scaleX->setValue(scale.x());
        ui.scaleY->setValue(scale.y());
        ui.scaleZ->setValue(scale.z());

        ui.rotationX->setValue(transform->rotationX());
        ui.rotationY->setValue(transform->rotationY());
        ui.rotationZ->setValue(transform->rotationZ());

        auto material = entity->componentsOfType<Qt3DExtras::QPhongMaterial>()[0];
        auto diffuse = material->diffuse();
        ui.colorR->setValue(diffuse.red());
        ui.colorG->setValue(diffuse.green());
        ui.colorB->setValue(diffuse.blue());

        ui.nameEdit->setText(entity->objectName());
    }

    void FishyRenderer::printMessage(const QString& msg)
    {
        ui.logMessage->append(msg);
    }

    void FishyRenderer::setColorR(int value)
    {
        auto material = currentEntity->componentsOfType<Qt3DExtras::QPhongMaterial>()[0];
        auto diffuse = material->diffuse();
        diffuse.setRed(value);
        material->setDiffuse(diffuse);
    }

    void FishyRenderer::setColorG(int value)
    {
        auto material = currentEntity->componentsOfType<Qt3DExtras::QPhongMaterial>()[0];
        auto diffuse = material->diffuse();
        diffuse.setGreen(value);
        material->setDiffuse(diffuse);
    }

    void FishyRenderer::setColorB(int value)
    {
        auto material = currentEntity->componentsOfType<Qt3DExtras::QPhongMaterial>()[0];
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
        currentEntity->setObjectName(tempName);
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

        timer.begin();

        integrator->Render(scene.get(), *scene->getCamera(), *originalSampler, film.get());

        timer.end();
        printMessage(QString("The time used to rendering is: %1s").arg(QString::number(timer.elapsedTime())));

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