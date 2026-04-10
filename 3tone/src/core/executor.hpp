// src/core/executor.hpp
#pragma once
#include "graph.hpp"
#include "../threading/thread_pool.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <future>
#include <unordered_map>

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

    struct NodeSync {
        std::promise<void> promise;
        std::shared_future<void> future;
        std::atomic<bool> scheduled{false};
    };
    std::unordered_map<NodeId, std::unique_ptr<NodeSync>> m_syncMap;
    std::mutex m_syncMutex;

    void executeNodeAsync(NodeId id);
    void waitForDependencies(NodeId id);
    std::shared_future<void> getFuture(NodeId id);
    void tryScheduleDependents(NodeId id);
};

} // namespace arxglue