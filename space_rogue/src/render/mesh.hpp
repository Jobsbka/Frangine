#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "marching_cubes.hpp"   // для Vertex

class Mesh {
public:
    Mesh(VkDevice device, VkPhysicalDevice physicalDevice);
    ~Mesh();

    // Загрузить данные вершин и индексов в GPU
    void upload(const std::vector<Vertex>& vertices,
                const std::vector<uint32_t>& indices);

    // Нарисовать меш (должен быть вызван внутри активного render pass)
    void draw(VkCommandBuffer commandBuffer) const;

    // Освободить ресурсы
    void cleanup();

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    uint32_t indexCount;

    // Вспомогательная функция создания буфера
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory);
};