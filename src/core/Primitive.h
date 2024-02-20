//
// Created by chankeep on 12/18/2023.
//
#pragma once

#include "Fishy.h"
#include "FishyShape.h"
#include "Interaction.h"
#include "../materials/Material.h"
#include "../lights/Light.h"

namespace Fishy
{
    class Primitive : public Qt3DCore::QEntity
    {
    public:
        Primitive(Qt3DCore::QNode *parent = nullptr)
                : Qt3DCore::QEntity(parent)
        {
        }

        virtual ~Primitive() = default;
        virtual bool Intersect(Ray &r, Interaction &isect) const = 0;
    };

    class GeometricPrimitive : public Primitive
    {
    public:
        std::shared_ptr<FishyShape> shape;
        std::shared_ptr<Material> material;
        std::shared_ptr<Light> light;

        GeometricPrimitive(const std::shared_ptr<FishyShape> &shape,
                const std::shared_ptr<Material> &material,
                const std::shared_ptr<Light> &light,
                Qt3DCore::QNode *parent = nullptr,
                vector3 translation = vector3(0,0,0))
                : Primitive(parent), shape(shape), material(material), light(light)
        {
            auto *transform = new Qt3DCore::QTransform();
            transform->setTranslation(translation);
            shape->setTransform(transform);
            addComponent(transform);
            addComponent(shape.get());
            addComponent(material.get());
        }

        ~GeometricPrimitive() override = default;

        bool Intersect(Ray &ray, Interaction &isect) const override
        {
            bool hit = shape->Intersect(ray, isect);
            if (hit)
            {
                isect.bsdf = material->Scattering(isect);
                isect.emission = light ? light->Le(isect, isect.w_o) : Color();
            }


            return hit;
        }

    };

}
// FishyRenderer

