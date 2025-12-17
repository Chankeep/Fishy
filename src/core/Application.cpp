#include "Application.h"
#include <iostream>

#include "renderer/Vertex.h"

namespace Fishy {

Application::GlfwInitializer::GlfwInitializer() {
	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialize GLFW");
	}
}

Application::GlfwInitializer::~GlfwInitializer() { glfwTerminate(); }

Application::Application()
	: _glfwInitializer(), _context(),
	  _window({.title = "Fishy Engine", .width = 1280, .height = 720}, _context.getInstance()),
	  _device(_context.getInstance(), _window.getSurface()), _resourceManager(_device),
	  _renderer(_device, _window, _resourceManager) {}

Application::~Application() {}

void Application::Run() {
	while (!_window.ShouldClose()) {
		_window.Update();

		const auto& cmd = _renderer.BeginFrame();
		if (*cmd) { // Check if command buffer is valid
			uint32_t imageIndex = _renderer.getCurrentImageIndex();
			int frameIndex = _renderer.getFrameIndex();

			// Update uniform buffer with current MVP matrices
			_renderer.updateUniformBuffer(frameIndex);

			// Transition image to COLOR_ATTACHMENT_OPTIMAL
			_renderer.transitionImageLayout(
				imageIndex, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, {},
				vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eTopOfPipe,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput);

			// Begin dynamic rendering
			vk::ClearValue clearColor =
				vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}));
			const auto& imageViews = _renderer.getSwapChainImageViews();
			vk::Extent2D extent = _renderer.getSwapChainExtent();

			vk::RenderingAttachmentInfo attachmentInfo{.imageView = *imageViews[imageIndex],
													   .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
													   .loadOp = vk::AttachmentLoadOp::eClear,
													   .storeOp = vk::AttachmentStoreOp::eStore,
													   .clearValue = clearColor};

			vk::RenderingInfo renderingInfo{.renderArea = vk::Rect2D{{0, 0}, extent},
											.layerCount = 1,
											.colorAttachmentCount = 1,
											.pColorAttachments = &attachmentInfo};

			cmd.beginRendering(renderingInfo);

			// Bind pipeline
			_renderer.getPipeline().bind(cmd);

			// Set viewport and scissor
			cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width),
											static_cast<float>(extent.height), 0.0f, 1.0f));
			cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

			// Bind vertex and index buffers
			cmd.bindVertexBuffers(0, *_renderer.getVertexBuffer(), {0});
			cmd.bindIndexBuffer(*_renderer.getIndexBuffer(), 0, vk::IndexType::eUint16);

			// Bind descriptor sets
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_renderer.getPipeline().getLayout(), 0,
								   *_renderer.getDescriptorSet(frameIndex), nullptr);

			// Draw indexed
			cmd.drawIndexed(_renderer.getIndexCount(), 1, 0, 0, 0);

			cmd.endRendering();

			// Transition image to PRESENT_SRC
			_renderer.transitionImageLayout(imageIndex, vk::ImageLayout::eColorAttachmentOptimal,
											vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits2::eColorAttachmentWrite,
											{}, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
											vk::PipelineStageFlagBits2::eBottomOfPipe);

			_renderer.EndFrame();
		}
	}

	_device->waitIdle();
}

} // namespace Fishy
