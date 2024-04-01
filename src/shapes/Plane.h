//
// Created by chankeep on 3/12/2024.
//

#ifndef FISHY_PLANE_H
#define FISHY_PLANE_H

#include "../core/Fishy.h"
#include "Triangle.h"


namespace Fishy
{

    class Plane : public Qt3DExtras::QPlaneMesh, public FishyShape
    {
    public:
        Plane() : width(50), height(50)
        {
            setWidth(50);
            setHeight(50);
            setMeshResolution(QSize(10,10));
            this->type = FShapeType::FPlane;

            normal = vector3(0,1,0);
        }
        Plane(const float &width, const float &height, const vector3& origin = {0,0,0}) : width(width), height(height)
        {
            setWidth(width);
            setHeight(height);
            setMeshResolution(QSize(10,10));
            this->type = FShapeType::FPlane;

            normal = vector3(0,1,0);
        }

        bool Intersect(const Ray &ray, Interaction &isect) const override;
        void setTransform(Qt3DCore::QTransform *transform) override;

        float width, height;
        vector3 p1, p2, p3, p4;
        Normal3 normal;
    };

} // Fishy

#endif //FISHY_PLANE_H
