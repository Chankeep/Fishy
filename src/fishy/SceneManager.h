// Created by chankeep on 2/21/2024.
//

#ifndef FISHY_RENDERMANAGER_H
#define FISHY_RENDERMANAGER_H

#include "../core/Fishy.h"
#include "../core/Scene.h"
#include "../core/Film.h"
#include "../core/Camera.h"
#include "../materials/Material.h"
#include <QTreeWidget>
#include <QComboBox>

namespace Fishy
{

    class SceneManager : public QObject
    {
        Q_OBJECT
    public:
        SceneManager(QWidget* previewWidget, QTreeWidget *sceneTreeWidget);
        ~SceneManager() override;

        void initQt3DView();
        void initCamera();
        void initLight();
        std::shared_ptr<Qt3DExtras::Qt3DWindow> getQt3DView() const
        {
            return qt3DView;
        }

        std::shared_ptr<Film> getFilm() const
        {
            return film;
        }

        std::shared_ptr<Scene> getScene() const
        {
            return scene;
        }

        QTreeWidgetItem* getCurItem()
        {
            return currentTreeItem;
        }

        size_t getImageWidth() const
        {
            return imageWidth;
        }

        size_t getImageHeight() const
        {
            return imageHeight;
        }

        void initSceneTree();
        void updateRenderData();
        void bindPrimitivesToScene();
        void displayPrimsProperties();
        void loadModel(const QString& path);
        void updateModelToTree(const shared_ptr<Model>& model);
        void setResolution(size_t width, size_t height);
        void initDefaultMaterials(QComboBox* materialBox);
        void bindPrimitiveToScene(const shared_ptr<Primitive> &prim);
        void updatePrimProperties(const shared_ptr<Primitive> &prim);
        std::shared_ptr<FishyShape> createNewShape(FShapeType SType, FMaterialType MType = FMaterialType::Matte);

    public slots:
        void changeCurrentEntity(QTreeWidgetItem*, int);
        void addNewPlane();
        void addNewSphere();
        void addNewModel();

    signals:
        void currentEntityChanged(Qt3DCore::QEntity* );
        void sendMessage(const QString&);

    private:
        std::shared_ptr<Scene> scene;
        std::shared_ptr<Film> film;
        std::shared_ptr<Qt3DExtras::Qt3DWindow> qt3DView;

        std::shared_ptr<QWidget> previewWidget;
        std::shared_ptr<QWidget> renderWidget;
        std::shared_ptr<QTreeWidget> sceneTree;
        QTreeWidgetItem* currentTreeItem;
        QWidget* container;

        std::shared_ptr<ModelLoader> modelLoader;

        size_t imageWidth = 1000, imageHeight = 1000;
    };

} // Fishy

#endif //FISHY_RENDERMANAGER_H
