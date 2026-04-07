#include "executor.hpp"
#include <unordered_set>
#include <queue>
#include <chrono>

namespace arxglue {

Executor::Executor(size_t numThreads) {
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 1;
    m_pool = std::make_unique<ThreadPool>(numThreads);
}

void Executor::execute(Graph& graph, Context& ctx, const std::vector<NodeId>& rootNodes) {
    m_graph = &graph;
    m_ctx = &ctx;

    // Получаем топологический порядок всех узлов
    std::vector<NodeId> allNodes;
    try {
        allNodes = graph.topologicalSort();
    } catch (const std::runtime_error& e) {
        std::cerr << "Graph contains cycle: " << e.what() << std::endl;
        return;
    }

    // Если заданы корневые узлы, оставляем только достижимые от них
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
    // Строим карту зависимостей (кто от кого зависит)
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

    // Очередь готовых к выполнению узлов (зависимостей нет)
    std::queue<NodeId> ready;
    for (auto& [id, cnt] : depCount) {
        if (cnt == 0) ready.push(id);
    }

    std::atomic<int> tasksRemaining = static_cast<int>(order.size());
    std::mutex queueMutex;
    std::mutex ctxMutex; // для потокобезопасного доступа к m_ctx->state (если нужно)

    // Основной цикл: пока есть задачи или узлы в очереди
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
            futures.push_back(m_pool->enqueue([this, id, &depCount, &dependents, &ready, &tasksRemaining, &queueMutex, &ctxMutex]() {
                // Выполняем узел
                executeNode(id);

                // Уменьшаем счётчик оставшихся задач
                tasksRemaining--;

                // Для каждого потомка уменьшаем счётчик зависимостей; если стал 0 – добавляем в ready
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

    // Собираем входные значения из соединений
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

    // Проверяем, нужно ли пересчитывать узел
    bool needExecute = true;
    const auto& metadata = node->getMetadata();
    if (!metadata.volatile_ && !node->isDirty()) {
        // Если не volatile и не грязный, проверяем входы и параметры
        if (!node->areInputsChanged(localCtx) && !node->areParametersChanged()) {
            needExecute = false; // входы и параметры не изменились – используем кэш
        }
    }

    if (needExecute) {
        node->execute(localCtx);
        node->setCachedOutput(localCtx.output);
        node->setDirty(false);
        node->updateLastInputs(localCtx);
    } else {
        // Восстанавливаем выход из кэша (уже есть в node->getCachedOutput)
        // Но также нужно записать выход в контекст, чтобы последующие узлы получили значение
        localCtx.output = node->getCachedOutput();
    }

    // Передаём выходные значения в глобальный контекст (для отладки и дальнейших соединений)
    for (const auto& conn : m_graph->getConnections()) {
        if (conn.srcNode == id) {
            std::string outKey = "out_" + std::to_string(id) + "_" + std::to_string(static_cast<int>(conn.srcPort));
            m_ctx->state[outKey] = localCtx.output;
        }
    }

    // Сохраняем конечный выход графа (если этот узел – последний)
    m_ctx->output = localCtx.output;
}

void Executor::invalidate(NodeId id) {
    if (m_graph) m_graph->invalidateSubgraph(id);
}

} // namespace arxglue