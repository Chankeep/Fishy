//
// Created by chankeep on 4/4/2024.
//

#include "../core/Scene.h"
#include "../core/Sampler.h"
#include "FishyThreadPool.h"

#include <utility>

namespace Fishy
{

    FragRenderer::FragRenderer(const vector2 &begin, const vector2 &end,
            std::shared_ptr<Scene> scene,
            std::shared_ptr<Camera> camera,
            std::shared_ptr<Sampler> originalSampler,
            std::shared_ptr<Film> film)
            : begin(begin), end(end), scene(std::move(scene)), camera(std::move(camera)), originalSampler(std::move(originalSampler)), film(std::move(film))
    {
        integrator = std::make_shared<PathIntegrator>();
    }

    void FragRenderer::run()
    {
        resetSignal();
        integrator->Render(begin, end, scene, camera, originalSampler, film);
    }

    void FragRenderer::resetSignal()
    {
        if(isFirstThread)
        {
            isFirstThread = false;
            renderSignal = 0;
        }
    }
}
