#include "Integrator.h"

namespace Fishy
{
    void Integrator::Render(const vector2 &begin, const vector2 &end, std::shared_ptr<Scene> scene, std::shared_ptr<Camera> camera, std::shared_ptr<Sampler> originalSampler, std::shared_ptr<Film> film)
    {
        auto resolution = film->Resolution();
        int height = resolution.y();
        int weight = resolution.x();
        auto TotalPixels = resolution.x() * resolution.y();
        std::shared_ptr<Sampler> sampler = originalSampler->Clone();
        for (int y = begin.y(); y < end.y(); y++)
        {
            auto msg = QString("\rRendering (%1 spp) %2%").arg( sampler->SamplesPerPixel()).arg( 100. * renderSignal / TotalPixels);
            qDebug("\rRendering (%d spp) %5.2f%%", sampler->SamplesPerPixel(), 100. * renderSignal / TotalPixels);
            emit(sentMessage(msg));
            for (int x = begin.x(); x < end.x(); x++)
            {
                vector3 L{};

                sampler->StartPixel();

                do
                {
                    auto cameraSample = sampler->GetCameraSample({float(x), float(y)});
                    auto ray = camera->GenerateRay(cameraSample);

                    L = L + Li(ray, scene, sampler) * (1. / sampler->SamplesPerPixel());
                } while (sampler->StartNextSample());
                film->setPixelColor(x, y, Gamma(L));
                renderSignal++;
            }
        }
    }

    vector3 PathIntegrator::Li(const Ray &ray, std::shared_ptr<Scene> scene, std::shared_ptr<Sampler> sampler, int depth)
    {
        Interaction isect;

        if (!scene->Intersect(ray, isect))
            return {};

        if (depth > maxDepth)
            return isect.Le();

        //采样材质
        auto bs = isect.GetBsdf()->sample(isect.w_o, isect.normal, {sampler->Get1D(), sampler->Get1D()});
        if (isBlack(bs.f) || bs.pdf == 0.f)
            return isect.Le();

        if (++depth > 5) //俄罗斯轮盘赌
        {
            float maxComponent = GetMaxNum(bs.f);
            if (sampler->Get1D() < maxComponent)
                bs.f = bs.f * (1 / maxComponent);
            else
                return isect.Le();
        }

        Ray wi(isect.position, bs.wi);
        return isect.Le() + (bs.f * Li(wi, scene, sampler, depth) * qAbs(dot(bs.wi, isect.normal)) / bs.pdf);
    }

}