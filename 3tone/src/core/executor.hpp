#pragma once
#include "graph.hpp"
#include "../threading/thread_pool.hpp"
#include <atomic>
#include <memory>

namespace arxglue {

class Executor {
public:
    explicit Executor(size_t numThreads = 0);
    ~Executor() = default;

    void execute(Graph& graph, Context& ctx, const std::vector<NodeId>& rootNodes = {});
    void invalidate(NodeId id);

private:
    std::unique_ptr<ThreadPool> m_pool;
    Graph* m_graph = nullptr;
    Context* m_ctx = nullptr;

    void runTopologically(const std::vector<NodeId>& order);
    void executeNode(NodeId id);
};

} // namespace arxglue