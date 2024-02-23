#pragma once

#include "Scene.h"
#include "Sampler.h"

namespace Fishy
{
    class Integrator : public QObject
    {
        Q_OBJECT
    public:
        Integrator() = default;
        virtual ~Integrator() = default;

        virtual void Render(Scene *scene, Camera &camera, Sampler &originalSampler, Film *film);

        virtual vector3 Li(Ray &ray, Scene *scene, Sampler &sampler) = 0;
    signals:
        void sentMessage(QString);
    };

    class PathIntegrator : public Integrator
    {
        Q_OBJECT
    public:
        using Integrator::Integrator;
        vector3 Li(Ray &ray, Scene *scene, Sampler &sampler) override{
            return Li(ray, scene, sampler, 0);
        }
        vector3 Li(Ray &ray, Scene *scene, Sampler &sampler, int depth);

    private:
        int maxDepth = 10;
    };
}