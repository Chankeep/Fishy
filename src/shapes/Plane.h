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
        Plane();

        Plane(const Point3& p0, const Point3& p1, const Point3& p2, const Point3& p3);

        Plane(const float &width, const float &height);

        void initialize();
        bool Intersect(const Ray &ray, Interaction &isect) const override;
        AABB boundingBox() const override;
        void setTransform(Qt3DCore::QTransform *transform) override;

        float width{}, height{};
        std::unique_ptr<Triangle> t1, t2;
        Normal3 normal;
        Point3 p0, p1, p2, p3, center{0,0,0};
    };

} // Fishy

#endif //FISHY_PLANE_H
