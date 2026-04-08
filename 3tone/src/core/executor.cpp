// src/core/executor.cpp
#include "executor.hpp"
#include <unordered_set>
#include <queue>
#include <chrono>
#include <iostream>

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
        std::vector<NodeId> filtered;
        for (NodeId n : allNodes) {
            if (reachable.count(n)) {
                filtered.push_back(n);
            }
        }
        allNodes = std::move(filtered);
    }

    // Строим карты зависимостей
    std::unordered_map<NodeId, int> depCount;
    std::unordered_map<NodeId, std::vector<NodeId>> dependents;

    for (NodeId id : allNodes) {
        depCount[id] = 0;
        dependents[id] = {};
    }

    for (NodeId id : allNodes) {
        for (NodeId dep : graph.getDependencies(id)) {
            if (depCount.find(dep) != depCount.end()) {
                depCount[id]++;
                dependents[dep].push_back(id);
            }
        }
    }

    // Очередь готовых узлов
    std::queue<NodeId> readyQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<int> tasksRemaining(static_cast<int>(allNodes.size()));

    for (const auto& pair : depCount) {
        if (pair.second == 0) {
            readyQueue.push(pair.first);
        }
    }

    // Обработчик одного узла (вызывается в пуле)
    auto processNode = [this, &depCount, &dependents, &readyQueue, &queueMutex, &cv, &tasksRemaining](NodeId id) {
        try {
            executeNode(id);
        } catch (const std::exception& e) {
            std::cerr << "Exception in node " << id << ": " << e.what() << std::endl;
        }

        int remaining = --tasksRemaining;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            for (NodeId child : dependents[id]) {
                int& cnt = depCount[child];
                if (--cnt == 0) {
                    readyQueue.push(child);
                }
            }
        }
        cv.notify_one();

        if (remaining == 0) {
            cv.notify_all();
        }
    };

    // Диспетчеризация
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        while (tasksRemaining > 0) {
            while (!readyQueue.empty()) {
                NodeId id = readyQueue.front();
                readyQueue.pop();
                m_pool->enqueue([processNode, id]() {
                    processNode(id);
                });
            }
            if (tasksRemaining > 0) {
                cv.wait(lock);
            }
        }
    }
}

void Executor::executeNode(NodeId id) {
    INode* node = m_graph->getNode(id);
    if (!node) return;

    // 1. Сбор входных значений: копируем из кэшей предшественников в state под защитой unique_lock
    {
        std::unique_lock lock(m_ctx->stateMutex);
        for (const auto& conn : m_graph->getConnections()) {
            if (conn.dstNode == id) {
                INode* srcNode = m_graph->getNode(conn.srcNode);
                if (srcNode) {
                    std::any value = srcNode->getCachedOutput();
                    if (value.has_value()) {
                        std::string inKey = "in_" + std::to_string(id) + "_" + std::to_string(static_cast<int>(conn.dstPort));
                        m_ctx->state[inKey] = value;
                    }
                }
            }
        }
    }

    // 2. Проверка необходимости выполнения (чтение state происходит внутри areInputsChanged, который теперь использует shared_lock)
    bool needExecute = true;
    const auto& metadata = node->getMetadata();
    std::any cached = node->getCachedOutput();
    if (!metadata.volatile_ && !node->isDirty() && cached.has_value()) {
        if (!node->areInputsChanged(*m_ctx) && !node->areParametersChanged()) {
            needExecute = false;
        }
    }

    // 3. Выполнение или восстановление
    if (needExecute) {
        node->execute(*m_ctx); // внутри узлы используют getInputValue/setOutputValue с блокировками
        node->setCachedOutput(m_ctx->output);
        node->setDirty(false);
        node->updateLastInputs(*m_ctx);
    } else {
        m_ctx->output = cached;
    }

    // 4. Сохранение выходов (уже делается внутри setOutputValue, но на всякий случай можно оставить для out_ ключей)
    // Фактически узлы сами вызывают setOutputValue, который пишет out_ ключи. 
    // Дополнительно ничего не нужно.
}

void Executor::invalidate(NodeId id) {
    if (m_graph) m_graph->invalidateSubgraph(id);
}

} // namespace arxglue