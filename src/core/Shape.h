#pragma once
#include "Ray.h"
#include "Interaction.h"

#include <Qt3DCore/QEntity>

namespace Fishy
{
	class Shape : Qt3DCore::QEntity
	{
	public:
		virtual ~Shape() = default;
		virtual bool Intersect(const Ray &r, Interaction& isect) const = 0;
	};
}
