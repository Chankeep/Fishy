#include "Integrator.h"


void fishy::Integrator::Render(fishy::Scene &scene, fishy::Camera &camera, fishy::Sampler &originalSampler, fishy::Film &film)
{
    auto resolution = film.Resolution();
    int height = resolution.y();
    int weight = resolution.x();

    for (int y = 0; y < height; y++)
    {
        std::unique_ptr<Sampler> sampler = originalSampler.Clone();
        qDebug("\rRendering (%d spp) %5.2f%%", sampler->SamplesPerPixel(), 100. * y / (height - 1));
        for (int x = 0; x < weight; x++)
        {
            vector3 L{};

            sampler->StartPixel();

            do
            {
                auto cameraSample = sampler->GetCameraSample({float(x), float(y)});
                auto ray = camera.GenerateRay(cameraSample);

                L = L + Li(ray, scene, *sampler) * (1. / sampler->SamplesPerPixel());
            }
            while(sampler->StartNextSample());

            film.add_color(x + weight * y, Clamp(L));
        }
    }
}

fishy::vector3 fishy::PathIntegrator::Li(fishy::Ray &ray, fishy::Scene &scene, fishy::Sampler &sampler, int depth)
{
    return fishy::vector3();
}
