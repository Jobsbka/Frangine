#pragma once

#include <cstdint>
#include <memory>
#include <glad/glad.h>

namespace arxglue::render {

class Texture {
public:
    Texture(int width, int height, int channels, const void* data, bool asRenderTarget = false);
    ~Texture();

    // Запрет копирования, разрешено перемещение
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    void bind(unsigned int slot = 0) const;
    void unbind() const;

    GLuint getHandle() const { return m_handle; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    // Загрузка данных (для динамического обновления)
    void updateData(const void* data);

private:
    GLuint m_handle = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
    bool m_isRenderTarget = false;
};

} // namespace arxglue::render