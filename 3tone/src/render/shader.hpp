#pragma once
#include <string>
#include <unordered_map>
#include <glad/glad.h>

namespace arxglue::render {

class Shader {
public:
    Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void bind() const;
    void unbind() const;

    void setUniform1i(const std::string& name, int value);
    void setUniform1f(const std::string& name, float value);
    void setUniformMat4(const std::string& name, const float* mat4);
    void setUniform3fv(const std::string& name, const float* vec3);
    void setUniform4fv(const std::string& name, const float* vec4);

    GLuint getHandle() const { return m_program; }
    GLint getUniformLocation(const std::string& name) const;

private:
    GLuint m_program = 0;
    mutable std::unordered_map<std::string, GLint> m_uniformCache;

    GLuint compileShader(GLenum type, const std::string& source, std::string& outError);
};

} // namespace arxglue::render