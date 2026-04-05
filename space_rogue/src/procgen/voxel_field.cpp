#include "voxel_field.hpp"

VoxelField::VoxelField(int sizeX, int sizeY, int sizeZ)
    : sx(sizeX), sy(sizeY), sz(sizeZ)
{
    data.resize(sx * sy * sz);
}

void VoxelField::generate(const Perlin3D& perlin, float isoValue, float scale) {
    for (int x = 0; x < sx; ++x) {
        float fx = (float(x) / (sx - 1)) * 2.0f - 1.0f;
        for (int y = 0; y < sy; ++y) {
            float fy = (float(y) / (sy - 1)) * 2.0f - 1.0f;
            for (int z = 0; z < sz; ++z) {
                float fz = (float(z) / (sz - 1)) * 2.0f - 1.0f;
                float val = perlin.noise(fx * scale, fy * scale, fz * scale);
                data[index(x,y,z)] = val - isoValue;
            }
        }
    }
}

float VoxelField::get(int x, int y, int z) const {
    return data[index(x,y,z)];
}