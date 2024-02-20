#pragma once
#include "../core/FishyShape.h"

namespace Fishy
{
    class Sphere : public FishyShape
	{
	public:
		Sphere() : radius(0.) {
            this->setRadius(radius);
            this->setSlices(100);
            this->setRings(100);
        }
		Sphere(float radius, vector3 origin) : radius(radius), center(origin) {
            this->setRadius(radius);
            this->setSlices(100);
            this->setRings(100);
        }

		bool Intersect(const Ray &r, Interaction& isect) const override;
        void setTransform(Qt3DCore::QTransform* transform) override;

		float radius;
		vector3 center{0,0,0};
	};
}
