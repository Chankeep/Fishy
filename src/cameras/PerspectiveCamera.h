#pragma once

#include "../core/Camera.h"

namespace Fishy
{
    class PerspectiveCamera : public Camera
    {
    public:
        PerspectiveCamera(const vector3 &position, const vector3 &direction, const vector3 &up, float fov, const vector2& resolution)
        {
            //ray tracing initialize
            this->position = position;
            this->front = direction;
            this->up = up;

            this->resolution = resolution;
            aspectRatio = resolution.x() / resolution.y();
            this->fov = fov;
            const float tan_fov = qTan(qDegreesToRadians(fov) / 2);

            right = cross(this->up, front).normalized() * tan_fov * aspectRatio;
            this->up = cross(front, right).normalized() * tan_fov;

        }

        virtual Ray GenerateRay(const CameraSample &sample) const override
        {
            const vector3 rayDirection = front
                                         + right * (sample.pFilm.x() / resolution.x() - 0.5)
                                         + up * (0.5 - sample.pFilm.y() / resolution.y());

            return {position, rayDirection.normalized()};
        }


    };

    static std::shared_ptr<Camera> createPerspectiveCamera(const vector3 &position, const vector3 &director, const vector3 &up,
            const int &fov, const vector2 &resolution)
    {
        return std::make_shared<PerspectiveCamera>(position, director, up, fov, resolution);
    }

}
