#pragma once

#include "Fishy.h"
#include "Interaction.h"
#include "../shapes/Plane.h"
#include "../shapes/Sphere.h"
#include "../shapes/Triangle.h"
#include "../shapes/ModelLoader.h"
#include "../cameras/PerspectiveCamera.h"
#include "../Accelerator/BVH.h"
#include <QTreeWidget>

namespace Fishy
{
    using std::shared_ptr;
    using std::make_shared;
    using std::vector;

    class Scene : public Qt3DCore::QEntity
    {
    public:
        Scene() = default;

        ~Scene() override = default;

        Scene(std::vector<shared_ptr<Primitive>> prims, shared_ptr<BVH> bvh)
                : prims(move(prims)), bvh(bvh)
        {
        }

        Scene(vector<shared_ptr<Primitive>> prims, shared_ptr<Camera> cam, shared_ptr<BVH> bvh)
                : prims(move(prims)), camera(cam), bvh(bvh)
        {
            for (const auto &prim: prims)
            {
                box = surroundingBox(box, prim->boundingBox());
            }
        }

        AABB boundingBox() const
        {
            return box;
        }

        bool Intersect(const Ray &ray, Interaction &isect)
        {
            if (prims.empty()) return false;

            bool isHit = false;
            for (auto &prim: prims)
            {
                if (prim->Intersect(ray, isect))
                    isHit = true;

            }
//            isHit = bvh->hit(ray, 0, Infinity, isect);
            return isHit;
        }

        void bindPrimitiveToScene(shared_ptr<Primitive> prim)
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

        void displayPrimsProperties(const shared_ptr<QTreeWidget> &sceneTree)
        {
            auto objectTopItem = sceneTree->findItems("Objects", Qt::MatchFlags::enum_type::MatchExactly)[0];
            for (auto &prim: prims)
            {
                auto *item = new QTreeWidgetItem(objectTopItem);
                item->setText(0, prim->objectName());
            }
        }

        void updatePrimProperties(const shared_ptr<QTreeWidget> &sceneTree, const shared_ptr<Primitive> &prim)
        {
            auto objectTopItem = sceneTree->findItems("Objects", Qt::MatchFlags::enum_type::MatchExactly)[0];
            auto *item = new QTreeWidgetItem(objectTopItem);
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

        vector<shared_ptr<Primitive>> &getPrims()
        {
            return prims;
        }

        shared_ptr<Camera> getCamera() const
        {
            return camera;
        }


    private:
        vector<shared_ptr<Primitive>> modelPrims;

        vector<shared_ptr<Primitive>> prims;
        vector<shared_ptr<Qt3DRender::QMesh>> models;
        shared_ptr<Qt3DExtras::QSkyboxEntity> skybox;
        vector<shared_ptr<Light>> lights;
        shared_ptr<Camera> camera;
        shared_ptr<BVH> bvh;

        AABB box;
    };

    std::shared_ptr<Qt3DCore::QTransform> createTransform(const vector3 &translation = {},
            const vector3 &rotation = {},
            const vector3 &scale = {});

    static shared_ptr<Scene> createTestScene(const vector2 &resolution)
    {
        auto eye = vector3(0, 20, -28);
        auto center = vector3(0, 0, 0);
        auto up = vector3(0, 1, 0);
        auto direction = (center - eye).normalized();
        auto cam = createPerspectiveCamera(eye, direction, up, 85, resolution);

        shared_ptr<FishyShape> ground = make_shared<Sphere>(1000);
        shared_ptr<FishyShape> matSphere = make_shared<Sphere>(4);

        vector<shared_ptr<Primitive>> prims;

        auto trans1 = make_shared<Qt3DCore::QTransform>();
        trans1->setTranslation(vector3(0, -1000, 0));
        prims.emplace_back(make_shared<GeometricPrimitive>(ground, grey, trans1));

        auto trans2 = make_shared<Qt3DCore::QTransform>();
        trans2->setTranslation(vector3(0, 4, 0));
        prims.emplace_back(make_shared<GeometricPrimitive>(matSphere, red, trans2));

        auto bvh = make_shared<BVH>(prims);

        return make_shared<Scene>(prims, cam, bvh);
    }

