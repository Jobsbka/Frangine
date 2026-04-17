#include "arxrender_nodes.hpp"
#include <fstream>
#include <stdexcept>
#include <sstream>

namespace arxrender::arxglue {

// ============================================================================
// RenderDeviceNode
// ============================================================================
RenderDeviceNode::RenderDeviceNode() = default;

void RenderDeviceNode::execute(arxglue::Context& ctx) {
    if (!m_ctx) {
        m_ctx = std::make_unique<arxrender::AR_context>();
        // Бэкенды должны быть зарегистрированы до вызова
        m_ctx->init(nullptr, 0, nullptr); 
        if (m_ctx->active_backend() == arxrender::AR_backend::NULL_BACKEND) {
            ctx.setState("arxrender.warning", "No GPU backend available, using NULL");
        }
    }
    if (!m_dev) {
        m_dev = std::make_unique<arxrender::AR_device>();
        arxrender::AR_device_config cfg{};
        cfg.debug_layer = m_cfg.debug;
        m_dev->init(m_ctx.get(), &cfg);
    }
    // Передаём в контекст графа для последующих узлов
    ctx.setState("arxrender.device", m_dev.get());
    ctx.setState("arxrender.context", m_ctx.get());
}

arxglue::ComponentMetadata RenderDeviceNode::getMetadata() const {
    return {"RenderDeviceNode", {}, {}, false, true, false};
}
void RenderDeviceNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "backend") m_cfg.backend = std::any_cast<std::string>(value);
    else if (name == "debug") m_cfg.debug = std::any_cast<bool>(value);
}
std::any RenderDeviceNode::getParameter(const std::string& name) const {
    if (name == "backend") return m_cfg.backend;
    if (name == "debug") return m_cfg.debug;
    return {};
}
void RenderDeviceNode::serialize(nlohmann::json& j) const {
    j["type"] = "RenderDeviceNode";
    j["params"]["backend"] = m_cfg.backend;
    j["params"]["debug"] = m_cfg.debug;
}
void RenderDeviceNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        if (j["params"].contains("backend")) m_cfg.backend = j["params"]["backend"];
        if (j["params"].contains("debug")) m_cfg.debug = j["params"]["debug"];
    }
}

// ============================================================================
// TextureLoadNode
// ============================================================================
TextureLoadNode::TextureLoadNode() = default;

void TextureLoadNode::execute(arxglue::Context& ctx) {
    m_device = ctx.getState<std::shared_ptr<arxrender::AR_device>>("arxrender.device");
    if (!m_device) {
        throw std::runtime_error("TextureLoadNode: No AR_device in context");
    }
    // Заглушка загрузки: в реальности здесь stb_image или DDS loader
    std::vector<uint8_t> pixels(4, 255); // 1x1 white
    arxrender::AR_texture_desc desc{};
    desc.width = 1; desc.height = 1; desc.format = arxrender::AR_format::RGBA8_UNORM;
    desc.initial_data = pixels.data();
    
    arxrender::AR_texture* texRaw = nullptr;
    m_device->create_texture(desc, &texRaw);
    m_texture = std::shared_ptr<arxrender::AR_texture>(texRaw, [dev = m_device](arxrender::AR_texture* ptr){
        if(dev && ptr) dev->context()->p_impl->callbacks.onTextureDestroy(ptr);
    });
    ctx.setOutputValue(ctx, 0, m_texture);
}

arxglue::ComponentMetadata TextureLoadNode::getMetadata() const {
    return {"TextureLoad", {}, {{"texture", typeid(std::shared_ptr<arxrender::AR_texture>)}, false, false};
}
void TextureLoadNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "path") m_path = std::any_cast<std::string>(value);
}
std::any TextureLoadNode::getParameter(const std::string& name) const {
    if (name == "path") return m_path;
    return {};
}
void TextureLoadNode::serialize(nlohmann::json& j) const {
    j["type"] = "TextureLoadNode"; j["params"]["path"] = m_path;
}
void TextureLoadNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params") && j["params"].contains("path")) m_path = j["params"]["path"];
}

// ============================================================================
// PipelineNode
// ============================================================================
PipelineNode::PipelineNode() = default;

void PipelineNode::execute(arxglue::Context& ctx) {
    m_device = ctx.getState<std::shared_ptr<arxrender::AR_device>>("arxrender.device");
    if (!m_device) throw std::runtime_error("PipelineNode: No AR_device");
    
    // Заглушка создания шейдеров/пайплайна
    // В реальности: чтение .spv/.glsl, компиляция
    arxrender::AR_pipeline_desc pdesc{};
    pdesc.topology = m_topology;
    
    arxrender::AR_pipeline* pipeRaw = nullptr;
    m_device->create_pipeline(pdesc, &pipeRaw);
    m_pipeline = std::shared_ptr<arxrender::AR_pipeline>(pipeRaw, [dev = m_device](arxrender::AR_pipeline* ptr){
        if(dev && ptr) dev->context()->p_impl->callbacks.onPipelineDestroy(ptr);
    });
    
    m_material = std::make_shared<arxrender::AR_material>(m_pipeline.get());
    ctx.setOutputValue(ctx, 0, m_material);
}

