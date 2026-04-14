#pragma once
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <memory>
#include <string>
#include <functional>
#include <any>
#include <unordered_map>
#include <mutex>


namespace arxglue::render {

class Texture;
class Mesh;
class Shader;
class Material;
class RenderTarget;

enum class ShaderType {
    Vertex,
    Fragment,
    Geometry,
    Compute
};

// Интерфейс графического устройства (абстракция над OpenGL)
class GraphicsDevice {
public:
    static GraphicsDevice& instance();

    // Инициализация с окном
    bool initialize(GLFWwindow* window);
    void shutdown();

    // Создание ресурсов
    std::shared_ptr<Texture> createTexture(int width, int height, int channels, const void* data);
    std::shared_ptr<Texture> createRenderTargetTexture(int width, int height);
    std::shared_ptr<Mesh> createMesh(const std::vector<float>& vertices,
                                     const std::vector<uint32_t>& indices,
                                     const std::vector<int>& attribSizes);
    std::shared_ptr<Shader> createShader(const std::string& vertexSrc,
                                         const std::string& fragmentSrc);
    std::shared_ptr<Material> createMaterial(std::shared_ptr<Shader> shader);
    std::shared_ptr<RenderTarget> createRenderTarget(int width, int height);

    // Текущий контекст
    GLFWwindow* getWindow() const { return m_window; }
    void makeCurrent();

    // Очистка экрана
    void clear(float r, float g, float b, float a);

    // Представление текущего фреймбуфера (swap buffers)
    void swapBuffers();

    // Установка области вывода
    void setViewport(int x, int y, int width, int height);

private:
    GraphicsDevice() = default;
    ~GraphicsDevice() = default;

    GLFWwindow* m_window = nullptr;
    bool m_initialized = false;
};

} // namespace arxglue::render