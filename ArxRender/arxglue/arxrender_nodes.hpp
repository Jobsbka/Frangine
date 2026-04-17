#pragma once
#include "../include/arxrender.hpp"
#include "../include/arxrender_context.hpp"
#include "../include/arxrender_device.hpp"
#include "../include/arxrender_resource.hpp"
#include "../include/arxrender_pipeline.hpp"
#include "../include/arxrender_command.hpp"
#include "../src/core/node.hpp" // ArxGlue INode
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

namespace arxrender::arxglue {

// ============================================================================
// ContextNode - инициализирует AR_context и AR_device
// ============================================================================
class RenderDeviceNode : public arxglue::INode {
public:
    RenderDeviceNode();
    void execute(arxglue::Context& ctx) override;
    arxglue::ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    std::unique_ptr<arxrender::AR_context> m_ctx;
    std::unique_ptr<arxrender::AR_device> m_dev;
    struct { std::string backend = "opengl"; bool debug = false; } m_cfg;
};

// ============================================================================
// TextureLoadNode - загружает текстуру из файла/ассета
// ============================================================================
class TextureLoadNode : public arxglue::INode {
public:
    TextureLoadNode();
    void execute(arxglue::Context& ctx) override;
    arxglue::ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    std::string m_path;
    std::shared_ptr<arxrender::AR_texture> m_texture;
    std::shared_ptr<arxrender::AR_device> m_device;
};

// ============================================================================
// PipelineNode - создаёт пайплайн + материал
// ============================================================================
class PipelineNode : public arxglue::INode {
public:
    PipelineNode();
    void execute(arxglue::Context& ctx) override;
    arxglue::ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    std::shared_ptr<arxrender::AR_device> m_device;
    std::shared_ptr<arxrender::AR_shader> m_vs, m_fs;
    std::shared_ptr<arxrender::AR_pipeline> m_pipeline;
    std::shared_ptr<arxrender::AR_material> m_material;
    arxrender::AR_primitive_topology m_topology = arxrender::AR_primitive_topology::TRIANGLE_LIST;
};

// ============================================================================
// DrawCommandNode - записывает отрисовку в командный буфер
// ============================================================================
class DrawCommandNode : public arxglue::INode {
public:
    DrawCommandNode();
    void execute(arxglue::Context& ctx) override;
    arxglue::ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    std::shared_ptr<arxrender::AR_command_buffer> m_cmd;
    std::shared_ptr<arxrender::AR_pipeline> m_pipeline;
    std::shared_ptr<arxrender::AR_material> m_material;
    std::shared_ptr<arxrender::AR_buffer> m_vertex_buf, m_index_buf;
    uint32_t m_vertex_count = 0, m_index_count = 0;
};

// ============================================================================
// PresentNode - финализирует кадр и презентует поверхность
// ============================================================================
class PresentNode : public arxglue::INode {
public:
    PresentNode();
    void execute(arxglue::Context& ctx) override;
    arxglue::ComponentMetadata getMetadata() const override;
    void setParameter(const std::string& name, const std::any& value) override;
    std::any getParameter(const std::string& name) const override;
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

private:
    std::shared_ptr<arxrender::AR_surface> m_surface;
    std::shared_ptr<arxrender::AR_command_buffer> m_cmd;
};

void register_render_nodes();

} // namespace arxrender::arxglue