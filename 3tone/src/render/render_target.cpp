#include "render_target.hpp"
#include <stdexcept>

namespace arxglue::render {

RenderTarget::RenderTarget(int width, int height)
    : m_width(width), m_height(height) {
    // Создаем текстуру цвета
    m_colorTexture = std::make_shared<Texture>(width, height, 4, nullptr, true);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Прикрепляем текстуру
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           m_colorTexture->getHandle(), 0);

    // Создаем renderbuffer для глубины и трафарета
    glGenRenderbuffers(1, &m_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("RenderTarget: Framebuffer incomplete");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

RenderTarget::~RenderTarget() {
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    if (m_rbo) glDeleteRenderbuffers(1, &m_rbo);
}

void RenderTarget::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
}

void RenderTarget::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace arxglue::render