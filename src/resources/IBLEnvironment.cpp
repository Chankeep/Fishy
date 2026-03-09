#include "IBLEnvironment.h"

#include <filesystem>
#include <fstream>

namespace Fishy {

std::unique_ptr<IBLEnvironment> IBLEnvironment::load(const VulkanDevice& device, const std::string& directory) {
	auto env = std::make_unique<IBLEnvironment>();

	std::string irradiancePath = directory + "/diffuse.ktx2";
	std::string prefilteredPath = directory + "/specular.ktx2";
	std::string brdfCachePath = directory + "/brdf_lut.bin";

	FISHY_LOG_TRACE("Loading IBL environment from: {}", directory);

	// Load irradiance map
	if (std::filesystem::exists(irradiancePath)) {
		env->irradianceMap = std::make_unique<CubemapTexture>(device, irradiancePath);
	} else {
		FISHY_LOG_ERROR("Irradiance map not found: {}", irradiancePath);
		throw std::runtime_error("Missing irradiance map: " + irradiancePath);
	}

	// Load pre-filtered specular map
	if (std::filesystem::exists(prefilteredPath)) {
		env->prefilteredMap = std::make_unique<CubemapTexture>(device, prefilteredPath);
		env->prefilteredMipLevels = env->prefilteredMap->getMipLevels();
	} else {
		FISHY_LOG_ERROR("Pre-filtered map not found: {}", prefilteredPath);
		throw std::runtime_error("Missing pre-filtered map: " + prefilteredPath);
	}

	// Load or generate BRDF LUT
	env->brdfLUT = loadOrGenerateBRDFLUT(device, brdfCachePath);

	FISHY_LOG_INFO("IBL environment loaded successfully ({} specular mip levels)", env->prefilteredMipLevels);

	return env;
}

bool IBLEnvironment::hasCachedBRDFLUT(const std::string& cachePath) { return std::filesystem::exists(cachePath); }

std::unique_ptr<Texture> IBLEnvironment::loadOrGenerateBRDFLUT(const VulkanDevice& device,
															   const std::string& cachePath) {
	constexpr int BYTES_PER_PIXEL = 4; // R16G16 = 4 bytes per pixel

	if (hasCachedBRDFLUT(cachePath)) {
		// Load from cache
		FISHY_LOG_TRACE("Loading cached BRDF LUT from: {}", cachePath);

		std::ifstream file(cachePath, std::ios::binary | std::ios::ate);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open BRDF LUT cache: " + cachePath);
		}

		size_t fileSize = static_cast<size_t>(file.tellg());

		// Derive texture size from file size (assuming square texture)
		// fileSize = width * height * 4, and width == height
		// So: fileSize = size^2 * 4  =>  size = sqrt(fileSize / 4)
		size_t pixelCount = fileSize / BYTES_PER_PIXEL;
		int lutSize = static_cast<int>(std::sqrt(pixelCount));

		// Validate that the file represents a valid square texture
		if (lutSize * lutSize * BYTES_PER_PIXEL != fileSize || lutSize <= 0) {
			FISHY_LOG_WARN("BRDF LUT cache has invalid size ({} bytes). Regenerating...", fileSize);
		} else {
			FISHY_LOG_TRACE("BRDF LUT cache: {} bytes -> {}x{} texture", fileSize, lutSize, lutSize);

			std::vector<uint8_t> data(fileSize);
			file.seekg(0);
			file.read(reinterpret_cast<char*>(data.data()), fileSize);

			if (!file) {
				FISHY_LOG_WARN("Failed to read all bytes from BRDF LUT cache. Regenerating...");
			} else {
				// Create texture from raw data (RG16F format = 4 bytes per pixel)
				return std::make_unique<Texture>(device, data.data(), lutSize, lutSize, vk::Format::eR16G16Sfloat);
			}
		}
	}

	// Generate BRDF LUT (CPU fallback - compute shader would be faster)
	constexpr int LUT_SIZE = 512;
	FISHY_LOG_INFO("Generating BRDF LUT ({}x{})...", LUT_SIZE, LUT_SIZE);

	std::vector<uint16_t> lutData(LUT_SIZE * LUT_SIZE * 2); // RG16

