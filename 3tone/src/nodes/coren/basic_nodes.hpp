#pragma once
#include "../../core/node.hpp"
#include "../../assets/asset_manager.hpp"
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <string_view>   // <-- добавить
#include <type_traits>   // <-- добавить

namespace arxglue {

void registerBasicNodes();

template<typename T>
class ConstantNode : public INode {
public:
    ConstantNode(T value) : m_value(value) {}

    void execute(Context& ctx) override {
        setOutputValue(ctx, 0, m_value);
    }

    ComponentMetadata getMetadata() const override {
        return {"Constant" + std::string(typeid(T).name()),
                {}, // нет входов
                {{"out", typeid(T)}},
                true, false};
    }

    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "value") {
            m_value = std::any_cast<T>(value);
            setDirty(true);
        }
    }

    std::any getParameter(const std::string& name) const override {
        if (name == "value") return m_value;
        return {};
    }

    void serialize(nlohmann::json& j) const override {
        if constexpr (std::is_same_v<T, int>) {
            j["type"] = "ConstantInt";
        } else if constexpr (std::is_same_v<T, float>) {
            j["type"] = "ConstantFloat";
        } else {
            j["type"] = "ConstantUnknown";
        }
        j["params"]["value"] = m_value;
    }

    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params") && j["params"].contains("value")) {
            m_value = j["params"]["value"].get<T>();
        }
    }

protected:
    size_t computeParamsHash() const override {
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::string_view bytes(reinterpret_cast<const char*>(&m_value), sizeof(T));
            return std::hash<std::string_view>{}(bytes);
        } else {
            return 0;
        }
    }

private:
    T m_value;
};

class AddNode : public INode {
public:
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override {
        return {"Add",
                {{"a", typeid(int)}, {"b", typeid(int)}},
                {{"out", typeid(int)}},
                true, false};
    }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "Add"; }
    void deserialize(const nlohmann::json&) override {}
};

class LoadTextureNode : public INode {
public:
    LoadTextureNode(const std::string& path = "") : m_path(path) {}
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override {
        return {"LoadTexture",
                {},
                {{"texture", typeid(std::shared_ptr<TextureAsset>)}},
                false, false};
    }
    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "path") m_path = std::any_cast<std::string>(value);
    }
    std::any getParameter(const std::string& name) const override {
        if (name == "path") return m_path;
        return {};
    }
    void serialize(nlohmann::json& j) const override {
        j["type"] = "LoadTexture";
        j["params"]["path"] = m_path;
    }
    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params") && j["params"].contains("path"))
            m_path = j["params"]["path"].get<std::string>();
    }
private:
    std::string m_path;
};

class PerlinNoiseNode : public INode {
public:
    PerlinNoiseNode(float scale = 0.01f, int octaves = 4) : m_scale(scale), m_octaves(octaves) {}
    void execute(Context& ctx) override;
    ComponentMetadata getMetadata() const override {
        return {"PerlinNoise",
                {},
                {{"value", typeid(float)}},
                false, false}; // volatile (случайное значение)
    }
    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "scale") m_scale = std::any_cast<float>(value);
        else if (name == "octaves") m_octaves = std::any_cast<int>(value);
    }
    std::any getParameter(const std::string& name) const override {
        if (name == "scale") return m_scale;
        if (name == "octaves") return m_octaves;
        return {};
    }
    void serialize(nlohmann::json& j) const override {
        j["type"] = "PerlinNoise";
        j["params"]["scale"] = m_scale;
        j["params"]["octaves"] = m_octaves;
    }
    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params")) {
            if (j["params"].contains("scale")) m_scale = j["params"]["scale"].get<float>();
            if (j["params"].contains("octaves")) m_octaves = j["params"]["octaves"].get<int>();
        }
    }
private:
    float m_scale;
    int m_octaves;
};

class FloatConsumerNode : public INode {
public:
    void execute(Context& ctx) override {
        float val = std::any_cast<float>(getInputValue(ctx, 0));
        setOutputValue(ctx, 0, val);
        static std::mutex coutMutex;
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[FloatConsumer] Received: " << val << std::endl;
    }

    ComponentMetadata getMetadata() const override {
        return {"FloatConsumer",
                {{"in", typeid(float), true}},
                {{"out", typeid(float)}},
                true, false};
    }

    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "FloatConsumer"; }
    void deserialize(const nlohmann::json&) override {}
};

class SlowNode : public INode {
public:
    SlowNode() : m_delayMs(100) {}
    explicit SlowNode(int delayMs) : m_delayMs(delayMs) {}

    void execute(Context& ctx) override {
        // Искусственная задержка для демонстрации параллелизма
        std::this_thread::sleep_for(std::chrono::milliseconds(m_delayMs));

        // Ожидаем на входе целое число, на выходе возвращаем его же
        int val = std::any_cast<int>(getInputValue(ctx, 0));
        setOutputValue(ctx, 0, val);
    }

    ComponentMetadata getMetadata() const override {
        return {
            "SlowNode",
            {{"in", typeid(int), true}},
            {{"out", typeid(int)}},
            true,   // pure (результат зависит только от входа)
            false   // не volatile
        };
    }

    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "delay") {
            m_delayMs = std::any_cast<int>(value);
        }
    }

    std::any getParameter(const std::string& name) const override {
        if (name == "delay") return m_delayMs;
        return {};
    }

    void serialize(nlohmann::json& j) const override {
        j["type"] = "SlowNode";
        j["params"]["delay"] = m_delayMs;
    }

    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params") && j["params"].contains("delay")) {
            m_delayMs = j["params"]["delay"].get<int>();
        }
    }

private:
    int m_delayMs;
};

} // namespace arxglue