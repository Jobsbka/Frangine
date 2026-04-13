// src/arxglue.hpp
#pragma once

#include <any>
#include <functional>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory>
#include <string>
#include <typeindex>
#include <shared_mutex>

namespace arxglue {

struct Context {
    std::any input;
    std::any output;
    // state теперь защищён мьютексом для потокобезопасного доступа
    std::unordered_map<std::string, std::any> state;
    mutable std::shared_mutex stateMutex;

    // Потокобезопасное получение значения (копия)
    template<typename T>
    T getState(const std::string& key) const {
        std::shared_lock lock(stateMutex);
        auto it = state.find(key);
        if (it != state.end()) {
            return std::any_cast<T>(it->second);
        }
        return T{};
    }

    // Потокобезопасная установка значения
    template<typename T>
    void setState(const std::string& key, T&& value) {
        std::unique_lock lock(stateMutex);
        state[key] = std::forward<T>(value);
    }

    // Проверка наличия ключа
    bool hasState(const std::string& key) const {
        std::shared_lock lock(stateMutex);
        return state.find(key) != state.end();
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