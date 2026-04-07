#pragma once
#include "../core/node.hpp"
#include <memory>
#include <unordered_map>
#include <functional>

namespace arxglue {

class NodeFactory {
public:
    using Creator = std::function<std::unique_ptr<INode>()>;

    static NodeFactory& instance() {
        static NodeFactory inst;
        return inst;
    }

    void registerNode(const std::string& type, Creator creator) {
        m_creators[type] = std::move(creator);
    }

    std::unique_ptr<INode> createNode(const std::string& type) {
        auto it = m_creators.find(type);
        if (it == m_creators.end()) return nullptr;
        return it->second();
    }

private:
    std::unordered_map<std::string, Creator> m_creators;
};

} // namespace arxglue