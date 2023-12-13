#pragma once
#include "../core/shape.h"

namespace fishy
{
	class Sphere : Shape
	{
	public:
		Sphere() : radius(0.), center(0, 0, 0) {}
		Sphere(double radius, vector3 origin) : radius(radius), center(origin) {}
		Sphere(double radius, float x = 0., float y = 0., float z = 0.) : radius(radius), center(x, y, z) {}

		virtual float Intersect(const Ray& r) const override;

		double radius;
		vector3 center;
	};
}
