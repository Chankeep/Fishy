#pragma once
#include "Ray.h"
#include "Interaction.h"


namespace Fishy
{
	class FishyShape : public Qt3DExtras::QSphereMesh
	{
	public:
		virtual ~FishyShape() = default;
		virtual bool Intersect(const Ray &r, Interaction& isect) const = 0;
        virtual void setTransform(Qt3DCore::QTransform* transform) = 0;
	};
}
