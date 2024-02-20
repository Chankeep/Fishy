#pragma once

#include "Fishy.h"
#include "../shapes/Sphere.h"
#include "Interaction.h"
#include "../shapes/Triangle.h"
#include "../shapes/ModelLoader.h"

namespace Fishy
{
    class Scene : public Qt3DCore::QEntity
    {
    public:
        Scene() = default;

        Scene(std::vector<std::shared_ptr<Primitive>> prims) : prims(std::move(prims))
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

        const std::vector<std::shared_ptr<Primitive>> &GetPrims()
        {
            return prims;
        }

        static Scene *CreateSmallptScene()
        {
            Scene* res = new Scene();
            std::shared_ptr<FishyShape> left = std::make_shared<Sphere>(1e5, vector3(1e5 + 20, 0, 0));
            std::shared_ptr<FishyShape> right = std::make_shared<Sphere>(1e5, vector3(-1e5 + -20, 0, 0));
            std::shared_ptr<FishyShape> back = std::make_shared<Sphere>(1e5, vector3(0, 0, 1e5 + 30));
            std::shared_ptr<FishyShape> front = std::make_shared<Sphere>(1e5, vector3(0, 0, -1e5 - 20));
            std::shared_ptr<FishyShape> bottom = std::make_shared<Sphere>(1e5, vector3(0, -1e5 - 18, 0));
            std::shared_ptr<FishyShape> top = std::make_shared<Sphere>(1e5, vector3(0, 1e5 + 18, 0));

            std::shared_ptr<FishyShape> mirror = std::make_shared<Sphere>(5, vector3(2, 0, 10));
            std::shared_ptr<FishyShape> glass = std::make_shared<Sphere>(5, vector3(-10, 0, 15));
            std::shared_ptr<FishyShape> light = std::make_shared<Sphere>(600, vector3(0, 618 - .21, 0));

            std::shared_ptr<Material> red = std::make_shared<MatteMaterial>(Color(.75, .25, .25));
            std::shared_ptr<Material> blue = std::make_shared<MatteMaterial>(Color(.25, .25, .75));
            std::shared_ptr<Material> gray = std::make_shared<MatteMaterial>(Color(.75, .75, .75));
            std::shared_ptr<Material> black = std::make_shared<MatteMaterial>(Color());

            std::shared_ptr<Light> area_light = std::make_shared<Light>(Color(8, 8, 8), light.get());

            std::vector<std::shared_ptr<Primitive>> prims;
            prims.emplace_back(std::make_shared<GeometricPrimitive>(left, red, nullptr, res, vector3(-1e5 - 20, 0, 0)));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(right, blue, nullptr, res,vector3(1e5 + 20, 0, 0)));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(back, gray, nullptr, res,vector3(0, 0, 1e5 + 30)));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(front, red, nullptr, res,vector3(0, 0, -1e5 - 20)));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(bottom, blue, nullptr, res,vector3(0, -1e5 - 18, 0)));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(top, red, nullptr, res,vector3(0, 1e5 + 18, 0)));
//            prims.emplace_back(std::make_shared<GeometricPrimitive>(mirror, blue, nullptr));
//            prims.emplace_back(std::make_shared<GeometricPrimitive>(tri, red, nullptr));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(glass, red, nullptr, res, vector3(10, 0, 15)));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(light, black, area_light));

            ModelLoader bot;
//            bot.loadModel(R"(D:\Projects\Fishy\Fishy\assets\models\testCube.fbx)");
//            bot.buildNoTextureModel(prims, red);

            res->prims = prims;
            return res;
        }

    private:
        std::vector<std::shared_ptr<Primitive>> prims;
    };
}
