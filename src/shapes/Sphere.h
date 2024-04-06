﻿#pragma once

#include "../core/Fishy.h"
#include "../core/FishyShape.h"

namespace Fishy
{
    class Sphere : public Qt3DExtras::QSphereMesh, public FishyShape
	{
	public:
		Sphere() : radius(5) {
            auto rvec = vector3(radius, radius, radius);
            box = AABB(origin - rvec, origin + rvec);

            setRadius(5);
            setSlices(100);
            setRings(100);
            this->type = FShapeType::FSphere;

        }
		Sphere(float radius, vector3 origin = vector3(0,0,0)) : radius(radius), origin(origin) {
            auto rvec = vector3(radius, radius, radius);
            box = AABB(origin - rvec, origin + rvec);

            setRadius(radius);
            setSlices(100);
            setRings(100);
            this->type = FShapeType::FSphere;
        }

		bool Intersect(const Ray &r, Interaction& isect) const override;
        AABB boundingBox() const {return box;}
        void setTransform(Qt3DCore::QTransform* transform) override;

		float radius;
		vector3 origin;
	};
}
