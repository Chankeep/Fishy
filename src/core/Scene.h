#pragma once

#include "Fishy.h"
#include "Interaction.h"
#include "../shapes/Plane.h"
#include "../shapes/Sphere.h"
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

        Scene(std::vector<std::shared_ptr<Primitive>> prims) : prims(std::move(prims))
        {
        }

        Scene(std::vector<std::shared_ptr<Primitive>> prims, std::shared_ptr<Camera> cam) : prims(std::move(prims)), camera(cam)
        {
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

        void bindPrimitiveToScene(std::shared_ptr<Primitive> prim)
        {
            prim->setParent(this);
        }

        void bindPrimitivesToScene()
        {
            for (auto &prim: prims)
            {
                prim->setParent(this);
            }
        }

        void updateRenderData()
        {
            for (auto &prim: prims)
            {
                prim->updateRenderData();
            }
        }

        void displayPrimsProperties(const std::shared_ptr<QTreeWidget>& sceneTree)
        {
            auto objectTopItem = sceneTree->findItems("Objects", Qt::MatchFlags::enum_type::MatchExactly)[0];
            for (auto &prim: prims)
            {
                auto *item = new QTreeWidgetItem(objectTopItem);
                item->setText(0, prim->objectName());
            }
        }

        void updatePrimProperties(const std::shared_ptr<QTreeWidget>& sceneTree, const std::shared_ptr<Primitive>& prim)
        {
            auto objectTopItem = sceneTree->findItems("Objects", Qt::MatchFlags::enum_type::MatchExactly)[0];
            auto * item = new QTreeWidgetItem(objectTopItem);
            item->setText(0, prim->objectName());
        }

        Qt3DCore::QEntity *findCurrentEntity(const QString &objName)
        {
            for (auto &prim: prims)
            {
                if (objName == prim->objectName())
                {
//                    auto transform = prim->componentsOfType<Qt3DCore::QTransform>()[0];
                    return prim.get();
                }
            }
            return nullptr;
        }

        std::vector<std::shared_ptr<Primitive>> &getPrims()
        {
            return prims;
        }

        std::shared_ptr<Camera> getCamera() const
        {
            return camera;
        }


    private:
        std::vector<std::shared_ptr<Primitive>> modelPrims;

        std::vector<std::shared_ptr<Primitive>> prims;
        std::vector<std::shared_ptr<Qt3DRender::QMesh>> models;
        std::shared_ptr<Qt3DExtras::QSkyboxEntity> skybox;
        std::vector<std::shared_ptr<Light>> lights;
        std::shared_ptr<Camera> camera;


    };

    static std::shared_ptr<Scene> createTestScene(const vector2& resolution)
    {
        auto eye = vector3(0,20,-28);
        auto center = vector3(0,0,0);
        auto up = vector3(0,1,0);
        auto direction = (center - eye).normalized();
        auto cam = createPerspectiveCamera(eye, direction, up, 85, resolution);

        std::shared_ptr<FishyShape> ground = std::make_shared<Sphere>(1000);
        std::shared_ptr<FishyShape> matSphere = std::make_shared<Sphere>(4);

        std::vector<std::shared_ptr<Primitive>> prims;
        prims.emplace_back(std::make_shared<GeometricPrimitive>(ground, grey, vector3(0, -1000, 0)));
        prims.emplace_back(std::make_shared<GeometricPrimitive>(matSphere, red, vector3(0, 4, 0)));

        return std::make_shared<Scene>(prims, cam);
    }

    static std::shared_ptr<Scene> createDefaultScene(const vector2& resolution)
    {
        auto cam = createPerspectiveCamera(
                vector3(0, 0, -19),
                vector3(0, 0, 1).normalized(),
                vector3(0, 1, 0),
                90,
                resolution
        );
        std::shared_ptr<FishyShape> left = std::make_shared<Sphere>(1e5);
        std::shared_ptr<FishyShape> right = std::make_shared<Sphere>(1e5);
        std::shared_ptr<FishyShape> back = std::make_shared<Sphere>(1e5);
        std::shared_ptr<FishyShape> front = std::make_shared<Sphere>(1e5);
        std::shared_ptr<FishyShape> bottom = std::make_shared<Sphere>(1e5);
        std::shared_ptr<FishyShape> top = std::make_shared<Sphere>(1e5);

        std::shared_ptr<FishyShape> mirror = std::make_shared<Sphere>(8);
        std::shared_ptr<FishyShape> glass = std::make_shared<Sphere>(8);

//            std::shared_ptr<FishyShape> testPlane = std::make_shared<Plane>(50, 50);

        std::vector<std::shared_ptr<Primitive>> prims;
        prims.emplace_back(std::make_shared<GeometricPrimitive>(left, red, vector3(1e5 + 20, 0, 0)));
        prims.emplace_back(std::make_shared<GeometricPrimitive>(right, blue, vector3(-1e5 + -20, 0, 0)));
        prims.emplace_back(std::make_shared<GeometricPrimitive>(back, grey, vector3(0, 0, 1e5 + 30)));
        prims.emplace_back(std::make_shared<GeometricPrimitive>(front, red, vector3(0, 0, -1e5 - 20)));
        prims.emplace_back(std::make_shared<GeometricPrimitive>(bottom, blue, vector3(0, -1e5 - 18, 0)));
        prims.emplace_back(std::make_shared<GeometricPrimitive>(top, red, vector3(0, 1e5 + 18, 0)));
//            prims.emplace_back(std::make_shared<GeometricPrimitive>(mirror, blue, nullptr));
//            prims.emplace_back(std::make_shared<GeometricPrimitive>(tri, red, nullptr));
//            prims.emplace_back(std::make_shared<GeometricPrimitive>(testPlane, blue, vector3(0, 0, 15)));
        prims.emplace_back(std::make_shared<GeometricPrimitive>(glass, glassMat, vector3(-10, -3, 15)));
        prims.emplace_back(std::make_shared<GeometricPrimitive>(mirror, mirrorMat, vector3(10, 5, 15)));

//        ModelLoader bot;
//            bot.loadModel(R"(D:\Projects\Fishy\Fishy\assets\models\spiderbot.fbx)");
//            bot.buildNoTextureModel(prims, red);

        return std::make_shared<Scene>(prims, cam);
    }

}
