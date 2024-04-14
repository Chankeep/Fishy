//
// Created by chankeep on 4/9/2024.
//

#include "Cube.h"

namespace Fishy
{
    Cube::Cube(const vector3 &pMin, const vector3 &pMax) : pMin(pMin), pMax(pMax)
    {
        width = pMax.x() - pMin.x();
        height = pMax.y() - pMin.y();
        length = pMax.z() - pMin.z();

        setXExtent(width);
        setYExtent(height);
        setZExtent(length);

        setXYMeshResolution(QSize(10, 10));
        setXZMeshResolution(QSize(10, 10));
        setYZMeshResolution(QSize(10, 10));

        
    }

    bool Cube::Intersect(const Ray &ray, double tNear, double tFar, Interaction &isect) const
    {
        return p0->Intersect(ray, tNear, tFar, isect) || p1->Intersect(ray, tNear, tFar, isect)
               || p2->Intersect(ray, tNear, tFar, isect) || p3->Intersect(ray, tNear, tFar, isect)
               || p4->Intersect(ray, tNear, tFar, isect) || p5->Intersect(ray, tNear, tFar, isect);
    }

    void Cube::setTransform(Qt3DCore::QTransform *transform)
    {
        p0->setTransform(transform);
        p1->setTransform(transform);
        p2->setTransform(transform);
        p3->setTransform(transform);
        p4->setTransform(transform);
        p5->setTransform(transform);

        pMin = transform->matrix().map(pMin);
        pMax = transform->matrix().map(pMax);

        box = AABB(pMin, pMax);
    }

    double Cube::area() const
    {
        return (width * length + width * height + length * height) * 2;
    }
} // Fishy
