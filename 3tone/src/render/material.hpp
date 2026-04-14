#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <any>
#include "shader.hpp"
#include "texture.hpp"

namespace arxglue::render {

class Material {
public:
    explicit Material(std::shared_ptr<Shader> shader);
    ~Material() = default;

    void setTexture(const std::string& name, std::shared_ptr<Texture> texture);
    void setParameter(const std::string& name, const std::any& value);
    void apply() const; // Применяет шейдер и устанавливает uniform'ы

    std::shared_ptr<Shader> getShader() const { return m_shader; }

private:
    std::shared_ptr<Shader> m_shader;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
    std::unordered_map<std::string, std::any> m_parameters;
};

} // namespace arxglue::render