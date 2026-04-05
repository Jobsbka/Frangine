#pragma once
#include "../math/math.hpp"
#include <vector>
#include "../procgen/voxel_field.hpp"  

struct Vertex {
    sVector30 pos;
    sVector30 normal;
};

class MarchingCubes {
public:
    static void generate(const VoxelField& field, float isoLevel,
                         std::vector<Vertex>& outVertices,
                         std::vector<uint32_t>& outIndices);
private:
    static const int edgeTable[256];
    static const int triTable[256][16];
    static void interpolateVertex(const Vertex& v1, const Vertex& v2,
                                  float val1, float val2, Vertex& out);
};