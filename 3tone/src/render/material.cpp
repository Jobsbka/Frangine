#include "material.hpp"
#include <stdexcept>
#include <array>

namespace arxglue::render {

Material::Material(std::shared_ptr<Shader> shader) : m_shader(shader) {
    if (!m_shader) throw std::runtime_error("Material: shader is null");
}

void Material::setTexture(const std::string& name, std::shared_ptr<Texture> texture) {
    m_textures[name] = texture;
}

void Material::setParameter(const std::string& name, const std::any& value) {
    m_parameters[name] = value;
}

void Material::apply() const {
    m_shader->bind();
    int texUnit = 0;
    for (const auto& [name, tex] : m_textures) {
        if (tex) {
            tex->bind(texUnit);
            m_shader->setUniform1i(name, texUnit);
            ++texUnit;
        }
    }
    for (const auto& [name, value] : m_parameters) {
        if (value.type() == typeid(int)) {
            m_shader->setUniform1i(name, std::any_cast<int>(value));
        } else if (value.type() == typeid(float)) {
            m_shader->setUniform1f(name, std::any_cast<float>(value));
        } else if (value.type() == typeid(bool)) {
            m_shader->setUniform1i(name, std::any_cast<bool>(value) ? 1 : 0);
        } else if (value.type() == typeid(std::array<float, 3>)) {
            auto arr = std::any_cast<std::array<float, 3>>(value);
            m_shader->setUniform3fv(name, arr.data());
        } else if (value.type() == typeid(std::array<float, 4>)) {
            auto arr = std::any_cast<std::array<float, 4>>(value);
            m_shader->setUniform4fv(name, arr.data());
        } else if (value.type() == typeid(std::array<float, 16>)) {
            auto arr = std::any_cast<std::array<float, 16>>(value);
            m_shader->setUniformMat4(name, arr.data());
        }
    }
}

} // namespace arxglue::render