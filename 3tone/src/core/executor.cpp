#include "executor.hpp"
#include <unordered_set>
#include <queue>

namespace arxglue {

Executor::Executor(size_t numThreads) {
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 1;
    m_pool = std::make_unique<ThreadPool>(numThreads);
}

void Executor::execute(Graph& graph, Context& ctx, const std::vector<NodeId>& rootNodes) {
    m_graph = &graph;
    m_ctx = &ctx;

    std::vector<NodeId> allNodes = graph.topologicalSort();

    if (!rootNodes.empty()) {
        std::unordered_set<NodeId> reachable;
        std::queue<NodeId> q;
        for (NodeId r : rootNodes) { q.push(r); reachable.insert(r); }
        while (!q.empty()) {
            NodeId cur = q.front(); q.pop();
            for (NodeId dep : graph.getDependencies(cur)) {
                if (!reachable.count(dep)) {
                    reachable.insert(dep);
                    q.push(dep);
                }
            }
        }
        std::vector<NodeId> filtered;
        for (NodeId n : allNodes) {
            if (reachable.count(n)) filtered.push_back(n);
        }
        allNodes = filtered;
    }

    runTopologically(allNodes);
}

void Executor::runTopologically(const std::vector<NodeId>& order) {
    std::unordered_map<NodeId, int> depCount;
    std::unordered_map<NodeId, std::vector<NodeId>> dependents;

    for (NodeId id : order) {
        depCount[id] = 0;
        dependents[id].clear();
    }
    for (NodeId id : order) {
        for (NodeId dep : m_graph->getDependencies(id)) {
            if (depCount.count(dep)) {
                depCount[id]++;
                dependents[dep].push_back(id);
            }
        }
    }

    std::queue<NodeId> ready;
    for (auto& [id, cnt] : depCount) {
        if (cnt == 0) ready.push(id);
    }

    std::atomic<int> tasksRemaining = static_cast<int>(order.size());
    std::mutex queueMutex;

    while (!ready.empty() || tasksRemaining > 0) {
        std::vector<NodeId> batch;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            while (!ready.empty()) {
                batch.push_back(ready.front());
                ready.pop();
            }
        }
        if (batch.empty()) {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            continue;
        }

        std::vector<std::future<void>> futures;
        for (NodeId id : batch) {
            futures.push_back(m_pool->enqueue([this, id, &depCount, &dependents, &ready, &tasksRemaining, &queueMutex]() {
                executeNode(id);
                tasksRemaining--;
                for (NodeId dep : dependents[id]) {
                    if (--depCount[dep] == 0) {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        ready.push(dep);
                    }
                }
            }));
        }
        for (auto& f : futures) f.get();
    }
}

void Executor::executeNode(NodeId id) {
    INode* node = m_graph->getNode(id);
    if (!node) return;

    if (!node->isDirty() && !node->getMetadata().volatile_) {
        return;
    }

    Context localCtx;
    for (const auto& conn : m_graph->getConnections()) {
        if (conn.dstNode == id) {
            INode* srcNode = m_graph->getNode(conn.srcNode);
            if (srcNode) {
                std::any value = srcNode->getCachedOutput();
                std::string inKey = "in_" + std::to_string(id) + "_" + std::to_string(static_cast<int>(conn.dstPort));
                localCtx.state[inKey] = value;
            }
        }
    }

    node->execute(localCtx);
    node->setCachedOutput(localCtx.output);
    node->setDirty(false);

    for (const auto& conn : m_graph->getConnections()) {
        if (conn.srcNode == id) {
            std::string outKey = "out_" + std::to_string(id) + "_" + std::to_string(static_cast<int>(conn.srcPort));
            m_ctx->state[outKey] = localCtx.output;
        }
    }

    m_ctx->output = localCtx.output;
}

void Executor::invalidate(NodeId id) {
    if (m_graph) m_graph->invalidateSubgraph(id);
}

} // namespace arxglue