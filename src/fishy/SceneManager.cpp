//
// Created by chankeep on 2/21/2024.
//

#include "SceneManager.h"
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>

namespace Fishy
{
    SceneManager::SceneManager(QWidget *previewWidget, QTreeWidget *sceneTreeWidget)
            : previewWidget(previewWidget), sceneTree(sceneTreeWidget)
    {
        modelLoader = std::make_shared<ModelLoader>();
    }

    SceneManager::~SceneManager()
    {
    }

    void SceneManager::initQt3DView()
    {
        qt3DView = std::make_shared<Qt3DExtras::Qt3DWindow>();
        qt3DView->defaultFrameGraph()->setClearColor(QColor(QRgb(0x4d4d4f)));
        container = QWidget::createWindowContainer(qt3DView.get());

        QSize screenSize = QSize(imageWidth, imageHeight);
        container->setMinimumSize(QSize(200, 100));
        container->setMaximumSize(screenSize);

        auto *hLayout = new QHBoxLayout(previewWidget.get());
        auto *vLayout = new QVBoxLayout();
        vLayout->setAlignment(Qt::AlignTop);
        hLayout->addWidget(container, 1);
        hLayout->addLayout(vLayout);

        setResolution(imageWidth, imageHeight);
    }

    void SceneManager::initCamera()
    {
        Qt3DRender::QCamera *cameraEntity = qt3DView->camera();
        auto cam = scene->getCamera();
        cameraEntity->setObjectName("camera");
        cameraEntity->lens()->setPerspectiveProjection(cam->fov, static_cast<float>(imageWidth) / static_cast<float>(imageHeight), 0.1f, 10000.0f);
        cameraEntity->setPosition(cam->position);
        cameraEntity->setUpVector(cam->up);
        cameraEntity->setViewCenter(cam->center);

        auto *cameraController = new Qt3DExtras::QFirstPersonCameraController(scene.get());
        cameraController->setCamera(cameraEntity);
    }

    void SceneManager::initLight()
    {
        auto *lightEntity = new Qt3DCore::QEntity(scene.get());
        lightEntity->setObjectName("light");
        std::shared_ptr<FishyShape> lightShape = std::make_shared<Plane>(130, 105);
        // std::shared_ptr<FishyShape> lightShape = std::make_shared<Plane>(10, 10);
        std::shared_ptr<Light> area_light = std::make_shared<Light>(Color(17, 17, 17), lightShape.get());
        std::shared_ptr<Material> black = std::make_shared<MatteMaterial>(Color());
        auto lightTransform = make_shared<Qt3DCore::QTransform>();
        lightTransform->setTranslation(vector3(278, 554, 278));
        lightTransform->setRotationX(180);
        // lightTransform->setTranslation(vector3(0, 20, 0));
        // lightTransform->setRotationX(180);
        scene->getPrims().emplace_back(std::make_shared<GeometricPrimitive>(lightShape, black, lightTransform, area_light));
        lightEntity->addComponent(area_light.get());
        lightEntity->addComponent(lightTransform.get());
    }

    void SceneManager::changeCurrentEntity(QTreeWidgetItem *item, int col)
    {
        if (item->text(0) == "Objects" || item->text(0) == "Lights")
            return;
        currentTreeItem = item;
        emit(currentEntityChanged(scene->findCurrentEntity(item->text(col))));
    }

    std::shared_ptr<FishyShape> SceneManager::createNewShape(FShapeType SType, FMaterialType MType)
    {
        if (SType == FShapeType::FPlane)
        {
            return std::make_shared<Plane>(10,10);
        }

        else if (SType == FShapeType::FSphere)
        {
            return std::make_shared<Sphere>();
        }

        return nullptr;
    }

    void SceneManager::addNewModel()
    {
        auto fileName = QFileDialog::getOpenFileName(nullptr,
                QString("Add a model"),
                "D:/",
                QString("files(*.obj)")
                );
        if (fileName.isEmpty()) {
            QMessageBox::warning(nullptr, "Warning!", "Failed to open the model file!");
        }
        else
        {
            loadModel(fileName);
        }

    }

    void SceneManager::addNewPlane()
    {
        emit(sendMessage("add new Plane"));
        auto shape = createNewShape(FShapeType::FPlane);
        auto transform = std::make_shared<Qt3DCore::QTransform>();
        auto newPrim = std::make_shared<GeometricPrimitive>(shape, white, transform);
        bindPrimitiveToScene(newPrim);
        scene->getPrims().emplace_back(newPrim);
        updatePrimProperties(newPrim);
    }


    void SceneManager::addNewSphere()
    {
        emit(sendMessage("add new Sphere"));
        auto shape = createNewShape(FShapeType::FSphere);
        auto transform = std::make_shared<Qt3DCore::QTransform>();
        auto newPrim = std::make_shared<GeometricPrimitive>(shape, white, transform);
        bindPrimitiveToScene(newPrim);
        scene->getPrims().emplace_back(newPrim);
        updatePrimProperties(newPrim);
    }

