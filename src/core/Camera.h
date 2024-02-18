#pragma once

#include "Ray.h"
#include "Film.h"
#include "Fishy.h"

namespace Fishy
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

        virtual Ray GenerateRay(const CameraSample &sample) const = 0;

        Film *film{};
    };


}
