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

        std::shared_ptr<FishyShape> createNewShape(FShapeType SType, FMaterialType MType = FMaterialType::Matte);

        void addNewModel();

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

        void initDefaultMaterials()
        {
            white = std::make_shared<MatteMaterial>(vector3(1, 1, 1));
            red = std::make_shared<MatteMaterial>(vector3(.75, .25, .25));
            blue = std::make_shared<MatteMaterial>(vector3(.25, .25, .75));
            grey = std::make_shared<MatteMaterial>(vector3(.75, .75, .75));
            mirrorMat = std::make_shared<MirrorMaterial>(Color(1,1,1) * 0.999);
            glassMat = std::make_shared<GlassMaterial>(Color(1,1,1)*0.999,Color(1,1,1)*0.999,1.5);
        }

    public slots:
        void changeCurrentEntity(QTreeWidgetItem*, int);
        void addNewPlane();
        void addNewSphere();
        void loadModel();

    signals:
        void currentEntityChanged(Qt3DCore::QEntity* );

    private:
        std::shared_ptr<Scene> scene;
        std::shared_ptr<Film> film;
        std::shared_ptr<Qt3DExtras::Qt3DWindow> qt3DView;

        std::shared_ptr<QWidget> previewWidget;
        std::shared_ptr<QWidget> renderWidget;
        std::shared_ptr<QTreeWidget> sceneTree;
        QTreeWidgetItem* currentTreeItem;

        uint width, height;
    };

} // Fishy

#endif //FISHY_RENDERMANAGER_H
