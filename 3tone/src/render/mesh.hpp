#pragma once

#include <vector>
#include <cstdint>
#include <glad/glad.h>

namespace arxglue::render {

class Mesh {
public:
    Mesh(const std::vector<float>& vertices,
         const std::vector<uint32_t>& indices,
         const std::vector<int>& attribSizes);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void draw() const;

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    size_t m_indexCount = 0;
};

} // namespace arxglue::render