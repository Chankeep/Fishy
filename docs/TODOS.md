# TODOS

## Active

- [ ] Fix PBR material (texture formats, gamma)
- [ ] ImGui integration and input handling
- [ ] KTX2 texture format support
- [ ] Shader reflection system
  - Add `ShaderReflection` struct for vertex inputs and descriptor bindings
  - Parse SPIR-V using SPIRV-Reflect (or custom parser)
  - Auto-generate `PipelineVertexInputStateCreateInfo` and `DescriptorSetLayout`
  - Validate shader/Material compatibility (glTF standard binding order)
  - Cache reflection results in `ResourceManager`

## Archived

- [x] Integrate a simple logging system: use spdlog! (Completed 2025-12-27)
  - Added LogSystem class with spdlog integration
  - Multi-sink output: console (color), file, ringbuffer (for ImGui)
  - Source location tracking using std::source_location
  - Vulkan debug callback integration