    static shared_ptr<Scene> createDefaultScene(const vector2 &resolution)
    {
        auto cam = createPerspectiveCamera(
                vector3(0, 10, -19),
                vector3(0, -0.5, 1).normalized(),
                vector3(0, 1, 0),
                90,
                resolution
        );

        shared_ptr<FishyShape> left = make_shared<Sphere>(1e5);
        shared_ptr<FishyShape> right = make_shared<Sphere>(1e5);
        shared_ptr<FishyShape> back = make_shared<Sphere>(1e5);
        shared_ptr<FishyShape> front = make_shared<Sphere>(1e5);
        shared_ptr<FishyShape> bottom = make_shared<Sphere>(1e5);
        shared_ptr<FishyShape> top = make_shared<Sphere>(1e5);
        shared_ptr<FishyShape> mirror = make_shared<Sphere>(5);
        shared_ptr<FishyShape> glass = make_shared<Sphere>(5);

        vector<shared_ptr<Primitive>> prims;

        auto trans1 = createTransform(vector3(1e5 + 20, 0, 0));
        auto trans2 = createTransform(vector3(-1e5 - 20, 0, 0));
        auto trans3 = createTransform(vector3(0, 0, 1e5 + 30));
        auto trans5 = createTransform(vector3(0, -1e5 - 18, 0));
        auto trans4 = createTransform(vector3(0, 0, -1e5 - 20));
        auto trans6 = createTransform(vector3(0, 1e5 + 18, 0));
        auto trans7 = createTransform(vector3(-10, -10, 15));
        auto trans8 = createTransform(vector3(10, 5, 15));

        prims.emplace_back(make_shared<GeometricPrimitive>(left, red, trans1));
        prims.emplace_back(make_shared<GeometricPrimitive>(right, blue, trans2));
        prims.emplace_back(make_shared<GeometricPrimitive>(back, grey, trans3));
        prims.emplace_back(make_shared<GeometricPrimitive>(front, red, trans4));
        prims.emplace_back(make_shared<GeometricPrimitive>(bottom, blue, trans5));
        prims.emplace_back(make_shared<GeometricPrimitive>(top, red, trans6));
        prims.emplace_back(make_shared<GeometricPrimitive>(glass, glassMat, trans7));
        prims.emplace_back(make_shared<GeometricPrimitive>(mirror, mirrorMat, trans8));

        auto bvh = make_shared<BVH>(prims);

        return make_shared<Scene>(prims, cam, bvh);
    }

    static shared_ptr<Scene> createPlaneScene(const vector2 &resolution)
    {
        auto cam = createPerspectiveCamera(
                vector3(0, 0, -25),
                vector3(0, 0, 1).normalized(),
                vector3(0, 1, 0),
                100,
                resolution
        );

        int length = 60;
        int width = length;
        int height = 60;

        vector<shared_ptr<Primitive>> prims;
        shared_ptr<FishyShape> top = make_shared<Plane>(width, length);
        shared_ptr<FishyShape> bottom = make_shared<Plane>(width, length);
        shared_ptr<FishyShape> left = make_shared<Plane>(length, height);
        shared_ptr<FishyShape> right = make_shared<Plane>(length, height);
        shared_ptr<FishyShape> front = make_shared<Plane>(width, height);
        shared_ptr<FishyShape> back = make_shared<Plane>(width, height);

        shared_ptr<FishyShape> mirror = make_shared<Sphere>(6);
        shared_ptr<FishyShape> glass = make_shared<Sphere>(6);

        shared_ptr<Qt3DCore::QTransform> topTrans = make_shared<Qt3DCore::QTransform>();
        topTrans->setTranslation(vector3(0, height / 2, 0));
        auto quaternion1 = Qt3DCore::QTransform::fromEulerAngles(vector3(180, 0, 0));
        topTrans->setRotation(quaternion1);

        shared_ptr<Qt3DCore::QTransform> bottomTrans = make_shared<Qt3DCore::QTransform>();
        bottomTrans->setTranslation(vector3(0, -height / 2, 0));
        auto quaternion2 = Qt3DCore::QTransform::fromEulerAngles(vector3(0, 0, 0));
        bottomTrans->setRotation(quaternion2);

        shared_ptr<Qt3DCore::QTransform> leftTrans = make_shared<Qt3DCore::QTransform>();
        leftTrans->setTranslation(vector3(width / 2, 0, 0));
        auto quaternion3= Qt3DCore::QTransform::fromEulerAngles(vector3(-90, 90, 0));
        leftTrans->setRotation(quaternion3);

        shared_ptr<Qt3DCore::QTransform> rightTrans = make_shared<Qt3DCore::QTransform>();
        rightTrans->setTranslation(vector3(-width / 2, 0, 0));
        auto quaternion4 = Qt3DCore::QTransform::fromEulerAngles(vector3(-90, -90, 0));
        rightTrans->setRotation(quaternion4);

        shared_ptr<Qt3DCore::QTransform> frontTrans = make_shared<Qt3DCore::QTransform>();
        frontTrans->setTranslation(vector3(0, 0, length / 2));
        auto quaternion5 = Qt3DCore::QTransform::fromEulerAngles(vector3(-90, 0, 0));
        frontTrans->setRotation(quaternion5);

        shared_ptr<Qt3DCore::QTransform> backTrans = make_shared<Qt3DCore::QTransform>();
        backTrans->setTranslation(vector3(0, 0, -length / 2));
        auto quaternion6 = Qt3DCore::QTransform::fromEulerAngles(vector3(90, 0, 0));
        backTrans->setRotation(quaternion6);

        shared_ptr<Qt3DCore::QTransform> mirrorTrans = make_shared<Qt3DCore::QTransform>();
        mirrorTrans->setTranslation(vector3(5, 0, 15));

        shared_ptr<Qt3DCore::QTransform> glassTrans = make_shared<Qt3DCore::QTransform>();
        glassTrans->setTranslation(vector3(-10, -11, 15));

        prims.emplace_back(make_shared<GeometricPrimitive>(top, red, topTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(bottom, grey, bottomTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(left, blue, leftTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(right, blue, rightTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(front, grey, frontTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(back, red, backTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(mirror, mirrorMat, mirrorTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(glass, glassMat, glassTrans));

        auto bvh = make_shared<BVH>(prims);

        return make_shared<Scene>(prims, cam, bvh);
    }

}
