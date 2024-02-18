#pragma once

#include "Scene.h"
#include "Sampler.h"

namespace Fishy
{
    class Integrator
    {
    public:
        Integrator() = default;
        virtual ~Integrator() = default;

        virtual void Render(Scene &scene, Camera &camera, Sampler &originalSampler, Film &film);

        virtual vector3 Li(Ray &ray, Scene &scene, Sampler &sampler) = 0;
    };

    class PathIntegrator : public Integrator
    {
    public:
        using Integrator::Integrator;
        vector3 Li(Ray &ray, Scene &scene, Sampler &sampler) override{
            return Li(ray, scene, sampler, 0);
        }
        vector3 Li(Ray &ray, Fishy::Scene &scene, Sampler &sampler, int depth);


        int maxDepth = 10;
    };
}