//
// Created by chankeep on 2/21/2024.
//

#include "SceneManager.h"
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace Fishy
{
    SceneManager::SceneManager(QWidget *previewWidget, QTreeWidget *sceneTreeWidget)
            : previewWidget(previewWidget), sceneTree(sceneTreeWidget)
    {
        width = 1000, height = 800;

        initDefaultMaterials();
        initQt3DView();
        initCamera();
        initLight();

        scene->displayPrimsProperties(sceneTree);
        scene->bindPrimitivesToScene();

//        auto *meshEntity = new Qt3DCore::QEntity(scene);
//        auto mesh = new Qt3DRender::QMesh(meshEntity);
//        mesh->setSource(QUrl::fromLocalFile("./temp.obj"));
//        qDebug() << mesh->status();

//        auto *meshTransform = new Qt3DCore::QTransform();
//        meshTransform->setScale(1.f);
//        meshTransform->setTranslation(QVector3D(0.0f, 0.0f, 0.0f));
//        auto *meshMaterial = new Qt3DExtras::QPhongMaterial();
//        meshMaterial->setDiffuse(QColor(QRgb(0x4f4f4c)));
//
//        meshEntity->addComponent(mesh);
//        meshEntity->addComponent(meshTransform);
//        meshEntity->addComponent(meshMaterial);

        qt3DView->setRootEntity(scene.get());
    }

    void SceneManager::initQt3DView()
    {
        vector2 resolution = {static_cast<float>(width), static_cast<float>(height)};
        scene = createPlaneScene(resolution);

        film = std::make_shared<Film>(resolution, QString("renderOutput.png"));

        qt3DView = std::make_shared<Qt3DExtras::Qt3DWindow>();
        qt3DView->defaultFrameGraph()->setClearColor(QColor(QRgb(0x4d4d4f)));
        QWidget *container = QWidget::createWindowContainer(qt3DView.get());
        QSize screenSize = QSize(width, height);
        container->setMinimumSize(QSize(200, 100));
        container->setMaximumSize(screenSize);

        auto *hLayout = new QHBoxLayout(previewWidget.get());
        auto *vLayout = new QVBoxLayout();
        vLayout->setAlignment(Qt::AlignTop);
        hLayout->addWidget(container, 1);
        hLayout->addLayout(vLayout);
    }

    void SceneManager::initCamera()
    {
        Qt3DRender::QCamera *cameraEntity = qt3DView->camera();
        auto cam = scene->getCamera();
        cameraEntity->setObjectName("camera");
        cameraEntity->lens()->setPerspectiveProjection(cam->fov, static_cast<float>(width) / static_cast<float>(height), 0.1f, 10000.0f);
        cameraEntity->setPosition(cam->position);
        cameraEntity->setUpVector(cam->up);
        cameraEntity->setViewCenter(cam->front);

        auto *cameraController = new Qt3DExtras::QFirstPersonCameraController(scene.get());
        cameraController->setCamera(cameraEntity);
    }

    void SceneManager::initLight()
    {
        auto *lightEntity = new Qt3DCore::QEntity(scene.get());
        lightEntity->setObjectName("light");
        std::shared_ptr<FishyShape> lightShape = std::make_shared<Plane>(15, 15);
        std::shared_ptr<Light> area_light = std::make_shared<Light>(Color(13, 13, 13), lightShape.get());
        std::shared_ptr<Material> black = std::make_shared<MatteMaterial>(Color());
        auto lightTransform = make_shared<Qt3DCore::QTransform>();
        lightTransform->setTranslation(vector3(0, 29.5, 0));
        scene->getPrims().emplace_back(std::make_shared<GeometricPrimitive>(lightShape, black, lightTransform, area_light));
        area_light->setParent(lightEntity);
        lightEntity->addComponent(area_light.get());
        lightEntity->addComponent(lightTransform.get());
    }

    void SceneManager::changeCurrentEntity(QTreeWidgetItem *item, int col)
    {
        if(item->text(0) == "Objects" || item->text(0) == "Lights")
            return;
        currentTreeItem = item;
        emit(currentEntityChanged(scene->findCurrentEntity(item->text(col))));
    }

    SceneManager::~SceneManager()
    {
    }

    std::shared_ptr<FishyShape> SceneManager::createNewShape(FShapeType SType, FMaterialType MType)
    {
        if(SType == FShapeType::FPlane)
        {
            return std::make_shared<Plane>();
        }

        else if(SType == FShapeType::FSphere)
        {
            return std::make_shared<Sphere>();
        }

        return nullptr;
    }

    void SceneManager::addNewModel()
    {
        qDebug() << "add new Model";
    }

    void SceneManager::addNewPlane()
    {
//        qDebug() << "add new Plane";
        auto shape = createNewShape(FShapeType::FPlane);
        auto transform = std::make_shared<Qt3DCore::QTransform>();
        auto newPrim = std::make_shared<GeometricPrimitive>(shape, white, transform);
        scene->bindPrimitiveToScene(newPrim);
        scene->getPrims().emplace_back(newPrim);
        scene->updatePrimProperties(sceneTree, newPrim);
    }

    void SceneManager::addNewSphere()
    {
//        qDebug() << "add new Sphere";
        auto shape = createNewShape(FShapeType::FSphere);
        auto transform = std::make_shared<Qt3DCore::QTransform>();
        auto newPrim = std::make_shared<GeometricPrimitive>(shape, white, transform);
        scene->bindPrimitiveToScene(newPrim);
        scene->getPrims().emplace_back(newPrim);
        scene->updatePrimProperties(sceneTree, newPrim);
    }

    void SceneManager::loadModel()
    {
        qDebug() << "add new Model";
    }
} // Fishy