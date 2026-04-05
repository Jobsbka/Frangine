#pragma once
#include "../math/math.hpp"

class Perlin3D {
public:
    Perlin3D(unsigned int seed = 0);
    float noise(float x, float y, float z) const;
private:
    unsigned char perm[512];
    static const sVector30 gradients[16];
    void initPermutation(unsigned int seed);
    static float grad(int hash, float x, float y, float z);
};