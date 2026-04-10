// src/core/executor.cpp
#include "executor.hpp"
#include <unordered_set>
#include <queue>
#include <iostream>

namespace arxglue {

Executor::Executor(size_t numThreads) {
    if (numThreads == 0) {
        m_synchronous = true;
        return;
    }
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 1;
    m_pool = std::make_unique<ThreadPool>(numThreads);
}

void Executor::execute(Graph& graph, Context& ctx, const std::vector<NodeId>& rootNodes) {
    m_graph = &graph;
    m_ctx = &ctx;

    std::vector<NodeId> allNodes;
    try {
        allNodes = graph.topologicalSort();
    } catch (const std::runtime_error& e) {
        std::cerr << "Graph contains cycle: " << e.what() << std::endl;
        return;
    }

    std::unordered_set<NodeId> reachable;
    if (!rootNodes.empty()) {
        std::queue<NodeId> q;
        for (NodeId r : rootNodes) {
            q.push(r);
            reachable.insert(r);
        }
        while (!q.empty()) {
            NodeId cur = q.front(); q.pop();
            for (NodeId dep : graph.getDependencies(cur)) {
                if (reachable.find(dep) == reachable.end()) {
                    reachable.insert(dep);
                    q.push(dep);
                }
            }
        }
    } else {
        for (NodeId id : allNodes) reachable.insert(id);
    }

    std::vector<NodeId> sortedReachable;
    for (NodeId id : allNodes) {
        if (reachable.count(id)) sortedReachable.push_back(id);
    }

    if (m_synchronous) {
        // Синхронное выполнение в текущем потоке
        for (NodeId id : sortedReachable) {
            INode* node = graph.getNode(id);
            if (!node) continue;

            // Копируем входы от предшественников
            {
                std::unique_lock lock(ctx.stateMutex);
                for (const auto& conn : graph.getConnections()) {
                    if (conn.dstNode == id) {
                        INode* srcNode = graph.getNode(conn.srcNode);
                        if (srcNode) {
                            std::any value = srcNode->getCachedOutput();
                            if (value.has_value()) {
                                std::string inKey = "in_" + std::to_string(id) + "_" + std::to_string(conn.dstPort);
                                ctx.state[inKey] = value;
                            }
                        }
                    }
                }
            }

            const auto& metadata = node->getMetadata();
            bool needExecute = true;
            std::any cached = node->getCachedOutput();
            if (!metadata.volatile_ && !node->isDirty() && cached.has_value()) {
                if (!node->areInputsChanged(ctx) && !node->areParametersChanged()) {
                    needExecute = false;
                }
            }

            if (needExecute) {
                node->execute(ctx);
                node->setCachedOutput(ctx.output);
                node->setDirty(false);
                node->updateLastInputs(ctx);
            } else {
                ctx.output = cached;
            }
        }
        return;
    }

    // Параллельное выполнение (существующий код)
    {
        std::lock_guard<std::mutex> lock(m_syncMutex);
        m_syncMap.clear();
        for (NodeId id : sortedReachable) {
            auto sync = std::make_unique<NodeSync>();
            sync->future = sync->promise.get_future().share();
            m_syncMap[id] = std::move(sync);
        }
    }

    for (NodeId id : sortedReachable) {
        if (graph.getDependencies(id).empty()) {
            auto it = m_syncMap.find(id);
            if (it != m_syncMap.end() && !it->second->scheduled.exchange(true)) {
                m_pool->enqueue([this, id] { executeNodeAsync(id); });
            }
        }
    }

    std::vector<std::shared_future<void>> rootFutures;
    {
        std::lock_guard<std::mutex> lock(m_syncMutex);
        for (NodeId r : rootNodes) {
            auto it = m_syncMap.find(r);
            if (it != m_syncMap.end()) {
                rootFutures.push_back(it->second->future);
            }
        }
    }
    for (auto& fut : rootFutures) {
        fut.wait();
    }
}

void Executor::executeNodeAsync(NodeId id) {
    INode* node = m_graph->getNode(id);
    if (!node) {
        auto it = m_syncMap.find(id);
        if (it != m_syncMap.end()) {
            it->second->promise.set_value();
        }
        return;
    }

    waitForDependencies(id);

    {
        std::unique_lock lock(m_ctx->stateMutex);
        for (const auto& conn : m_graph->getConnections()) {
            if (conn.dstNode == id) {
                INode* srcNode = m_graph->getNode(conn.srcNode);
                if (srcNode) {
                    std::any value = srcNode->getCachedOutput();
                    if (value.has_value()) {
                        std::string inKey = "in_" + std::to_string(id) + "_" + std::to_string(conn.dstPort);
                        m_ctx->state[inKey] = value;
                    }
                }
            }
        }
    }

    bool needExecute = true;
    const auto& metadata = node->getMetadata();
    std::any cached = node->getCachedOutput();
    if (!metadata.volatile_ && !node->isDirty() && cached.has_value()) {
        if (!node->areInputsChanged(*m_ctx) && !node->areParametersChanged()) {
            needExecute = false;
        }
    }

    if (needExecute) {
        node->execute(*m_ctx);
        node->setCachedOutput(m_ctx->output);
        node->setDirty(false);
        node->updateLastInputs(*m_ctx);
    } else {
        m_ctx->output = cached;
    }

    {
        auto it = m_syncMap.find(id);
        if (it != m_syncMap.end()) {
            it->second->promise.set_value();
        }
    }

    tryScheduleDependents(id);
}

void Executor::tryScheduleDependents(NodeId id) {
    for (NodeId dep : m_graph->getDependents(id)) {
        bool allDepsReady = true;
        for (NodeId pred : m_graph->getDependencies(dep)) {
            auto fut = getFuture(pred);
            if (fut.valid() && fut.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                allDepsReady = false;
                break;
            }
        }
        if (allDepsReady) {
            auto it = m_syncMap.find(dep);
            if (it != m_syncMap.end() && !it->second->scheduled.exchange(true)) {
                m_pool->enqueue([this, dep] { executeNodeAsync(dep); });
            }
        }
    }
}

void Executor::waitForDependencies(NodeId id) {
    for (NodeId pred : m_graph->getDependencies(id)) {
        auto fut = getFuture(pred);
        if (fut.valid()) {
            fut.wait();
        }
    }
}

std::shared_future<void> Executor::getFuture(NodeId id) {
    std::lock_guard<std::mutex> lock(m_syncMutex);
    auto it = m_syncMap.find(id);
    if (it != m_syncMap.end()) {
        return it->second->future;
    }
    return {};
}

void Executor::invalidate(NodeId id) {
    if (m_graph) {
        m_graph->invalidateSubgraph(id);
        std::lock_guard<std::mutex> lock(m_syncMutex);
        std::queue<NodeId> q;
        std::unordered_set<NodeId> visited;
        q.push(id);
        while (!q.empty()) {
            NodeId cur = q.front(); q.pop();
            if (visited.count(cur)) continue;
            visited.insert(cur);
            auto it = m_syncMap.find(cur);
            if (it != m_syncMap.end()) {
                it->second->scheduled = false;
                it->second->promise = std::promise<void>();
                it->second->future = it->second->promise.get_future().share();
            }
            for (NodeId dep : m_graph->getDependents(cur)) {
                q.push(dep);
            }
        }
    }
}

} // namespace arxglue