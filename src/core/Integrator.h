#pragma once

#include "Scene.h"
#include "Sampler.h"
#include "../utilities/FishyThreadPool.h"

namespace Fishy
{
    class Integrator : public QObject
    {
    Q_OBJECT
    public:
        Integrator() = default;
        virtual ~Integrator() = default;

        virtual void Render(const vector2 &begin, const vector2 &end, std::shared_ptr<Scene> scene, std::shared_ptr<Camera> camera, std::shared_ptr<Sampler> originalSampler, std::shared_ptr<Film> film);

        virtual vector3 Li(const Ray &ray, std::shared_ptr<Scene> scene, std::shared_ptr<Sampler> sampler) = 0;
    signals:
        void sentMessage(QString);
    };

    class PathIntegrator : public Integrator
    {
    Q_OBJECT
    public:
        using Integrator::Integrator;

        vector3 Li(const Ray &ray, std::shared_ptr<Scene> scene, std::shared_ptr<Sampler> sampler) override
        {
            return Li(ray, scene, sampler, 0);
        }

        vector3 Li(const Ray &ray, std::shared_ptr<Scene> scene, std::shared_ptr<Sampler> sampler, int depth);


    private:
        int maxDepth = 10;
    };
}