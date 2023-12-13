#pragma once
#include "Ray.h"
#include "Film.h"


namespace fishy
{

	struct CameraSample
	{
		vector2 pFilm;
	};

	class Camera
	{
	public:
		Camera() = default;
		virtual ~Camera() = default;

		virtual float GenerateRay(const CameraSample& sample, Ray* ray) const = 0;

		Film* film;
	};
}
