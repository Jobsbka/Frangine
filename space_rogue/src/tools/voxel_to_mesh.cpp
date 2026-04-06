#include "voxel_to_mesh.hpp"
#include "../render/marching_cubes.hpp"   // для генерации исходной сетки
#include <cmath>
#include <algorithm>
#include <fstream>
#include <limits>

#ifdef USE_MESHOPTIMIZER
#include <meshoptimizer.h>
#endif

// ============================================================================
// Публичные методы
// ============================================================================

VoxelToMeshResult VoxelToMeshTool::generate(const VoxelField& field,
                                            float isoLevel,
                                            bool simplify,
                                            int targetTriangleCount,
                                            bool createSkeleton)
{
    VoxelToMeshResult result;

    // 1. Получаем исходную сетку через Marching Cubes
    std::vector<Vertex> rawVertices;
    std::vector<uint32_t> rawIndices;
    MarchingCubes::generate(field, isoLevel, rawVertices, rawIndices);

    if (rawVertices.empty()) {
        return result; // пустой результат
    }

    // Конвертируем Vertex -> SkinnedVertex (пока без весов)
    result.vertices.resize(rawVertices.size());
    for (size_t i = 0; i < rawVertices.size(); ++i) {
        result.vertices[i].pos = rawVertices[i].pos;
        result.vertices[i].normal = rawVertices[i].normal;
        for (int j = 0; j < 4; ++j) {
            result.vertices[i].boneIndices[j] = 0;
            result.vertices[i].boneWeights[j] = 0.0f;
        }
    }
    result.indices = std::move(rawIndices);

    // 2. Упрощение сетки (децимация)
    if (simplify && targetTriangleCount > 0 && result.indices.size() / 3 > (size_t)targetTriangleCount) {
#ifdef USE_MESHOPTIMIZER
        simplifyMesh(result.vertices, result.indices, targetTriangleCount);
#else
        // Если meshoptimizer не подключён, используем выпуклую оболочку
        convexHull(result.vertices, result.indices);
#endif
    }

    // 3. Создание скелета
    if (createSkeleton) {
        // Определяем количество костей: можно вычислить автоматически на основе размера
        int numBones = std::max(4, (int)std::cbrt(result.vertices.size()) / 10);
        result.bones = createSkeleton(result.vertices, numBones);
        assignBoneWeights(result.vertices, result.bones);
    }

    return result;
}

bool VoxelToMeshTool::saveToFile(const VoxelToMeshResult& result, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;

    // Магия для версионирования (можно увеличивать при изменении формата)
    uint32_t magic = 0x564D544D; // "VMTM"
    uint32_t version = 1;
    file.write((char*)&magic, 4);
    file.write((char*)&version, 4);

    // Сохраняем вершины
    uint32_t vertexCount = (uint32_t)result.vertices.size();
    file.write((char*)&vertexCount, 4);
    file.write((char*)result.vertices.data(), vertexCount * sizeof(SkinnedVertex));

    // Сохраняем индексы
    uint32_t indexCount = (uint32_t)result.indices.size();
    file.write((char*)&indexCount, 4);
    file.write((char*)result.indices.data(), indexCount * sizeof(uint32_t));

    // Сохраняем кости
    uint32_t boneCount = (uint32_t)result.bones.size();
    file.write((char*)&boneCount, 4);
    for (const auto& bone : result.bones) {
        uint32_t nameLen = (uint32_t)bone.name.size();
        file.write((char*)&nameLen, 4);
        file.write(bone.name.c_str(), nameLen);
        file.write((char*)&bone.position, sizeof(sVector31));
        file.write((char*)&bone.rotation, sizeof(sQuaternion));
        file.write((char*)&bone.scale, sizeof(sVector31));
        file.write((char*)&bone.parent, sizeof(int));
    }

    return true;
}

bool VoxelToMeshTool::loadFromFile(const std::string& filename, VoxelToMeshResult& outResult) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return false;

    uint32_t magic, version;
    file.read((char*)&magic, 4);
    file.read((char*)&version, 4);
    if (magic != 0x564D544D || version != 1) return false;

    uint32_t vertexCount;
    file.read((char*)&vertexCount, 4);
    outResult.vertices.resize(vertexCount);
    file.read((char*)outResult.vertices.data(), vertexCount * sizeof(SkinnedVertex));

    uint32_t indexCount;
    file.read((char*)&indexCount, 4);
    outResult.indices.resize(indexCount);
    file.read((char*)outResult.indices.data(), indexCount * sizeof(uint32_t));

    uint32_t boneCount;
    file.read((char*)&boneCount, 4);
    outResult.bones.resize(boneCount);
    for (auto& bone : outResult.bones) {
        uint32_t nameLen;
        file.read((char*)&nameLen, 4);
        bone.name.resize(nameLen);
        file.read(&bone.name[0], nameLen);
        file.read((char*)&bone.position, sizeof(sVector31));
        file.read((char*)&bone.rotation, sizeof(sQuaternion));
        file.read((char*)&bone.scale, sizeof(sVector31));
        file.read((char*)&bone.parent, sizeof(int));
    }

    return true;
}

