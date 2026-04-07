#pragma once

#include <memory>
#include "texture.hpp"
#include <glad/glad.h>

namespace arxglue::render {

class RenderTarget {
public:
    RenderTarget(int width, int height);
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    void bind() const;
    void unbind() const;

    std::shared_ptr<Texture> getColorTexture() const { return m_colorTexture; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    GLuint m_fbo = 0;
    GLuint m_rbo = 0; // depth-stencil renderbuffer
    std::shared_ptr<Texture> m_colorTexture;
    int m_width;
    int m_height;
};

} // namespace arxglue::render