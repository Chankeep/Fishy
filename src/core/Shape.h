#pragma once
#include "ray.h"
#include "Interaction.h"

namespace fishy
{
	class Shape
	{
	public:
		virtual ~Shape() = default;
		virtual bool Intersect(Ray &r, Interaction& isect) const = 0;
	};
}
