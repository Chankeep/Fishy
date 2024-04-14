#pragma once

#include "../core/Fishy.h"
#include "../core/FishyShape.h"

namespace Fishy
{
    class Sphere : public Qt3DExtras::QSphereMesh, public FishyShape
	{
	public:
		Sphere() : radius(5) {

            setRadius(5);
            setSlices(100);
            setRings(100);
            this->type = FShapeType::FSphere;

        }
		Sphere(float radius, vector3 origin = vector3(0,0,0)) : radius(radius), origin(origin) {

            setRadius(radius);
            setSlices(100);
            setRings(100);
            this->type = FShapeType::FSphere;
        }

		bool Intersect(const Ray &r, double tNear, double tFar, Interaction &isect) const override;
        void setTransform(Qt3DCore::QTransform* transform) override;
        double area() const override;

		double radius;
		vector3 origin;
	};
}
