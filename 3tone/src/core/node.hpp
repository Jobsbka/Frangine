// src/core/node.hpp
#pragma once
#include "arxglue.hpp"
#include "../types/type_system.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include <any>
#include <unordered_map>
#include <typeindex>
#include <iostream>
#include <mutex>
#include <shared_mutex>

namespace arxglue {

class INode {
public:
    virtual ~INode() = default;
    virtual void execute(Context& ctx) = 0;
    virtual ComponentMetadata getMetadata() const = 0;
    virtual void setParameter(const std::string& name, const std::any& value) = 0;
    virtual std::any getParameter(const std::string& name) const = 0;
    virtual void serialize(nlohmann::json& j) const = 0;
    virtual void deserialize(const nlohmann::json& j) = 0;

    void setId(NodeId id) { m_id = id; }
    NodeId getId() const { return m_id; }

    bool isDirty() const { return m_dirty; }
    void setDirty(bool dirty) { m_dirty = dirty; }

    void setCachedOutput(const std::any& output) { m_cachedOutput = output; }
    std::any getCachedOutput() const { return m_cachedOutput; }

    // Получить значение входа с автоматической конвертацией к ожидаемому типу
    std::any getInputValue(Context& ctx, int portIndex) const {
        const auto& inputs = getMetadata().inputs;
        if (portIndex < 0 || portIndex >= static_cast<int>(inputs.size())) {
            return {};
        }
        std::string key = "in_" + std::to_string(m_id) + "_" + std::to_string(portIndex);
        std::shared_lock stateLock(ctx.stateMutex);   // переименовано
        auto it = ctx.state.find(key);
        if (it == ctx.state.end()) {
            return {};
        }
        const std::any& rawValue = it->second;
        const std::type_index& expectedType = inputs[portIndex].type;
        if (std::type_index(rawValue.type()) == expectedType) {
            return rawValue;
        }
        try {
            return TypeSystem::instance().convertTo(rawValue, expectedType);
        } catch (const std::exception& e) {
            std::cerr << "Type conversion error in node " << m_id << " port " << portIndex << ": " << e.what() << std::endl;
            return {};
        }
    }

    void setOutputValue(Context& ctx, int portIndex, const std::any& value) {
        const auto& outputs = getMetadata().outputs;
        if (portIndex >= 0 && portIndex < static_cast<int>(outputs.size())) {
            std::string key = "out_" + std::to_string(m_id) + "_" + std::to_string(portIndex);
            std::unique_lock lock(ctx.stateMutex);
            ctx.state[key] = value;
        }
        ctx.output = value;
    }

    // Проверка, изменились ли входы по сравнению с последним выполнением
    bool areInputsChanged(const Context& ctx) {
        const auto& inputs = getMetadata().inputs;
        if (inputs.empty()) return false;

        std::lock_guard<std::mutex> inputLock(m_inputMutex);
        if (m_lastInputs.size() != inputs.size()) {
            m_lastInputs.resize(inputs.size());
            return true;
        }

        for (size_t i = 0; i < inputs.size(); ++i) {
            std::string key = "in_" + std::to_string(m_id) + "_" + std::to_string(i);
            std::any currentValue;
            {
                std::shared_lock lock(ctx.stateMutex);
                auto it = ctx.state.find(key);
                if (it != ctx.state.end()) {
                    currentValue = it->second;
                }
            }
            if (!TypeSystem::compareAny(currentValue, m_lastInputs[i])) {
                m_lastInputs[i] = currentValue;
                return true;
            }
        }
        return false;
    }

    // Обновить сохранённые значения входов после выполнения
    void updateLastInputs(const Context& ctx) {
        const auto& inputs = getMetadata().inputs;
        std::lock_guard<std::mutex> inputLock(m_inputMutex);   
        m_lastInputs.resize(inputs.size());
        for (size_t i = 0; i < inputs.size(); ++i) {
            std::string key = "in_" + std::to_string(m_id) + "_" + std::to_string(i);
            std::shared_lock stateLock(ctx.stateMutex);        
            auto it = ctx.state.find(key);
            m_lastInputs[i] = (it != ctx.state.end()) ? it->second : std::any{};
        }
    }

    std::type_index getInputPortType(int portIndex) const {
        const auto& inputs = getMetadata().inputs;
        if (portIndex >= 0 && portIndex < static_cast<int>(inputs.size())) {
            return inputs[portIndex].type;
        }
        return typeid(void);
    }

    std::type_index getOutputPortType(int portIndex) const {
        const auto& outputs = getMetadata().outputs;
        if (portIndex >= 0 && portIndex < static_cast<int>(outputs.size())) {
            return outputs[portIndex].type;
        }
        return typeid(void);
    }

    bool areParametersChanged() const {
        size_t currentHash = computeParamsHash();
        if (currentHash != m_lastParamsHash) {
            m_lastParamsHash = currentHash;
            return true;
        }
        return false;
    }

    void clearLastInputs() {
        std::lock_guard<std::mutex> inputLock(m_inputMutex);
        m_lastInputs.clear();
    }

protected:
    virtual size_t computeParamsHash() const { return 0; }

private:
    NodeId m_id = 0;
    bool m_dirty = true;
    std::any m_cachedOutput;
    mutable std::vector<std::any> m_lastInputs;
    mutable std::mutex m_inputMutex;
    mutable size_t m_lastParamsHash = 0;
};

} // namespace arxglue