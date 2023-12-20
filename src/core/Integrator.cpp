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
            film.setPixelColor(x, y, Clamp(L));
        }
    }
}

fishy::vector3 fishy::PathIntegrator::Li(fishy::Ray &ray, fishy::Scene &scene, fishy::Sampler &sampler, int depth)
{
    Interaction isect;
    if(!scene.Intersect(ray, isect))
        return {};
    return vector3(1, 1, 1);

    if(depth > maxDepth)
        return isect.Le();

    auto bs = isect.GetBsdf()->sample(isect.w_o, sampler.Get2D());
    if(bs.f.isNull() || bs.pdf == 0.f)
        return isect.Le();

    if(++depth > 5)
    {
        float maxComponent = GetMaxNum(bs.f);
        if(sampler.Get1D() < maxComponent)
            bs.f = bs.f * (1 / maxComponent);
        else
            return isect.Le();
    }

    Ray wi(isect.position, bs.wi);
    return isect.Le() + (bs.f * Li(wi, scene, sampler, depth) * qAbs(dot(bs.wi, isect.normal) / bs.pdf));
}
