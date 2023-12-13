#include "sphere.h"

namespace fishy
{

	float Sphere::Intersect(const Ray& r) const
	{
		const vector3	oc = center - r.origin;
		const double neg_b = dot(oc, r.direction);
		double det = neg_b * neg_b - dot(oc, oc) + radius * radius;

		if (det < 0)
			return 0;
		else
			det = qSqrt(det);

		constexpr double epsilon = 1e-4;
		if (double t = neg_b - det; t > epsilon)
			return t;
		else if (t = neg_b + det; t > epsilon)
			return t;

		return 0;
	}
}