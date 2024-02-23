//
// Created by chankeep on 2/21/2024.
//

#include "SceneManager.h"
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "../core/Scene.h"
#include "../core/Film.h"
#include "../core/Camera.h"

namespace Fishy
{
    SceneManager::SceneManager(QWidget *previewWidget, QTreeWidget *sceneTree)
            : previewWidget(previewWidget), sceneTree(sceneTree)
    {
        width = 1000, height = 800;
        vector2 resolution = {static_cast<float>(width), static_cast<float>(height)};
        scene = Scene::createDefaultScene(resolution);
        film = new Film(resolution, "renderOutput.png");
        auto cam = scene->getCamera();

        qt3DView = new Qt3DExtras::Qt3DWindow();
        qt3DView->defaultFrameGraph()->setClearColor(QColor(QRgb(0x4d4d4f)));
        QWidget *container = QWidget::createWindowContainer(qt3DView);
        QSize screenSize = QSize(width, height);
        container->setMinimumSize(QSize(200, 100));
        container->setMaximumSize(screenSize);

        auto *hLayout = new QHBoxLayout(previewWidget);
        auto *vLayout = new QVBoxLayout();
        vLayout->setAlignment(Qt::AlignTop);
        hLayout->addWidget(container, 1);
        hLayout->addLayout(vLayout);

        scene->setSceneTreeWidget(sceneTree);
        scene->processPrims();

        Qt3DRender::QCamera *cameraEntity = qt3DView->camera();
        cameraEntity->setObjectName("camera");
        cameraEntity->lens()->setPerspectiveProjection(cam->fov, static_cast<float>(width) / static_cast<float>(height), 0.1f, 10000.0f);
        cameraEntity->setPosition(cam->position);
        cameraEntity->setUpVector(cam->up);
        cameraEntity->setViewCenter(cam->center);

        auto *cameraController = new Qt3DExtras::QFirstPersonCameraController(scene);
        cameraController->setCamera(cameraEntity);

        auto *lightEntity = new Qt3DCore::QEntity(scene);
        lightEntity->setObjectName("light");
        std::shared_ptr<FishyShape> lightShape = std::make_shared<Sphere>(600);
        std::shared_ptr<Light> area_light = std::make_shared<Light>(Color(8, 8, 8), lightShape.get());
        std::shared_ptr<Material> black = std::make_shared<MatteMaterial>(Color());
        scene->getPrims().emplace_back(std::make_shared<GeometricPrimitive>(lightShape, black, vector3(0, 618 - .21, 0), area_light));
        area_light->setParent(lightEntity);
        lightEntity->addComponent(area_light.get());

        auto *lightTransform = new Qt3DCore::QTransform(lightEntity);
        lightTransform->setTranslation(vector3(0, 18 - .5, 0));
        lightEntity->addComponent(lightTransform);

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

        qt3DView->setRootEntity(scene);
        for (auto &child: scene->childNodes())
        {
            qDebug() << child->id();
        }

    }

    void SceneManager::changeCurrentEntity(QTreeWidgetItem *item, int col)
    {
        currentTreeItem = item;
        emit(currentEntityChanged(scene->findCurrentEntity(item->text(col))));
    }

    SceneManager::~SceneManager()
    {
        delete scene;
        delete film;
        delete qt3DView;
    }
} // Fishy