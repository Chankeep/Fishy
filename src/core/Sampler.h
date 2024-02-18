#pragma once

#include "Fishy.h"
#include "Camera.h"


namespace Fishy
{

    class RNG
    {
    public:
        RNG() = default;

        float UniformFloat()
        {
            return float01Dist(rngEngine);
        }

        vector2 UniformFloat2()
        {
            return { UniformFloat(), UniformFloat() };
        }
    private:
        std::random_device rd;
        std::mt19937_64 rngEngine = std::mt19937_64(rd());
        std::uniform_real_distribution<double> float01Dist{ 0, 1 };
    };


    class Sampler
    {
    public:
        virtual ~Sampler() = default;
        Sampler(int samplesPerPixel) : samplesPerPixel(samplesPerPixel) {}

        virtual int SamplesPerPixel()
        {
            return samplesPerPixel;
        }

        virtual std::unique_ptr<Sampler> Clone() = 0;

        virtual void StartPixel()
        {
            currentSampleIndex = 0;
        }

        virtual bool StartNextSample()
        {
            currentSampleIndex += 1;
            return currentSampleIndex < samplesPerPixel;
        }

        virtual float Get1D() = 0;
        virtual vector2 Get2D() = 0;
        virtual CameraSample GetCameraSample(vector2 pFilm) = 0;


    protected:
        RNG rng{};

        int samplesPerPixel{};
        int currentSampleIndex{};
    };
}
