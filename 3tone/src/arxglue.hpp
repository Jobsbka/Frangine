#pragma once

#include <any>
#include <functional>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory>
#include <string>
#include <typeindex>

namespace arxglue {

struct Context {
    std::any input;
    std::any output;
    std::unordered_map<std::string, std::any> state;

    template<typename T>
    T& getState(const std::string& key) {
        return std::any_cast<T&>(state[key]);
    }

    template<typename T>
    void setState(const std::string& key, T&& value) {
        state[key] = std::forward<T>(value);
    }
};

using Component = std::function<void(Context&)>;

struct MultiConnection {
    Component source;
    std::vector<Component> targets;
    std::optional<std::function<void(Context&)>> transformer;
};

template<typename... Targets>
MultiConnection connect(Component source, Targets... targets) {
    return MultiConnection{source, {targets...}, std::nullopt};
}

template<typename... Targets>
MultiConnection connect(Component source, std::function<void(Context&)> transformer, Targets... targets) {
    return MultiConnection{source, {targets...}, transformer};
}

using NodeId = uint32_t;

struct PortInfo {
    std::string name;
    std::type_index type;
    bool required = true;
};

struct ComponentMetadata {
    std::string name;
    std::vector<PortInfo> inputs;
    std::vector<PortInfo> outputs;
    bool pure = false;
    bool volatile_ = false;
    bool threadSafe = false;
};

} // namespace arxglue