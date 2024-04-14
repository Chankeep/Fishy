#pragma once

#include "../Accelerator/AABB.h"
#include "Interaction.h"


namespace Fishy
{
    class FishyShape : public Qt3DRender::QGeometryRenderer
    {
    public:
        FishyShape() = default;
        virtual ~FishyShape() = default;
        virtual bool Intersect(const Ray &r, double tNear, double tFar, Interaction &isect) const = 0;

        virtual AABB boundingBox() const
        {
            return box;
        };
        virtual void setTransform(Qt3DCore::QTransform *transform) = 0;
        virtual double area() const = 0;

        FShapeType type;
        AABB box;
    };
}
