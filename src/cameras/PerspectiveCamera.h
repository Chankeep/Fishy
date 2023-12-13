#pragma once
#include "../core/Camera.h"

namespace fishy
{
	class PerspectiveCamera : public Camera
	{
	public:
		PerspectiveCamera(const vector3& position, const vector3& direction, const vector3& up, float fov) {}
	};
}
