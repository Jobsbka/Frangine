#pragma once
#include <any>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <fstream>
#include <vector>
#include <cstdint>

namespace arxglue {

class Asset {
public:
    virtual ~Asset() = default;
    virtual std::type_index getType() const = 0;
    virtual void writeToFile(const std::string& path) const = 0;
};

class TextureAsset : public Asset {
public:
    TextureAsset(int w, int h, const std::vector<uint8_t>& data)
        : width(w), height(h), pixels(data) {}

    std::type_index getType() const override { return typeid(TextureAsset); }

    void writeToFile(const std::string& path) const override {
        std::ofstream f(path, std::ios::binary);
        int32_t w = width, h = height;
        f.write(reinterpret_cast<const char*>(&w), sizeof(w));
        f.write(reinterpret_cast<const char*>(&h), sizeof(h));
        uint32_t size = static_cast<uint32_t>(pixels.size());
        f.write(reinterpret_cast<const char*>(&size), sizeof(size));
        f.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    }

    static std::shared_ptr<TextureAsset> readFromFile(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return nullptr;
        int32_t w, h;
        f.read(reinterpret_cast<char*>(&w), sizeof(w));
        f.read(reinterpret_cast<char*>(&h), sizeof(h));
        uint32_t size;
        f.read(reinterpret_cast<char*>(&size), sizeof(size));
        std::vector<uint8_t> pixels(size);
        f.read(reinterpret_cast<char*>(pixels.data()), size);
        return std::make_shared<TextureAsset>(w, h, pixels);
    }

    int width, height;
    std::vector<uint8_t> pixels;
};

class MeshAsset : public Asset {
public:
    struct Vertex { float x, y, z; };
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    MeshAsset(const std::vector<Vertex>& v, const std::vector<uint32_t>& i)
        : vertices(v), indices(i) {}

    std::type_index getType() const override { return typeid(MeshAsset); }

    void writeToFile(const std::string& path) const override {
        std::ofstream f(path, std::ios::binary);
        uint32_t vcount = static_cast<uint32_t>(vertices.size());
        f.write(reinterpret_cast<const char*>(&vcount), sizeof(vcount));
        f.write(reinterpret_cast<const char*>(vertices.data()), vcount * sizeof(Vertex));
        uint32_t icount = static_cast<uint32_t>(indices.size());
        f.write(reinterpret_cast<const char*>(&icount), sizeof(icount));
        f.write(reinterpret_cast<const char*>(indices.data()), icount * sizeof(uint32_t));
    }

    static std::shared_ptr<MeshAsset> readFromFile(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return nullptr;
        uint32_t vcount;
        f.read(reinterpret_cast<char*>(&vcount), sizeof(vcount));
        std::vector<Vertex> verts(vcount);
        f.read(reinterpret_cast<char*>(verts.data()), vcount * sizeof(Vertex));
        uint32_t icount;
        f.read(reinterpret_cast<char*>(&icount), sizeof(icount));
        std::vector<uint32_t> inds(icount);
        f.read(reinterpret_cast<char*>(inds.data()), icount * sizeof(uint32_t));
        return std::make_shared<MeshAsset>(verts, inds);
    }
};

class AssetManager {
public:
    static AssetManager& instance() {
        static AssetManager inst;
        return inst;
    }

    std::shared_ptr<TextureAsset> loadTexture(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(path);
        if (it != m_cache.end()) return std::dynamic_pointer_cast<TextureAsset>(it->second);
        auto tex = TextureAsset::readFromFile(path);
        if (tex) m_cache[path] = tex;
        return tex;
    }

    std::shared_ptr<MeshAsset> loadMesh(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(path);
        if (it != m_cache.end()) return std::dynamic_pointer_cast<MeshAsset>(it->second);
        auto mesh = MeshAsset::readFromFile(path);
        if (mesh) m_cache[path] = mesh;
        return mesh;
    }

    void saveAsset(const std::string& path, std::shared_ptr<Asset> asset) {
        std::lock_guard<std::mutex> lock(m_mutex);
        asset->writeToFile(path);
        m_cache[path] = asset;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Asset>> m_cache;
    std::mutex m_mutex;
};

} // namespace arxglue