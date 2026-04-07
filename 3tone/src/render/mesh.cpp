#include "mesh.hpp"
#include <stdexcept>

namespace arxglue::render {

Mesh::Mesh(const std::vector<float>& vertices,
           const std::vector<uint32_t>& indices,
           const std::vector<int>& attribSizes)
    : m_indexCount(indices.size()) {
    if (vertices.empty() || indices.empty() || attribSizes.empty()) {
        throw std::runtime_error("Mesh: invalid data");
    }

    // Вычисляем суммарный размер одного вершинного элемента
    int stride = 0;
    for (int size : attribSizes) stride += size;
    stride *= sizeof(float);

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    // VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // Настройка атрибутов
    size_t offset = 0;
    for (size_t i = 0; i < attribSizes.size(); ++i) {
        glEnableVertexAttribArray(static_cast<GLuint>(i));
        glVertexAttribPointer(static_cast<GLuint>(i),
                              attribSizes[i],
                              GL_FLOAT,
                              GL_FALSE,
                              stride,
                              reinterpret_cast<void*>(offset * sizeof(float)));
        offset += attribSizes[i];
    }

    glBindVertexArray(0);
}

Mesh::~Mesh() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_vao(other.m_vao), m_vbo(other.m_vbo), m_ebo(other.m_ebo), m_indexCount(other.m_indexCount) {
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
        if (m_ebo) glDeleteBuffers(1, &m_ebo);
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_ebo = other.m_ebo;
        m_indexCount = other.m_indexCount;
        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_ebo = 0;
    }
    return *this;
}

void Mesh::draw() const {
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

} // namespace arxglue::render