    void SceneManager::loadModel(const QString &path)
    {
        emit(sendMessage("add new Model:" + path));
        auto mesh = modelLoader->loadModel(path, white);
        if(mesh == nullptr)
        {
            emit(sendMessage("Model loading failure"));
            return;
        }
        mesh->setParent(scene.get());

        updateModelToTree(mesh);
        scene->getModels().emplace_back(mesh);
    }

    void SceneManager::displayPrimsProperties()
    {
        initSceneTree();
        auto prims = scene->getPrims();
        auto models = scene->getModels();
        auto objectTopItem = sceneTree->findItems("Objects", Qt::MatchFlags::enum_type::MatchExactly)[0];
        for (auto &prim: prims)
        {
            auto *item = new QTreeWidgetItem(objectTopItem);
            item->setText(0, prim->objectName());
        }

        for(auto &model : models)
        {
            auto *item = new QTreeWidgetItem(objectTopItem);
            item->setText(0, model->objectName());
        }
    }

    void SceneManager::updateModelToTree(const shared_ptr<Model> &model)
    {
        auto objectTopItem = sceneTree->findItems("Objects", Qt::MatchFlags::enum_type::MatchExactly)[0];
        auto *item = new QTreeWidgetItem(objectTopItem);
        item->setText(0, model->objectName());
    }


    void SceneManager::initSceneTree()
    {
        sceneTree->clear();
        auto* topItem1 = new QTreeWidgetItem();
        auto* topItem2 = new QTreeWidgetItem();
        auto* topItem3 = new QTreeWidgetItem();
        topItem1->setText(0, "Objects");
        topItem2->setText(0, "Lights");
        topItem3->setText(0, "Camera");
        sceneTree->setHeaderLabel("Hierarchy");
        sceneTree->addTopLevelItem(topItem1);
        sceneTree->addTopLevelItem(topItem2);
        sceneTree->addTopLevelItem(topItem3);
    }

    void SceneManager::updatePrimProperties(const shared_ptr<Primitive> &prim)
    {
        auto objectTopItem = sceneTree->findItems("Objects", Qt::MatchFlags::enum_type::MatchExactly)[0];
        auto *item = new QTreeWidgetItem(objectTopItem);
        item->setText(0, prim->objectName());
    }

    void SceneManager::bindPrimitiveToScene(const shared_ptr<Primitive> &prim)
    {
        prim->setParent(scene.get());
    }

    void SceneManager::bindPrimitivesToScene()
    {
        auto prims = scene->getPrims();
        auto models = scene->getModels();
        for (auto &prim: prims)
        {
            prim->setParent(scene.get());
        }
        for(auto& model: models)
        {
            auto modelPrims = model->getPrims();
            for(auto& modelPrim : modelPrims)
            {
                modelPrim->setParent(scene.get());
            }
        }
    }

    void SceneManager::updateRenderData()
    {
        auto prims = scene->getPrims();
        auto models = scene->getModels();
        for (auto &prim: prims)
        {
            prim->updateRenderData();
        }
        for(auto& model: models)
        {
            auto modelPrims = model->getPrims();
            for(auto& modelPrim : modelPrims)
            {
                modelPrim->updateRenderData();
            }
        }
    }

    void SceneManager::initDefaultMaterials(QComboBox* materialBox)
    {
        white = std::make_shared<MatteMaterial>(vector3(.73, .73, .73));
        red = std::make_shared<MatteMaterial>(vector3(.65, .05, .05));
        blue = std::make_shared<MatteMaterial>(vector3(.05, .05, .65));
        grey = std::make_shared<MatteMaterial>(vector3(.55, .55, .55));
        green = std::make_shared<MatteMaterial>(vector3(.12, .45, .15));
        mirrorMat = std::make_shared<MirrorMaterial>(Color(0.6, 0.6, 0.6));
        glassMat = std::make_shared<GlassMaterial>(Color(1, 1, 1) * 0.999, Color(1, 1, 1) * 0.999, 1.5);

        white->setObjectName("Phong(white)");
        red->setObjectName("Phong(red)");
        blue->setObjectName("Phong(blue)");
        grey->setObjectName("Phong(grey)");
        mirrorMat->setObjectName("Mirror");
        glassMat->setObjectName("Glass");

        materialBox->addItem(QString("Phong(white)"));
        materialBox->addItem(QString("Phong(red)"));
        materialBox->addItem(QString("Phong(blue)"));
        materialBox->addItem(QString("Phong(grey)"));
        materialBox->addItem(QString("Mirror"));
        materialBox->addItem(QString("Glass"));
    }

    void SceneManager::setResolution(size_t width, size_t height) {
        imageWidth = width;
        imageHeight = height;

        vector2 resolution = {static_cast<float>(width), static_cast<float>(height)};
        scene = createCornellBox(resolution);

        film = std::make_shared<Film>(resolution, QString("renderOutput.png"));
        QSize screenSize = QSize(imageWidth, imageHeight);
        container->setMaximumSize(screenSize);
        qt3DView->setRootEntity(scene.get());
    }


} // Fishy