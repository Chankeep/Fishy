# TODOS

## Active

- [ ] Fix PBR material implementation (currently not working properly).
- [ ] Improve ImGui integration and input handling
- [ ] PBR material: Correct texture format selection and proper gamma operations in shader
- [ ] Integrate KTX2 texture format

## Archived

- [x] Integrate a simple logging system: use spdlog! (Completed 2025-12-27)
  - Added LogSystem class with spdlog integration
  - Multi-sink output: console (color), file, ringbuffer (for ImGui)
  - Source location tracking using std::source_location
  - Vulkan debug callback integration
