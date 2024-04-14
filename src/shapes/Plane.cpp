//
// Created by chankeep on 3/12/2024.
//

#include "Plane.h"

namespace Fishy
{

    Plane::Plane(const Point3 &p0, const Point3 &p1, const Point3 &p2, const Point3 &p3)
            : p0(p0), p1(p1), p2(p2), p3(p3)
    {
        width = p2.x() - p0.x();
        height = p1.z() - p0.z();
        setWidth(width);
        setHeight(height);
        setMeshResolution(QSize(10, 10));

        auto v1 = p1 - p0;
        auto v2 = p3 - p1;
        normal = cross(v1, v2).normalized();
        center = (p0 + p1 + p2 + p3) / 4;

        t1 = std::make_unique<Triangle>(p0, p1, p2);
        t2 = std::make_unique<Triangle>(p0, p2, p3);
        this->type = FShapeType::FPlane;
    }

    Plane::Plane(const double &width, const double &height) : width(width), height(height)
    {
        setWidth(width);
        setHeight(height);
        setMeshResolution(QSize(10, 10));
        this->type = FShapeType::FPlane;

        auto x = width / 2;
        auto z = height / 2;
        p0 = Point3(-x, 0, -z);
        p1 = Point3(-x, 0, z);
        p2 = Point3(x, 0, z);
        p3 = Point3(x, 0, -z);

        normal = vector3(0, 1, 0);
        t1 = std::make_unique<Triangle>(p0, p1, p2);
        t2 = std::make_unique<Triangle>(p0, p2, p3);
    }

    bool Plane::Intersect(const Ray &ray, double tNear, double tFar, Interaction &isect) const
    {
        if(!t1 || !t2)
        {
            return false;
        }
        return t1->Intersect(ray, tNear, tFar, isect) || t2->Intersect(ray, tNear, tFar, isect);
    }

    void Plane::setTransform(Qt3DCore::QTransform *transform)
    {
        p0 = transform->matrix().map(p0);
        p1 = transform->matrix().map(p1);
        p2 = transform->matrix().map(p2);
        p3 = transform->matrix().map(p3);
        center = transform->matrix().map(center);

        t1->setTransform(transform);
        t2->setTransform(transform);

        vector3 pMax, pMin;
        pMax.setX(std::max({p0.x(), p1.x(), p2.x(), p3.x()}));
        pMax.setY(std::max({p0.y(), p1.y(), p2.y(), p3.y()}));
        pMax.setZ(std::max({p0.z(), p1.z(), p2.z(), p3.z()}));

        pMin.setX(std::min({p0.x(), p1.x(), p2.x(), p3.x()}));
        pMin.setY(std::min({p0.y(), p1.y(), p2.y(), p3.y()}));
        pMin.setZ(std::min({p0.z(), p1.z(), p2.z(), p3.z()}));
        box = {pMin, pMax};
    }

    double Plane::area() const
    {
        return width * height;
    }


} // Fishy