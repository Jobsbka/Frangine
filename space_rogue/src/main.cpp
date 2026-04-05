#include <GLFW/glfw3.h>
#include "render/vulkan_context.hpp"
#include "procgen/perlin.hpp"
#include "procgen/voxel_field.hpp"
#include "render/marching_cubes.hpp"
#include "render/mesh.hpp"
#include <iostream>
#include <thread>   // <-- добавлено для sleep_for
#include <chrono>

int main() {
    // Инициализация окна
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1024, 768, "Space Rogue - Procedural Voxel Terrain", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Vulkan
    VulkanContext vk;
    if (!vk.init(window)) {
        std::cerr << "Failed to initialize Vulkan" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // 1. Генерация воксельного поля с помощью шума Перлина
    const int voxelRes = 64;               // 64x64x64 вокселей
    VoxelField field(voxelRes, voxelRes, voxelRes);
    Perlin3D perlin(12345);               // seed
    float isoLevel = 0.0f;                // изоповерхность там, где поле равно 0
    float scale = 2.5f;                   // частота шума
    field.generate(perlin, isoLevel, scale);

    // 2. Применяем Marching Cubes для получения полигональной сетки
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    MarchingCubes::generate(field, isoLevel, vertices, indices);

    std::cout << "Generated " << vertices.size() << " vertices, "
              << indices.size() / 3 << " triangles" << std::endl;

    if (vertices.empty()) {
        std::cerr << "No geometry generated! Check voxel field or isoLevel." << std::endl;
        // Выходим, но сначала почистим
        vk.cleanup();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // 3. Создаём Mesh и загружаем данные в GPU
    Mesh mesh(vk.getDevice(), vk.getPhysicalDevice());
    mesh.upload(vertices, indices);

    // 4. Обновляем командные буферы Vulkan для отрисовки этого меша
    vk.updateCommandBuffers(mesh);

    // 5. Главный цикл
    auto lastTime = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Здесь можно добавить вращение камеры, но пока оставим статику
        vk.drawFrame();

        // Небольшая задержка для снижения нагрузки CPU
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();
        if (elapsed < 16) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16 - elapsed));
        }
        lastTime = now;
    }

    // Очистка
    mesh.cleanup();
    vk.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}