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
#include <utility>

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
        Scene(std::vector<shared_ptr<Primitive> > prims);
        Scene(vector<shared_ptr<Primitive> > prims, shared_ptr<Camera> cam);
        Scene(vector<shared_ptr<Model> > models,
            vector<shared_ptr<Primitive> > prims,
            shared_ptr<Camera> cam
        );

        AABB boundingBox() const
        {
            return box;
        }

        bool Intersect(const Ray &ray, Interaction &isect) const;

        void createBVH();

        Qt3DCore::QEntity *findCurrentEntity(const QString &objName) const;

        vector<shared_ptr<Model> > &getModels()
        {
            return models;
        }

        vector<shared_ptr<Primitive> > &getPrims()
        {
            return prims;
        }

        vector<shared_ptr<Light> > &getLights()
        {
            return lights;
        }

        shared_ptr<Camera> getCamera() const
        {
            return camera;
        }

    private:
        vector<shared_ptr<Primitive> > prims;
        vector<shared_ptr<Model> > models;
        shared_ptr<Qt3DExtras::QSkyboxEntity> skybox;
        vector<shared_ptr<Light> > lights;
        shared_ptr<Camera> camera;
        shared_ptr<BVH> bvh;

        AABB box;
    };

    std::shared_ptr<Qt3DCore::QTransform> createTransform(const vector3 &translation = {},
        const vector3 &rotation = {},
        const vector3 &scale = {});

    static shared_ptr<Scene> createTestScene(const vector2 &resolution)
    {
        auto eye = vector3(0, 5, -10);
        auto center = vector3(0, 0, 0);
        auto up = vector3(0, 1, 0);
        auto cam = createPerspectiveCamera(eye, center, up, 60, resolution);

        shared_ptr<FishyShape> ground = make_shared<Sphere>(1000);

        vector<shared_ptr<Primitive> > prims;

        auto trans1 = make_shared<Qt3DCore::QTransform>();
        trans1->setTranslation(vector3(0, -1000, 0));
        prims.emplace_back(make_shared<GeometricPrimitive>(ground, grey, trans1));


        return make_shared<Scene>(prims, cam);
    }

    static shared_ptr<Scene> createDefaultScene(const vector2 &resolution)
    {
        auto cam = createPerspectiveCamera(
            vector3(0, 10, -19),
            vector3(0, -0.5, 1),
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

        vector<shared_ptr<Primitive> > prims;

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


        return make_shared<Scene>(prims, cam);
    }

    static shared_ptr<Scene> createPlaneScene(const vector2 &resolution)
    {
        auto position = vector3(0, 0, -40);
        auto center = vector3(0, 0, 1);
        auto cam = createPerspectiveCamera(
            position,
            center,
            vector3(0, 1, 0),
            90,
            resolution
        );

        int length = 40;
        int width = length;
        int height = 40;

        vector<shared_ptr<Primitive> > prims;
        shared_ptr<FishyShape> top = make_shared<Plane>(width, length);
        shared_ptr<FishyShape> bottom = make_shared<Plane>(width, length);
        shared_ptr<FishyShape> left = make_shared<Plane>(length, height);
        shared_ptr<FishyShape> right = make_shared<Plane>(length, height);
        shared_ptr<FishyShape> front = make_shared<Plane>(width, height);
        shared_ptr<FishyShape> back = make_shared<Plane>(width, height);

        shared_ptr<FishyShape> mirror = make_shared<Sphere>(5);
        shared_ptr<FishyShape> glass = make_shared<Sphere>(5);

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
        auto quaternion3 = Qt3DCore::QTransform::fromEulerAngles(vector3(-90, 90, 0));
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
        mirrorTrans->setTranslation(vector3(5, -11, 5));
        //        mirrorTrans->setScale(0.01);

        shared_ptr<Qt3DCore::QTransform> glassTrans = make_shared<Qt3DCore::QTransform>();
        glassTrans->setTranslation(vector3(-14, -13, 5));

        shared_ptr<Qt3DCore::QTransform> modelTrans = make_shared<Qt3DCore::QTransform>();

        modelTrans->setTranslation(vector3(0, -0, 0));
        modelTrans->setScale(3);
        //        modelTrans->setRotationY(40);
        //        modelTrans->setRotationX(30);
        vector<shared_ptr<Model> > models;
        shared_ptr<ModelLoader> model = make_shared<ModelLoader>();
        //        model->loadModel("../assets/models/cube.obj");
        //        model->buildNoTextureModel(prims, blue, modelTrans);
        //        models.emplace_back(model->getQMesh());

        prims.emplace_back(make_shared<GeometricPrimitive>(top, red, topTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(bottom, grey, bottomTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(left, red, leftTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(right, blue, rightTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(front, grey, frontTrans));
        //        prims.emplace_back(make_shared<GeometricPrimitive>(back, red, backTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(mirror, mirrorMat, mirrorTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(glass, glassMat, glassTrans));

        return make_shared<Scene>(models, prims, cam);
    }

    static shared_ptr<Scene> createCornellBox(const vector2 &resolution)
    {
        auto position = vector3(278, 278, -800);
        auto center = vector3(278, 278, 0);
        auto cam = createPerspectiveCamera(
            position,
            center,
            vector3(0, 1, 0),
            40,
            resolution
        );


        int length = 555;
        int width = length;
        int height = 555;

        vector<shared_ptr<Primitive> > prims;
        shared_ptr<FishyShape> top = make_shared<Plane>(width, length);
        shared_ptr<FishyShape> bottom = make_shared<Plane>(width, length);
        shared_ptr<FishyShape> left = make_shared<Plane>(length, height);
        shared_ptr<FishyShape> right = make_shared<Plane>(length, height);
        shared_ptr<FishyShape> front = make_shared<Plane>(width, height);
        //        shared_ptr<FishyShape> back = make_shared<Plane>(width, height);

        shared_ptr<FishyShape> mirror = make_shared<Sphere>(50);
        shared_ptr<FishyShape> glass = make_shared<Sphere>(50);
        shared_ptr<FishyShape> matte = make_shared<Sphere>(50);

        shared_ptr<Qt3DCore::QTransform> topTrans = make_shared<Qt3DCore::QTransform>();
        topTrans->setTranslation(vector3(278, 555, 278));
        auto quaternion1 = Qt3DCore::QTransform::fromEulerAngles(vector3(180, 0, 0));
        topTrans->setRotation(quaternion1);

        shared_ptr<Qt3DCore::QTransform> bottomTrans = make_shared<Qt3DCore::QTransform>();
        bottomTrans->setTranslation(vector3(278, 0, 278));
        auto quaternion2 = Qt3DCore::QTransform::fromEulerAngles(vector3(0, 0, 0));
        bottomTrans->setRotation(quaternion2);

        shared_ptr<Qt3DCore::QTransform> leftTrans = make_shared<Qt3DCore::QTransform>();
        leftTrans->setTranslation(vector3(555, 278, 278));
        auto quaternion3 = Qt3DCore::QTransform::fromEulerAngles(vector3(-90, 90, 0));
        leftTrans->setRotation(quaternion3);

        shared_ptr<Qt3DCore::QTransform> rightTrans = make_shared<Qt3DCore::QTransform>();
        rightTrans->setTranslation(vector3(0, 278, 278));
        auto quaternion4 = Qt3DCore::QTransform::fromEulerAngles(vector3(-90, -90, 0));
        rightTrans->setRotation(quaternion4);

        shared_ptr<Qt3DCore::QTransform> frontTrans = make_shared<Qt3DCore::QTransform>();
        frontTrans->setTranslation(vector3(278, 278, 555));
        auto quaternion5 = Qt3DCore::QTransform::fromEulerAngles(vector3(-90, 0, 0));
        frontTrans->setRotation(quaternion5);

        // shared_ptr<Qt3DCore::QTransform> backTrans = make_shared<Qt3DCore::QTransform>();
        // backTrans->setTranslation(vector3(0, 0, -length / 2));
        // auto quaternion6 = Qt3DCore::QTransform::fromEulerAngles(vector3(90, 0, 0));
        // backTrans->setRotation(quaternion6);

        auto models = vector<shared_ptr<Model> >();

        shared_ptr<Qt3DCore::QTransform> mirrorTrans = make_shared<Qt3DCore::QTransform>();
        mirrorTrans->setTranslation(vector3(5, -11, 5));

        shared_ptr<Qt3DCore::QTransform> glassTrans = make_shared<Qt3DCore::QTransform>();
        glassTrans->setTranslation(vector3(-14, -13, 5));

        shared_ptr<Qt3DCore::QTransform> matteTrans = make_shared<Qt3DCore::QTransform>();
        matteTrans->setTranslation(vector3(-14, -13, 5));

        shared_ptr<Qt3DCore::QTransform> traTrans1 = make_shared<Qt3DCore::QTransform>();
        traTrans1->setTranslation(vector3(278, 278, 278));

        shared_ptr<Qt3DCore::QTransform> traTrans2 = make_shared<Qt3DCore::QTransform>();
        traTrans2->setTranslation(vector3(278, 278, 278));


        shared_ptr<FishyShape> tra1 = make_shared<Triangle>(vector3(50, 0, 0),
            vector3(-50, 0, 0),
            vector3(0, 50, 50));
        shared_ptr<FishyShape> tra2 = make_shared<Triangle>(vector3(50.3717184, 2.30857432, 0.947163031),
            vector3(-18.9790058, 46.7251492, 0.307246037),
            vector3(14.948585, 23.9424253, 41.7951822));

        prims.emplace_back(make_shared<GeometricPrimitive>(top, white, topTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(bottom, white, bottomTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(left, green, leftTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(right, red, rightTrans));
        prims.emplace_back(make_shared<GeometricPrimitive>(front, white, frontTrans));
        // prims.emplace_back(make_shared<GeometricPrimitive>(tra1, red, traTrans1));
        // prims.emplace_back(make_shared<GeometricPrimitive>(tra2, blue, traTrans2));
        //        prims.emplace_back(make_shared<GeometricPrimitive>(back, red, backTrans));
        // prims.emplace_back(make_shared<GeometricPrimitive>(mirror, mirrorMat, mirrorTrans));
        // prims.emplace_back(make_shared<GeometricPrimitive>(glass, glassMat, glassTrans));
        // prims.emplace_back(make_shared<GeometricPrimitive>(matte, white, matteTrans));

        return make_shared<Scene>(models, prims, cam);
    }
}
