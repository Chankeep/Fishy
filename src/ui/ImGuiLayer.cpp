#include "ImGuiLayer.h"

#include "core/VulkanContext.h"
#include "core/VulkanDevice.h"
#include "core/Window.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>
#include <vector>

namespace Fishy {

ImGuiLayer::ImGuiLayer(VulkanContext& context, VulkanDevice& device, Window& window, vk::Format renderPassFormat)
	: _context(context), _device(device), _window(window), _renderPassFormat(static_cast<VkFormat>(renderPassFormat)) {
	initVulkanResources();
	initImGui();
}

ImGuiLayer::~ImGuiLayer() {
	// Wait for GPU to finish before cleanup
	_device->waitIdle();

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	if (_descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(static_cast<VkDevice>(*(*_device)), _descriptorPool, nullptr);
	}
}

void ImGuiLayer::initVulkanResources() {
	// Create Descriptor Pool for ImGui
	VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
										 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
										 {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
										 {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
	pool_info.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
	pool_info.pPoolSizes = pool_sizes;

	VkDevice device = static_cast<VkDevice>(*(*_device));
	if (vkCreateDescriptorPool(device, &pool_info, nullptr, &_descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create ImGui descriptor pool!");
	}
}

void ImGuiLayer::initImGui() {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	  // Enable Docking
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForVulkan(_window.getNativeWindow(), true);

	// Setup Dynamic Rendering Info for the new ImGui API (2025/09 changes)
	VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &_renderPassFormat;
	// No depth for UI overlay

	// Zero-clear and setup init info
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.ApiVersion = VK_API_VERSION_1_3;
	init_info.Instance = static_cast<VkInstance>(*(_context.getInstance()));
	init_info.PhysicalDevice = static_cast<VkPhysicalDevice>(*_device.getPhysicalDevice());
	init_info.Device = static_cast<VkDevice>(*(*_device));
	init_info.QueueFamily = _device.getGraphicsQueueFamilyIndex();
	init_info.Queue = static_cast<VkQueue>(*_device.getGraphicsQueue());
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = _descriptorPool;
	init_info.MinImageCount = 2; // Should match swapchain
	init_info.ImageCount = 2;	 // Should match swapchain
	init_info.UseDynamicRendering = true;

	// New API: Pipeline info is now in PipelineInfoMain struct
	init_info.PipelineInfoMain.Subpass = 0;
	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;

	ImGui_ImplVulkan_Init(&init_info);

	// Font texture creation is now automatic in the new ImGui API
	// No need to call ImGui_ImplVulkan_CreateFontsTexture()
}

void ImGuiLayer::newFrame() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiLayer::render(VkCommandBuffer cmd) {
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

} // namespace Fishy
