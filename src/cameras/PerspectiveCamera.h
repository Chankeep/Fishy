#pragma once
#include "../core/Camera.h"

namespace Fishy
{
class PerspectiveCamera : public Camera
    {
    public:
        PerspectiveCamera(const vector3 &position, const vector3 &center, const vector3 &up, float fov, vector2 resolution)
        {
            //ray tracing initialize
            this->position = position;
            this->center = center;
            this->up = up;
            this->resolution = resolution;
            this->fov = fov;
            const float tan_fov = qTan(qDegreesToRadians(fov) / 2);

            right = cross(this->up, center).normalized() * tan_fov * Aspect_ratio();
            this->up = cross(center, right).normalized() * tan_fov;

        }

        virtual Ray GenerateRay(const CameraSample &sample) const override
        {
            const vector3 rayDirection = center
                                         + right * (sample.pFilm.x() / resolution.x() - 0.5)
                                         + up * (0.5 - sample.pFilm.y() / resolution.y());

            return {position, rayDirection.normalized()};
        }

        float Aspect_ratio() const
        {
            return resolution.x() / resolution.y();
        }


    };

}
