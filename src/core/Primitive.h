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
    static uint32_t objectIndex = 0;

    class Primitive : public Qt3DCore::QEntity
    {
    public:
        Primitive(Qt3DCore::QNode *parent = nullptr)
                : Qt3DCore::QEntity(parent)
        {
        }

        virtual ~Primitive() = default;
        virtual bool Intersect(Ray &r, Interaction &isect) const = 0;
        virtual void updateRenderData() const = 0;
    };

    class GeometricPrimitive : public Primitive
    {
    public:
        std::shared_ptr<FishyShape> fishyShape;
        std::shared_ptr<Material> material;
        std::shared_ptr<Light> light;
        std::shared_ptr<Qt3DCore::QTransform> transform;

        GeometricPrimitive(const std::shared_ptr<FishyShape> &fishyShape,
                const std::shared_ptr<Material> &material,
                const vector3 &translation,
                const std::shared_ptr<Light> &light = nullptr
        )
                : fishyShape(fishyShape), material(material), light(light)
        {
            transform = std::make_shared<Qt3DCore::QTransform>();
            transform->setTranslation(translation);

            addComponent(transform.get());
            setShapeType(fishyShape.get());
            setMaterialType(material.get());

            if(objectIndex == 0)
            {
                this->setObjectName(QString("untitled").arg(QString::number(objectIndex++)));
            }
            else
            {
                this->setObjectName(QString("untitled_%1").arg(QString::number(objectIndex++)));
            }
        }

        ~GeometricPrimitive() override = default;

        bool Intersect(Ray &ray, Interaction &isect) const override
        {
            bool hit = fishyShape->Intersect(ray, isect);
            if (hit)
            {
                isect.bsdf = material->Scattering(isect);
                isect.emission = light ? light->Le(isect, isect.w_o) : Color();
            }
            return hit;
        }

        void setShapeType(FishyShape* shape)
        {
            if(shape->type == FShapeType::FSphere)
            {
                auto* castShape = dynamic_cast<Qt3DExtras::QSphereMesh*>(fishyShape.get());
                this->addComponent(castShape);
            }

            if(shape->type == FShapeType::FPlane)
            {
                auto* castShape = dynamic_cast<Qt3DExtras::QPlaneMesh*>(fishyShape.get());
                this->addComponent(castShape);
            }

        }

        void setMaterialType(Material* mat)
        {
            if(mat->type == FMaterialType::Matte)
            {
                auto *castMat = dynamic_cast<Qt3DExtras::QPhongMaterial*>(mat);
                addComponent(castMat);
            }

            if(mat->type == FMaterialType::Mirror)
            {
                auto *castMat = dynamic_cast<Qt3DExtras::QMetalRoughMaterial*>(mat);
                addComponent(castMat);
            }

            if(mat->type == FMaterialType::Glass)
            {
                auto *castMat = dynamic_cast<Qt3DExtras::QDiffuseSpecularMaterial*>(mat);
                addComponent(castMat);
            }

        }

        void updateRenderData() const override
        {
            fishyShape->setTransform(transform.get());
            material->setParameters();
        }


    };

}
// FishyRenderer

