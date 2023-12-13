#pragma once

#include "common.h"
#include "Camera.h"

class RNG
{
public:
	RNG(int seed = 1234) {}

	float UniformFloat()
	{
		return float01Dist(rngEngine);
	}

	fishy::vector2 UniformFloat2()
	{
		return { UniformFloat(), UniformFloat() };
	}
private:
	std::mt19937_64 rngEngine;
	std::uniform_real_distribution<float> float01Dist{ static_cast<float>(0), static_cast<float>(1) };
};


namespace fishy
{
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
