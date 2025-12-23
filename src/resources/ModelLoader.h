#pragma once

#include "Model.h"
#include <memory>
#include <string>

namespace Fishy {

class ResourceManager;

class ModelLoader {
public:
	// Loads a glTF model from the specified path.
	// Uses ResourceManager to load textures if provided.
	static std::shared_ptr<Model> loadModel(const std::string& filepath, ResourceManager* resourceManager = nullptr);
};

} // namespace Fishy
