#pragma once
#include "../core/Sampler.h"

namespace Fishy
{
	class RandomSampler : public Sampler
	{
	public:
		using Sampler::Sampler;
		virtual std::unique_ptr<Sampler> Clone() override
		{
			return std::make_unique<RandomSampler>(samplesPerPixel);
		}
		virtual float Get1D() override
		{
			return rng.UniformFloat();
		}
		virtual vector2 Get2D()override
		{
			return rng.UniformFloat2();
		}
		virtual CameraSample GetCameraSample(vector2 pFilm) override
		{
			return { pFilm + rng.UniformFloat2() };
		}
	};
}
