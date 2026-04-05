#include "perlin.hpp"
#include <cstdlib>
#include <cmath>

const sVector30 Perlin3D::gradients[16] = {
    {1,1,0}, {-1,1,0}, {1,-1,0}, {-1,-1,0},
    {1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1},
    {0,1,1}, {0,-1,1}, {0,1,-1}, {0,-1,-1},
    {1,1,0}, {-1,1,0}, {0,-1,1}, {0,1,-1}
};

Perlin3D::Perlin3D(unsigned int seed) {
    initPermutation(seed);
}

void Perlin3D::initPermutation(unsigned int seed) {
    unsigned char p[256];
    for (int i = 0; i < 256; ++i) p[i] = i;
    std::srand(seed);
    for (int i = 255; i > 0; --i) {
        int j = std::rand() % (i + 1);
        unsigned char t = p[i]; p[i] = p[j]; p[j] = t;
    }
    for (int i = 0; i < 256; ++i) perm[i] = p[i];
    for (int i = 0; i < 256; ++i) perm[256 + i] = p[i];
}

float Perlin3D::grad(int hash, float x, float y, float z) {   // <-- убрали const
    int h = hash & 15;
    const sVector30& g = gradients[h];
    return g.x * x + g.y * y + g.z * z;
}

float Perlin3D::noise(float x, float y, float z) const {
    int ix = (int)std::floor(x - 0.5f);
    int iy = (int)std::floor(y - 0.5f);
    int iz = (int)std::floor(z - 0.5f);
    float fx = x - ix;
    float fy = y - iy;
    float fz = z - iz;
    
    auto fade = [](float t) -> float {
        return t * t * t * (t * (t * 6 - 15) + 10);
    };
    float u = fade(fx);
    float v = fade(fy);
    float w = fade(fz);
    
    int X = ix & 255;
    int Y = iy & 255;
    int Z = iz & 255;
    
    float n000 = grad(perm[perm[perm[X] + Y] + Z], fx,     fy,     fz);
    float n100 = grad(perm[perm[perm[X+1] + Y] + Z], fx-1.0f, fy,     fz);
    float n010 = grad(perm[perm[perm[X] + Y+1] + Z], fx,     fy-1.0f, fz);
    float n110 = grad(perm[perm[perm[X+1] + Y+1] + Z], fx-1.0f, fy-1.0f, fz);
    float n001 = grad(perm[perm[perm[X] + Y] + Z+1], fx,     fy,     fz-1.0f);
    float n101 = grad(perm[perm[perm[X+1] + Y] + Z+1], fx-1.0f, fy,     fz-1.0f);
    float n011 = grad(perm[perm[perm[X] + Y+1] + Z+1], fx,     fy-1.0f, fz-1.0f);
    float n111 = grad(perm[perm[perm[X+1] + Y+1] + Z+1], fx-1.0f, fy-1.0f, fz-1.0f);
    
    float nx0 = n000 * (1-u) + n100 * u;
    float nx1 = n010 * (1-u) + n110 * u;
    float ny0 = nx0 * (1-v) + nx1 * v;
    float nx2 = n001 * (1-u) + n101 * u;
    float nx3 = n011 * (1-u) + n111 * u;
    float ny1 = nx2 * (1-v) + nx3 * v;
    return ny0 * (1-w) + ny1 * w;
}