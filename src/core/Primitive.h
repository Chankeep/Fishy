//
// Created by chankeep on 12/18/2023.
//
#pragma once

#include "Shape.h"
#include "Interaction.h"
namespace fishy
{

    struct Primitive
    {
        const Shape* shape;

        bool Intersect(Ray& ray, Interaction& isect) const
        {
            bool hit = shape->Intersect(ray, isect);
            if(hit)
            {
                return hit;
            }
        }

    };

} // fishy

