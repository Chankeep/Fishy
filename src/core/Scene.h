#pragma once

#include "Fishy.h"
#include "../shapes/Sphere.h"
#include "Interaction.h"
#include "../shapes/Triangle.h"
#include "../shapes/ModelLoader.h"
#include "../cameras/PerspectiveCamera.h"
#include <QTreeWidget>

namespace Fishy
{
    class Scene : public Qt3DCore::QEntity
    {
    public:
        Scene() = default;

        ~Scene() override = default;

        Scene(std::vector<std::shared_ptr<Primitive>> prims, const vector2 &resolution) : prims(std::move(prims))
        {
            camera = std::make_shared<PerspectiveCamera>(
                    vector3(0, 0, -19), vector3(0, 0, 1).normalized(), vector3(0, 1, 0), 75, resolution
            );
        }

        bool Intersect(Ray &ray, Interaction &isect)
        {
            bool isHit = false;
            for (auto &prim: prims)
            {
                if (prim->Intersect(ray, isect))
                    isHit = true;

            }
            return isHit;
        }

        void updateRenderData()
        {
            for (auto &prim: prims)
            {
                prim->updateRenderData();
            }
        }

        void setSceneTreeWidget(QTreeWidget *sceneTree)
        {
            this->sceneTree = sceneTree;
        }

        void processPrims()
        {
            for (auto &prim: prims)
            {
                prim->setParent(this);
                auto objectTopItem = sceneTree->findItems("Objects", Qt::MatchFlags::enum_type::MatchExactly)[0];
                auto *item = new QTreeWidgetItem(objectTopItem);
                item->setText(0, prim->objectName());
            }
        }

        Qt3DCore::QEntity *findCurrentEntity(const QString &objName)
        {
            for (auto &prim: prims)
            {
                if (objName == prim->objectName())
                {
                    auto transform = prim->componentsOfType<Qt3DCore::QTransform>()[0];
                    return prim.get();
                }
            }
            return nullptr;
        }

        std::vector<std::shared_ptr<Primitive>> &getPrims()
        {
            return prims;
        }

        Camera *getCamera() const
        {
            return camera.get();
        }

        static Scene *createDefaultScene(const vector2 &resolution)
        {
            static std::shared_ptr<Material> red = std::make_shared<MatteMaterial>(Color(.75, .25, .25));
            static std::shared_ptr<Material> blue = std::make_shared<MatteMaterial>(Color(.25, .25, .75));
            static std::shared_ptr<Material> gray = std::make_shared<MatteMaterial>(Color(.75, .75, .75));

            std::shared_ptr<FishyShape> left = std::make_shared<Sphere>(1e5);
            std::shared_ptr<FishyShape> right = std::make_shared<Sphere>(1e5);
            std::shared_ptr<FishyShape> back = std::make_shared<Sphere>(1e5);
            std::shared_ptr<FishyShape> front = std::make_shared<Sphere>(1e5);
            std::shared_ptr<FishyShape> bottom = std::make_shared<Sphere>(1e5);
            std::shared_ptr<FishyShape> top = std::make_shared<Sphere>(1e5);

            std::shared_ptr<FishyShape> mirror = std::make_shared<Sphere>(5);
            std::shared_ptr<FishyShape> glass = std::make_shared<Sphere>(5);

            std::vector<std::shared_ptr<Primitive>> prims;
            prims.emplace_back(std::make_shared<GeometricPrimitive>(left, red, vector3(1e5 + 20, 0, 0)));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(right, blue, vector3(-1e5 + -20, 0, 0)));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(back, gray, vector3(0, 0, 1e5 + 30)));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(front, red, vector3(0, 0, -1e5 - 20)));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(bottom, blue, vector3(0, -1e5 - 18, 0)));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(top, red, vector3(0, 1e5 + 18, 0)));
//            prims.emplace_back(std::make_shared<GeometricPrimitive>(mirror, blue, nullptr));
//            prims.emplace_back(std::make_shared<GeometricPrimitive>(tri, red, nullptr));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(glass, red, vector3(-10, 0, 15)));

            ModelLoader bot;
//            bot.loadModel(R"(D:\Projects\Fishy\Fishy\assets\models\spiderbot.fbx)");
//            bot.buildNoTextureModel(prims, red);

            return new Scene(prims, resolution);
        }

    private:
        std::vector<std::shared_ptr<Primitive>> modelPrims;

        std::vector<std::shared_ptr<Primitive>> prims;
        std::vector<std::shared_ptr<Qt3DRender::QMesh>> models;
        std::vector<std::shared_ptr<Light>> lights;
        std::shared_ptr<Camera> camera;

        QTreeWidget *sceneTree;

    };
}
