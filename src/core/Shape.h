#pragma once
#include "ray.h"

namespace fishy
{
	class Shape
	{
	public:
		virtual ~Shape() = default;
		virtual float Intersect(const Ray& r) const = 0;
	};
}
