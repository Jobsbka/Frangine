#pragma once
#include "../math/math.hpp"
#include "perlin.hpp"      // <-- обязательно
#include <vector>

class VoxelField {
public:
    VoxelField(int sizeX, int sizeY, int sizeZ);
    void generate(const Perlin3D& perlin, float isoValue, float scale = 1.0f);
    float get(int x, int y, int z) const;
    int getSizeX() const { return sx; }
    int getSizeY() const { return sy; }
    int getSizeZ() const { return sz; }
    const std::vector<float>& getData() const { return data; }
private:
    int sx, sy, sz;
    std::vector<float> data;
    int index(int x, int y, int z) const { return x + y * sx + z * sx * sy; }
};