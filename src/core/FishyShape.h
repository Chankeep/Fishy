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
        virtual bool Intersect(const Ray &r, Interaction &isect) const = 0;
        virtual AABB boundingBox() const = 0;
        virtual void setTransform(Qt3DCore::QTransform *transform) = 0;

        FShapeType type;
        AABB box;
    };
}
