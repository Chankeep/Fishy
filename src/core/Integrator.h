#pragma once
#include "Scene.h"

namespace fishy
{
	class Integrator
	{
	public:
		Integrator() = default;
		virtual ~Integrator() = default;

		virtual void Render(const Scene& scene) = 0;
	};
}