//
// Created by chankeep on 12/21/2023.
//

#ifndef FISHY_TRAPEZOIDALSAMPLER_H
#define FISHY_TRAPEZOIDALSAMPLER_H

#include "../core/Sampler.h"
namespace Fishy
{
    class TrapezoidalSampler : public Sampler
    {
    public:
        using Sampler::Sampler;

        int SamplesPerPixel() override
        {
            return samplesPerPixel * SubPixelNum;
        }

        std::unique_ptr<Sampler> Clone() override
        {
            return std::make_unique<TrapezoidalSampler>(samplesPerPixel);
        }

    public:
        void StartPixel() override
        {
            Sampler::StartPixel();
            currentSubPixelIndex = 0;
        }

        bool StartNextSample() override
        {
            currentSampleIndex += 1;
            if (currentSampleIndex < samplesPerPixel)
            {
                return true;
            }
            else if (currentSampleIndex == samplesPerPixel)
            {
                currentSampleIndex = 0;
                currentSubPixelIndex += 1;

                return currentSubPixelIndex < SubPixelNum;
            }
            else
            {
                return false;
            }
        }

    public:
        float Get1D() override
        {
            return rng.UniformFloat();
        }

        vector2 Get2D() override
        {
            return rng.UniformFloat2();
        }

        CameraSample GetCameraSample(vector2 pFilm) override
        {
            int subPixelX = currentSubPixelIndex % 2;
            int subPixelY = currentSubPixelIndex / 2;

            float random1 = 2 * rng.UniformFloat();
            float random2 = 2 * rng.UniformFloat();

            // uniform dist [0, 1) => triangle dist [-1, 1)
            float deltaX = random1 < 1 ? sqrt(random1) - 1 : 1 - sqrt(2 - random1);
            float deltaY = random2 < 1 ? sqrt(random2) - 1 : 1 - sqrt(2 - random2);

            vector2 samplePoint
                    {
                            static_cast<float>((subPixelX + deltaX + 0.5) / 2),
                            static_cast<float>((subPixelY + deltaY + 0.5) / 2)
                    };

            return {pFilm + samplePoint};
        }

    private:
        static constexpr int SubPixelNum = 4; // 2x2

        int currentSubPixelIndex{};
    };
}

#endif //FISHY_TRAPEZOIDALSAMPLER_H
