#include "texture.hpp"
#include <stdexcept>
#include <cstring>

namespace arxglue::render {

Texture::Texture(int width, int height, int channels, const void* data, bool asRenderTarget)
    : m_width(width), m_height(height), m_channels(channels), m_isRenderTarget(asRenderTarget) {
    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_2D, m_handle);

    GLenum format = GL_RGBA;
    GLenum internalFormat = GL_RGBA8;
    switch (channels) {
        case 1: format = GL_RED; internalFormat = GL_R8; break;
        case 3: format = GL_RGB; internalFormat = GL_RGB8; break;
        case 4: format = GL_RGBA; internalFormat = GL_RGBA8; break;
        default: throw std::runtime_error("Unsupported number of channels");
    }

    if (asRenderTarget) {
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::~Texture() {
    if (m_handle) {
        glDeleteTextures(1, &m_handle);
    }
}

Texture::Texture(Texture&& other) noexcept
    : m_handle(other.m_handle), m_width(other.m_width), m_height(other.m_height),
      m_channels(other.m_channels), m_isRenderTarget(other.m_isRenderTarget) {
    other.m_handle = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (m_handle) glDeleteTextures(1, &m_handle);
        m_handle = other.m_handle;
        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;
        m_isRenderTarget = other.m_isRenderTarget;
        other.m_handle = 0;
    }
    return *this;
}

void Texture::bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_handle);
}

void Texture::unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::updateData(const void* data) {
    bind();
    GLenum format = GL_RGBA;
    switch (m_channels) {
        case 1: format = GL_RED; break;
        case 3: format = GL_RGB; break;
        case 4: format = GL_RGBA; break;
    }
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    unbind();
}

} // namespace arxglue::render