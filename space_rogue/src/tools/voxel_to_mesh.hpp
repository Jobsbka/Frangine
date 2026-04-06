#pragma once
#include "../math/math.hpp"
#include "../procgen/voxel_field.hpp"
#include <vector>
#include <string>
#include <cstdint>

// Расширенная вершина для скиннинга
struct SkinnedVertex {
    sVector30 pos;
    sVector30 normal;
    sU32 boneIndices[4];   // индексы костей (0..255)
    sF32 boneWeights[4];   // веса (сумма = 1.0)
};

// Простая кость (для скелета)
struct SimpleBone {
    std::string name;
    sVector31 position;        // локальная позиция относительно центра объекта
    sQuaternion rotation;      // локальное вращение
    sVector31 scale;           // масштаб
    int parent;                // индекс родительской кости (-1 для корня)
};

// Результат работы инструмента
struct VoxelToMeshResult {
    std::vector<SkinnedVertex> vertices;   // вершины с весами
    std::vector<uint32_t> indices;         // индексы треугольников
    std::vector<SimpleBone> bones;         // скелет
};

class VoxelToMeshTool {
public:
    // Основной метод: из воксельного поля -> меш + скелет
    static VoxelToMeshResult generate(const VoxelField& field,
                                      float isoLevel = 0.0f,
                                      bool simplify = true,
                                      int targetTriangleCount = 500,
                                      bool createSkeleton = true);

    // Сохранить результат в бинарный файл (можно загрузить потом в движке)
    static bool saveToFile(const VoxelToMeshResult& result, const std::string& filename);

    // Загрузить из файла (для использования в игре)
    static bool loadFromFile(const std::string& filename, VoxelToMeshResult& outResult);

private:
    // Упрощение сетки с помощью meshoptimizer (если доступна)
    static void simplifyMesh(std::vector<SkinnedVertex>& vertices,
                             std::vector<uint32_t>& indices,
                             int targetTriangles);

    // Построение выпуклой оболочки (очень грубое упрощение)
    static void convexHull(std::vector<SkinnedVertex>& vertices,
                           std::vector<uint32_t>& indices);

    // Создание скелета на основе анализа вершин
    static std::vector<SimpleBone> createSkeleton(const std::vector<SkinnedVertex>& vertices,
                                                  int numBones = 6);

    // Назначение весов вершин к костям (простой метод: по ближайшей кости по направлению от центра)
    static void assignBoneWeights(std::vector<SkinnedVertex>& vertices,
                                  const std::vector<SimpleBone>& bones);
};