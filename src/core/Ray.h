#pragma once
#include "common.h"

namespace fishy
{
	class Ray
	{
	public:
		Ray() = default;
		Ray(const vector3& o, const vector3& d) : origin(o), direction(d) {}

		vector3 operator()(float t) const { return origin + direction * t; }

		vector3 origin;
		vector3 direction;
	};
}

