#pragma once
#include "../arxglue.hpp"
#include "../types/type_system.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include <any>
#include <unordered_map>
#include <typeindex>
#include <iostream>

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
        auto it = ctx.state.find(key);
        if (it == ctx.state.end()) {
            return {};
        }
        const std::any& rawValue = it->second;
        const std::type_index& expectedType = inputs[portIndex].type;
        // Сравниваем типы, обернув rawValue.type() в std::type_index
        if (std::type_index(rawValue.type()) == expectedType) {
            return rawValue;
        }
        // Иначе пытаемся сконвертировать через TypeSystem
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
            ctx.state[key] = value;
        }
        ctx.output = value;
    }

    // Проверка, изменились ли входы по сравнению с последним выполнением
    bool areInputsChanged(const Context& ctx) {
        const auto& inputs = getMetadata().inputs;
        if (inputs.empty()) return true; // Нет входов – всегда менять не нужно, но для чистоты считаем неизменным

        if (m_lastInputs.size() != inputs.size()) {
            m_lastInputs.resize(inputs.size());
            return true; // первый раз – считаем изменённым
        }

        for (size_t i = 0; i < inputs.size(); ++i) {
            std::string key = "in_" + std::to_string(m_id) + "_" + std::to_string(i);
            auto it = ctx.state.find(key);
            std::any currentValue = (it != ctx.state.end()) ? it->second : std::any{};
            // Сравниваем значения
            if (!compareAny(currentValue, m_lastInputs[i])) {
                m_lastInputs[i] = currentValue;
                return true;
            }
        }
        return false;
    }

    // Обновить сохранённые значения входов после выполнения
    void updateLastInputs(const Context& ctx) {
        const auto& inputs = getMetadata().inputs;
        m_lastInputs.resize(inputs.size());
        for (size_t i = 0; i < inputs.size(); ++i) {
            std::string key = "in_" + std::to_string(m_id) + "_" + std::to_string(i);
            auto it = ctx.state.find(key);
            m_lastInputs[i] = (it != ctx.state.end()) ? it->second : std::any{};
        }
    }

private:
    NodeId m_id = 0;
    bool m_dirty = true;
    std::any m_cachedOutput;
    mutable std::vector<std::any> m_lastInputs;

    // Безопасное сравнение двух std::any
    static bool compareAny(const std::any& a, const std::any& b) {
        if (!a.has_value() && !b.has_value()) return true;
        if (!a.has_value() || !b.has_value()) return false;
        // Сравниваем типы через std::type_index
        if (std::type_index(a.type()) != std::type_index(b.type())) return false;
        const std::type_info& ti = a.type();
        if (ti == typeid(int)) {
            return std::any_cast<int>(a) == std::any_cast<int>(b);
        } else if (ti == typeid(float)) {
            return std::any_cast<float>(a) == std::any_cast<float>(b);
        } else if (ti == typeid(std::string)) {
            return std::any_cast<std::string>(a) == std::any_cast<std::string>(b);
        } else if (ti == typeid(bool)) {
            return std::any_cast<bool>(a) == std::any_cast<bool>(b);
        }
        // Для сложных типов (текстуры, меши) считаем разными, чтобы избежать ошибочного кэширования
        return false;
    }
};

} // namespace arxglue