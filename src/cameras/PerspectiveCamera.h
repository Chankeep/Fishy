#pragma once
#include "../core/Camera.h"

namespace fishy
{
    class PerspectiveCamera : public Camera
    {
    public:
        PerspectiveCamera(const vector3 &position, const vector3 &direction, const vector3 &up, float fov, vector2 resolution)
                : position(position), direction(direction), up(up), resolution(resolution)
        {
	        const float tan_fov = qTan(qDegreesToRadians(fov) / 2);

            right = cross(this->up, direction).normalized() * tan_fov * Aspect_ratio();
            this->up = cross(direction, right).normalized() * tan_fov;
        }

        virtual Ray GenerateRay(const CameraSample &sample) const override
        {
	        const vector3 rayDirection = direction
                                   + right * (sample.pFilm.x() / resolution.x() - 0.5)
                                   + up * (0.5 - sample.pFilm.y() / resolution.y());

            return {position, rayDirection.normalized()};
        }

        float Aspect_ratio() const
        {
            return resolution.x() / resolution.y();
        }

        vector3 position;
        vector3 direction;
        vector3 right;
        vector3 up;
        vector2 resolution;
    };

}