// ============================================================================
// Приватные методы
// ============================================================================

#ifdef USE_MESHOPTIMIZER
void VoxelToMeshTool::simplifyMesh(std::vector<SkinnedVertex>& vertices,
                                   std::vector<uint32_t>& indices,
                                   int targetTriangles)
{
    // Целевое количество треугольников
    size_t targetIndexCount = targetTriangles * 3;
    if (indices.size() <= targetIndexCount) return;

    // Подготовка буфера для упрощённых индексов
    std::vector<uint32_t> simplifiedIndices(indices.size());
    float targetError = 0.01f; // допустимая ошибка
    size_t newIndexCount = meshopt_simplify(simplifiedIndices.data(),
                                            indices.data(),
                                            indices.size(),
                                            &vertices[0].pos.x,
                                            vertices.size(),
                                            sizeof(SkinnedVertex),
                                            targetIndexCount,
                                            targetError);

    simplifiedIndices.resize(newIndexCount);
    indices = std::move(simplifiedIndices);

    // После упрощения некоторые вершины могли стать неиспользуемыми – удалим их
    std::vector<bool> used(vertices.size(), false);
    for (uint32_t idx : indices) used[idx] = true;
    std::vector<uint32_t> remap(vertices.size(), ~0u);
    std::vector<SkinnedVertex> newVertices;
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (used[i]) {
            remap[i] = (uint32_t)newVertices.size();
            newVertices.push_back(vertices[i]);
        }
    }
    for (uint32_t& idx : indices) idx = remap[idx];
    vertices = std::move(newVertices);
}
#endif

void VoxelToMeshTool::convexHull(std::vector<SkinnedVertex>& vertices,
                                 std::vector<uint32_t>& indices)
{
    // Реализация QuickHull для трёхмерных точек
    // Это упрощённая версия – для реального использования лучше взять библиотеку (qhull)
    // Здесь приведён псевдокод, который нужно заменить на реальную реализацию.
    // Для начала можно оставить исходную сетку без изменений, если convex hull не критичен.
    // Ниже – заглушка, чтобы код компилировался.
    // В реальном проекте подключите qhull или напишите свой алгоритм.
    (void)vertices; (void)indices;
    // Пока ничего не делаем
}

std::vector<SimpleBone> VoxelToMeshTool::createSkeleton(const std::vector<SkinnedVertex>& vertices,
                                                        int numBones)
{
    std::vector<SimpleBone> bones;
    if (vertices.empty()) return bones;

    // Вычисляем центр масс всех вершин
    sVector31 center(0,0,0);
    for (const auto& v : vertices) {
        center = center + v.pos;
    }
    center = center * (1.0f / vertices.size());

    // Для простоты создаём кости, направленные из центра по осям (X+, X-, Y+, Y-, Z+, Z-)
    // Можно добавить больше костей, разбивая сферу равномерно.
    int axes[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (int i = 0; i < std::min(numBones, 6); ++i) {
        SimpleBone bone;
        bone.name = "bone_" + std::to_string(i);
        bone.position = center + sVector31(axes[i][0], axes[i][1], axes[i][2]) * 0.8f; // смещение от центра
        bone.rotation = sQuaternion(); // единичный кватернион
        bone.scale = sVector31(1,1,1);
        bone.parent = (i == 0) ? -1 : 0; // все кости привязаны к первой (корневой)
        bones.push_back(bone);
    }

    // Если нужно больше костей, можно добавить промежуточные (например, для конечностей)
    // Для шара/астероида 6 костей достаточно.

    return bones;
}

void VoxelToMeshTool::assignBoneWeights(std::vector<SkinnedVertex>& vertices,
                                        const std::vector<SimpleBone>& bones)
{
    if (bones.empty()) return;

    // Для каждой вершины находим ближайшую кость (по направлению от центра)
    // Сначала вычислим общий центр (если ещё не сохранён)
    sVector31 center(0,0,0);
    for (const auto& v : vertices) center = center + v.pos;
    center = center * (1.0f / vertices.size());

    for (auto& vert : vertices) {
        sVector30 dir = (vert.pos - center).Normalize();
        int bestBone = -1;
        float bestDot = -1.0f;
        for (size_t b = 0; b < bones.size(); ++b) {
            sVector30 boneDir = (bones[b].position - center).Normalize();
            float dot = dir ^ boneDir;
            if (dot > bestDot) {
                bestDot = dot;
                bestBone = (int)b;
            }
        }
        if (bestBone >= 0) {
            vert.boneIndices[0] = (uint32_t)bestBone;
            vert.boneWeights[0] = 1.0f;
            for (int j = 1; j < 4; ++j) {
                vert.boneIndices[j] = 0;
                vert.boneWeights[j] = 0.0f;
            }
        }
    }
}