	// Split-sum approximation computation
	// Based on Epic's Real Shading in Unreal Engine 4
	for (int y = 0; y < LUT_SIZE; y++) {
		float roughness = (y + 0.5f) / LUT_SIZE;
		for (int x = 0; x < LUT_SIZE; x++) {
			float NdotV = (x + 0.5f) / LUT_SIZE;
			NdotV = std::max(NdotV, 0.001f); // Avoid division by zero

			// Integrate BRDF over hemisphere
			float a = roughness * roughness;
			float a2 = a * a;

			// Numerical integration (32 samples)
			float A = 0.0f;
			float B = 0.0f;
			constexpr int SAMPLE_COUNT = 32;

			for (int i = 0; i < SAMPLE_COUNT; i++) {
				for (int j = 0; j < SAMPLE_COUNT; j++) {
					// Hammersley sample
					float u = (i + 0.5f) / SAMPLE_COUNT;
					float v = (j + 0.5f) / SAMPLE_COUNT;

					// Importance sample GGX
					constexpr float PI = 3.14159265358979323846f;
					float phi = 2.0f * PI * u;
					float cosTheta = sqrtf((1.0f - v) / (1.0f + (a2 - 1.0f) * v));
					float sinTheta = sqrtf(1.0f - cosTheta * cosTheta);

					// Half vector in tangent space
					float Hx = sinTheta * cosf(phi);
					float Hy = sinTheta * sinf(phi);
					float Hz = cosTheta;

					// View vector (N = (0, 0, 1))
					float Vz = NdotV;
					float Vx = sqrtf(1.0f - Vz * Vz);

					// Light direction = reflect(-V, H)
					float VdotH = Vx * Hx + Vz * Hz;
					float Lx = 2.0f * VdotH * Hx - Vx;
					float Lz = 2.0f * VdotH * Hz - Vz;
					float NdotL = std::max(Lz, 0.0f);
					float NdotH = std::max(Hz, 0.0f);
					VdotH = std::max(VdotH, 0.0f);

					if (NdotL > 0.0f) {
						// Geometry term (Smith GGX)
						float k = a / 2.0f;
						float G1_V = NdotV / (NdotV * (1.0f - k) + k);
						float G1_L = NdotL / (NdotL * (1.0f - k) + k);
						float G = G1_V * G1_L;

						float G_Vis = (G * VdotH) / (NdotH * NdotV);
						float Fc = powf(1.0f - VdotH, 5.0f);

						A += (1.0f - Fc) * G_Vis;
						B += Fc * G_Vis;
					}
				}
			}

			A /= (SAMPLE_COUNT * SAMPLE_COUNT);
			B /= (SAMPLE_COUNT * SAMPLE_COUNT);

			// Convert to float16 (half precision)
			// Using simple conversion - might lose some precision
			auto floatToHalf = [](float value) -> uint16_t {
				// Simplified float to half conversion
				uint32_t bits = *reinterpret_cast<uint32_t*>(&value);
				uint32_t sign = (bits >> 16) & 0x8000;
				int32_t expo = ((bits >> 23) & 0xFF) - 127 + 15;
				uint32_t mant = bits & 0x7FFFFF;

				if (expo <= 0) {
					return static_cast<uint16_t>(sign);
				} else if (expo >= 31) {
					return static_cast<uint16_t>(sign | 0x7C00);
				}
				return static_cast<uint16_t>(sign | (expo << 10) | (mant >> 13));
			};

			lutData[(y * LUT_SIZE + x) * 2 + 0] = floatToHalf(std::clamp(A, 0.0f, 1.0f));
			lutData[(y * LUT_SIZE + x) * 2 + 1] = floatToHalf(std::clamp(B, 0.0f, 1.0f));
		}
	}

	// Save to cache
	std::filesystem::create_directories(std::filesystem::path(cachePath).parent_path());
	std::ofstream file(cachePath, std::ios::binary);
	if (file.is_open()) {
		file.write(reinterpret_cast<const char*>(lutData.data()), lutData.size() * sizeof(uint16_t));
		FISHY_LOG_TRACE("Saved BRDF LUT cache to: {}", cachePath);
	}

	// Create texture from generated data
	return std::make_unique<Texture>(device, reinterpret_cast<const unsigned char*>(lutData.data()), LUT_SIZE, LUT_SIZE,
									 vk::Format::eR16G16Sfloat);
}

} // namespace Fishy
