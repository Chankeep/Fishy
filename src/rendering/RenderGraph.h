#pragma once

#include "IRenderPass.h"

#include <memory>
#include <vector>

namespace Fishy {

/**
 * @brief Simple Render Graph container for sequential pass execution.
 *
 * Manages a collection of render passes and executes them in order.
 * Phase 1 implementation: sequential execution with std::vector.
 * Future: DAG-based scheduling with automatic barrier insertion.
 */
class RenderGraph {
public:
	RenderGraph() = default;
	~RenderGraph() = default;

	// Non-copyable, moveable
	RenderGraph(const RenderGraph&) = delete;
	RenderGraph& operator=(const RenderGraph&) = delete;
	RenderGraph(RenderGraph&&) = default;
	RenderGraph& operator=(RenderGraph&&) = default;

	/**
	 * @brief Add a render pass to the graph.
	 * @tparam PassT The concrete pass type (must derive from IRenderPass).
	 * @tparam Args Constructor argument types.
	 * @param args Arguments forwarded to PassT constructor.
	 * @return Reference to the created pass.
	 */
	template <typename PassT, typename... Args>
		requires std::derived_from<PassT, IRenderPass>
	PassT& addPass(Args&&... args) {
		auto pass = std::make_unique<PassT>(std::forward<Args>(args)...);
		PassT& ref = *pass;
		_passes.push_back(std::move(pass));
		return ref;
	}

	template <typename PassT> [[nodiscard]] PassT* getPass() {
		for (auto& pass : _passes) {
			if (auto* p = dynamic_cast<PassT*>(pass.get()))
				return p;
		}
		return nullptr;
	}

	/**
	 * @brief Execute all registered passes in order.
	 * @param ctx Context containing borrowed data for rendering.
	 * @param registry ECS registry for entity queries.
	 */
	void execute(RenderGraphContext& ctx, entt::registry& registry) {
		for (auto& pass : _passes) {
			pass->execute(ctx, registry);
		}
	}

	/**
	 * @brief Get the number of registered passes.
	 */
	[[nodiscard]] size_t getPassCount() const { return _passes.size(); }

	/**
	 * @brief Clear all registered passes.
	 */
	void clear() { _passes.clear(); }

private:
	std::vector<std::unique_ptr<IRenderPass>> _passes;
};

} // namespace Fishy
