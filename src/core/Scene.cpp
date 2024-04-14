//
// Created by chankeep on 4/4/2024.
//
#include "Fishy.h"
#include "Scene.h"

namespace Fishy
{
    Scene::Scene(std::vector<shared_ptr<Primitive>> prims): prims(std::move(prims)) {
        for (const auto &prim: prims)
        {
            box = surroundingBox(box, prim->boundingBox());
        }
    }

    Scene::Scene(vector<shared_ptr<Primitive>> prims, shared_ptr<Camera> cam): prims(std::move(prims)),
                                                                               camera(std::move(cam)) {
        for (const auto &prim: prims)
        {
            box = surroundingBox(box, prim->boundingBox());
        }
    }

    Scene::Scene(vector<shared_ptr<Model>> models, vector<shared_ptr<Primitive>> prims, shared_ptr<Camera> cam): prims(std::move(prims)), camera(std::move(cam)), models(std::move(models)) {
        for (const auto &prim: prims)
        {
            box = surroundingBox(box, prim->boundingBox());
        }
    }

    bool Scene::Intersect(const Ray &ray, Interaction &isect) const {
        bool isHit = false;
        // vector<shared_ptr<Primitive> > allPrims;
        // auto closest_so_far = Infinity;
        // for(auto& model : models)
        // {
        //     auto modelPrims = model->getPrims();
        //     allPrims.insert(allPrims.end(), modelPrims.begin(), modelPrims.end());
        // }
        // allPrims.insert(allPrims.end(), prims.begin(), prims.end());
        // if (allPrims.empty()) return false;
        //
        // for (auto &prim: allPrims)
        // {
        //     if (prim->Intersect(ray, 0, closest_so_far,isect))
        //     {
        //         isHit = true;
        //         closest_so_far = isect.tNear;
        //     }
        // }

        isHit = bvh->hit(ray, 0, Infinity, isect);
        return isHit;
    }

    void Scene::createBVH() {
        vector<shared_ptr<Primitive> > allPrims;
        for (auto &model: models)
        {
            auto modelPrims = model->getPrims();
            allPrims.insert(allPrims.end(), modelPrims.begin(), modelPrims.end());
        }
        allPrims.insert(allPrims.end(), prims.begin(), prims.end());
        bvh = make_shared<BVH>(allPrims);
    }

    Qt3DCore::QEntity * Scene::findCurrentEntity(const QString &objName) const {
        //     if(objName == "Camera")
        //         return camera.get();
        for (auto &prim: prims)
        {
            if (objName == prim->objectName())
            {
                //                    auto transform = prim->componentsOfType<Qt3DCore::QTransform>()[0];
                return prim.get();
            }
        }
        for (auto &model: models)
        {
            if (objName == model->objectName())
                return model.get();
        }
        return nullptr;
    }

    shared_ptr<Qt3DCore::QTransform> createTransform(const vector3 &translation, const vector3 &rotation, const vector3 &scale) {
        auto res = make_shared<Qt3DCore::QTransform>();
        auto quaternion = Qt3DCore::QTransform::fromEulerAngles(rotation);
        res->setRotation(quaternion);
        res->setTranslation(translation);
        res->setScale3D(scale);
        return res;
    }


}