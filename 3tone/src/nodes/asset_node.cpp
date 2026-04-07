#include "asset_node.hpp"
#include "../assets/asset_manager.hpp"

namespace arxglue {

void AssetNode::execute(Context& ctx) {
    if (!m_cachedAsset) {
        if (m_assetType == "texture") {
            m_cachedAsset = AssetManager::instance().loadTexture(m_assetPath);
        } else if (m_assetType == "mesh") {
            m_cachedAsset = AssetManager::instance().loadMesh(m_assetPath);
        }
        // Если не загрузилось, создаём пустой ассет-заглушку
        if (!m_cachedAsset) {
            if (m_assetType == "texture") {
                std::vector<uint8_t> dummy(64 * 64 * 4, 128);
                m_cachedAsset = std::make_shared<TextureAsset>(64, 64, dummy);
            } else if (m_assetType == "mesh") {
                m_cachedAsset = std::make_shared<MeshAsset>(
                    std::vector<MeshAsset::Vertex>{}, std::vector<uint32_t>{});
            }
        }
    }

    // Приводим к ожидаемому типу в зависимости от m_assetType
    if (m_assetType == "texture") {
        auto tex = std::dynamic_pointer_cast<TextureAsset>(m_cachedAsset);
        if (!tex) {
            // fallback: создаём новую текстуру
            std::vector<uint8_t> dummy(64 * 64 * 4, 128);
            tex = std::make_shared<TextureAsset>(64, 64, dummy);
            m_cachedAsset = tex;
        }
        setOutputValue(ctx, 0, tex);
    } else if (m_assetType == "mesh") {
        auto mesh = std::dynamic_pointer_cast<MeshAsset>(m_cachedAsset);
        if (!mesh) {
            mesh = std::make_shared<MeshAsset>(
                std::vector<MeshAsset::Vertex>{}, std::vector<uint32_t>{});
            m_cachedAsset = mesh;
        }
        setOutputValue(ctx, 0, mesh);
    } else {
        setOutputValue(ctx, 0, std::any{});
    }
}

} // namespace arxglue