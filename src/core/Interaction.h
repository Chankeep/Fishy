#pragma once

#include <complex>

#include "common.h"
#include "BSDF.h"

namespace fishy
{
    class Interaction
    {
    public:
        Interaction() = default;

        Interaction(const vector3 &position, const vector3 &normal, const vector3 &w_o)
                : position(position), normal(normal), w_o(w_o)
        {
        }


        vector3 position;
        vector3 normal;
        vector3 w_o;

        const BSDF *GetBsdf()
        {
            return bsdf.get();
        }

        vector3 Le() const
        {
            return emission;
        }

//    private:
        vector3 emission{};
        std::unique_ptr<BSDF> bsdf{};
    };
}
