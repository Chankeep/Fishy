//
// Created by chankeep on 4/9/2024.
//

#ifndef CUBE_H
#define CUBE_H

#include "Plane.h"

namespace Fishy
{
    class Cube : public Qt3DExtras::QCuboidMesh, public FishyShape
    {
    public:
        Cube() =default;
        virtual ~Cube() override = default;

        Cube(const vector3& pMin, const vector3& pMax);

        virtual bool Intersect(const Ray &ray, double tNear, double tFar, Interaction &isect) const override;
        virtual void setTransform(Qt3DCore::QTransform *transform) override;
        virtual double area() const override;

        double length;
        double width;
        double height;
        vector3 pMin;
        vector3 pMax;

        std::unique_ptr<Plane> p0, p1, p2, p3, p4, p5;
    };
} // Fishy

#endif //CUBE_H