arxglue::ComponentMetadata PipelineNode::getMetadata() const {
    return {"PipelineNode", {}, {{"material", typeid(std::shared_ptr<arxrender::AR_material>)}, false, false};
}
void PipelineNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "topology") m_topology = std::any_cast<arxrender::AR_primitive_topology>(value);
}
std::any PipelineNode::getParameter(const std::string& name) const {
    if (name == "topology") return m_topology;
    return {};
}
void PipelineNode::serialize(nlohmann::json& j) const {
    j["type"] = "PipelineNode"; j["params"]["topology"] = static_cast<int>(m_topology);
}
void PipelineNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params") && j["params"].contains("topology")) m_topology = static_cast<arxrender::AR_primitive_topology>(j["params"]["topology"]);
}

// ============================================================================
// DrawCommandNode
// ============================================================================
DrawCommandNode::DrawCommandNode() = default;

void DrawCommandNode::execute(arxglue::Context& ctx) {
    m_cmd = ctx.getState<std::shared_ptr<arxrender::AR_command_buffer>>("arxrender.command_buffer");
    if (!m_cmd) throw std::runtime_error("DrawCommandNode: No command buffer in context");

    m_cmd->bind_pipeline(m_pipeline.get());
    m_cmd->bind_material(m_material.get());
    
    if (m_index_buf) {
        m_cmd->draw_indexed(m_index_buf.get(), m_index_count, 1, 0, 0, 0);
    } else {
        m_cmd->draw(m_vertex_count, 1, 0, 0);
    }
}

arxglue::ComponentMetadata DrawCommandNode::getMetadata() const {
    return {"DrawCommandNode", {
        {"pipeline", typeid(std::shared_ptr<arxrender::AR_pipeline>)},
        {"material", typeid(std::shared_ptr<arxrender::AR_material>)},
        {"vertex_buf", typeid(std::shared_ptr<arxrender::AR_buffer>)},
        {"index_buf", typeid(std::shared_ptr<arxrender::AR_buffer>)}
    }, {}, false, true, true};
}
void DrawCommandNode::setParameter(const std::string& name, const std::any& value) {
    if (name == "vertex_count") m_vertex_count = std::any_cast<uint32_t>(value);
    else if (name == "index_count") m_index_count = std::any_cast<uint32_t>(value);
}
std::any DrawCommandNode::getParameter(const std::string& name) const {
    if (name == "vertex_count") return m_vertex_count;
    if (name == "index_count") return m_index_count;
    return {};
}
void DrawCommandNode::serialize(nlohmann::json& j) const {
    j["type"] = "DrawCommandNode";
    j["params"]["vertex_count"] = m_vertex_count;
    j["params"]["index_count"] = m_index_count;
}
void DrawCommandNode::deserialize(const nlohmann::json& j) {
    if (j.contains("params")) {
        if (j["params"].contains("vertex_count")) m_vertex_count = j["params"]["vertex_count"];
        if (j["params"].contains("index_count")) m_index_count = j["params"]["index_count"];
    }
}

// ============================================================================
// PresentNode
// ============================================================================
PresentNode::PresentNode() = default;

void PresentNode::execute(arxglue::Context& ctx) {
    m_surface = ctx.getState<std::shared_ptr<arxrender::AR_surface>>("arxrender.surface");
    m_cmd = ctx.getState<std::shared_ptr<arxrender::AR_command_buffer>>("arxrender.command_buffer");
    if (!m_surface || !m_cmd) throw std::runtime_error("PresentNode: Missing surface or cmd buffer");
    m_surface->present(m_cmd.get());
}

arxglue::ComponentMetadata PresentNode::getMetadata() const {
    return {"PresentNode", {
        {"surface", typeid(std::shared_ptr<arxrender::AR_surface>)},
        {"command_buffer", typeid(std::shared_ptr<arxrender::AR_command_buffer>)}
    }, {}, false, true, false};
}
void PresentNode::setParameter(const std::string&, const std::any&) {}
std::any PresentNode::getParameter(const std::string&) const { return {}; }
void PresentNode::serialize(nlohmann::json& j) const { j["type"] = "PresentNode"; }
void PresentNode::deserialize(const nlohmann::json&) {}

// ============================================================================
// Регистрация
// ============================================================================
void register_render_nodes() {
    auto& factory = arxglue::NodeFactory::instance();
    factory.registerNode("RenderDeviceNode", []() { return std::make_unique<RenderDeviceNode>(); });
    factory.registerNode("TextureLoadNode", []() { return std::make_unique<TextureLoadNode>(); });
    factory.registerNode("PipelineNode", []() { return std::make_unique<PipelineNode>(); });
    factory.registerNode("DrawCommandNode", []() { return std::make_unique<DrawCommandNode>(); });
    factory.registerNode("PresentNode", []() { return std::make_unique<PresentNode>(); });
}

} // namespace arxrender::arxglue