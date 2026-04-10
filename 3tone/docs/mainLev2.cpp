// main.cpp -- Practical tests for 3tone nested graph capabilities
// Builds a procedural rock texture generator with multi-octave noise,
// demonstrates baking of subgraphs for performance, and recursive scene assembly.
//
// Old surface tests are kept in main_experiments.cpp for reference.

#include "core/graph.hpp"
#include "core/executor.hpp"
#include "core/baking.hpp"
#include "nodes/basic_nodes.hpp"
#include "nodes/node_factory.hpp"
#include "types/type_system.hpp"
#include "assets/asset_manager.hpp"
#include <iostream>
#include <chrono>
#include <cmath>
#include <random>
#include <array>
#include <fstream>

using namespace arxglue;

// ------------------------------------------------------------
// Diagnostic utilities (minimal, just for timing)
// ------------------------------------------------------------
class Timer {
public:
    Timer(const std::string& name) : m_name(name), m_start(std::chrono::high_resolution_clock::now()) {}
    ~Timer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start).count();
        std::cout << "[Timer] " << m_name << ": " << ms << " ms" << std::endl;
    }
private:
    std::string m_name;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

// ------------------------------------------------------------
// Improved Perlin-like noise node (sum of sines for demonstration)
// ------------------------------------------------------------
class NoiseNode : public INode {
public:
    NoiseNode() : m_scale(0.1f), m_seed(0) {}

    void execute(Context& ctx) override {
        int width = std::any_cast<int>(getInputValue(ctx, 0));
        int height = std::any_cast<int>(getInputValue(ctx, 1));
        // Generate simple noise texture
        std::vector<uint8_t> pixels(width * height * 4);
        std::mt19937 rng(m_seed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float fx = x * m_scale;
                float fy = y * m_scale;
                // Simple multi-octave? For single octave we just use random
                float v = dist(rng);
                uint8_t c = static_cast<uint8_t>(v * 255);
                size_t idx = (y * width + x) * 4;
                pixels[idx+0] = c;
                pixels[idx+1] = c;
                pixels[idx+2] = c;
                pixels[idx+3] = 255;
            }
        }
        auto tex = std::make_shared<TextureAsset>(width, height, pixels);
        setOutputValue(ctx, 0, tex);
    }

    ComponentMetadata getMetadata() const override {
        return {
            "NoiseNode",
            {{"width", typeid(int)}, {"height", typeid(int)}},
            {{"texture", typeid(std::shared_ptr<TextureAsset>)}},
            false, false
        };
    }

    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "scale") m_scale = std::any_cast<float>(value);
        else if (name == "seed") m_seed = std::any_cast<int>(value);
    }
    std::any getParameter(const std::string& name) const override {
        if (name == "scale") return m_scale;
        if (name == "seed") return m_seed;
        return {};
    }
    void serialize(nlohmann::json& j) const override {
        j["type"] = "NoiseNode";
        j["params"]["scale"] = m_scale;
        j["params"]["seed"] = m_seed;
    }
    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params")) {
            if (j["params"].contains("scale")) m_scale = j["params"]["scale"];
            if (j["params"].contains("seed")) m_seed = j["params"]["seed"];
        }
    }

private:
    float m_scale;
    int m_seed = 0;
};

// ------------------------------------------------------------
// Gaussian blur component (operates on TextureAsset)
// ------------------------------------------------------------
// This is a pure function that takes a texture and returns a blurred version.
// It uses a simple box blur for demonstration.
std::shared_ptr<TextureAsset> blurTexture(const std::shared_ptr<TextureAsset>& src, int radius) {
    if (!src || radius <= 0) return src;
    int w = src->width, h = src->height;
    const auto& srcPix = src->pixels;
    std::vector<uint8_t> dstPix(w * h * 4);
    // Simple box blur
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float sum[4] = {0,0,0,0};
            int count = 0;
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >=0 && nx < w && ny >=0 && ny < h) {
                        size_t idx = (ny * w + nx) * 4;
                        sum[0] += srcPix[idx];
                        sum[1] += srcPix[idx+1];
                        sum[2] += srcPix[idx+2];
                        sum[3] += srcPix[idx+3];
                        ++count;
                    }
                }
            }
            size_t idx = (y * w + x) * 4;
            dstPix[idx]   = static_cast<uint8_t>(sum[0] / count);
            dstPix[idx+1] = static_cast<uint8_t>(sum[1] / count);
            dstPix[idx+2] = static_cast<uint8_t>(sum[2] / count);
            dstPix[idx+3] = static_cast<uint8_t>(sum[3] / count);
        }
    }
    return std::make_shared<TextureAsset>(w, h, dstPix);
}

// Wrapper component that can be used inside nodes
struct BlurComponent {
    int radius;
    std::shared_ptr<TextureAsset> operator()(const std::shared_ptr<TextureAsset>& tex) const {
        return blurTexture(tex, radius);
    }
};

// ------------------------------------------------------------
// Node that uses a component internally (demonstrates mixing)
// ------------------------------------------------------------
class BlurNode : public INode {
public:
    BlurNode() : m_radius(2) {}
    void execute(Context& ctx) override {
        auto tex = std::any_cast<std::shared_ptr<TextureAsset>>(getInputValue(ctx, 0));
        BlurComponent comp{m_radius};
        auto result = comp(tex);
        setOutputValue(ctx, 0, result);
    }
    ComponentMetadata getMetadata() const override {
        return {"BlurNode", {{"in", typeid(std::shared_ptr<TextureAsset>)}}, {{"out", typeid(std::shared_ptr<TextureAsset>)}}, true, false};
    }
    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "radius") m_radius = std::any_cast<int>(value);
    }
    std::any getParameter(const std::string& name) const override {
        if (name == "radius") return m_radius;
        return {};
    }
    void serialize(nlohmann::json& j) const override {
        j["type"] = "BlurNode";
        j["params"]["radius"] = m_radius;
    }
    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params") && j["params"].contains("radius"))
            m_radius = j["params"]["radius"];
    }
private:
    int m_radius;
};

// ------------------------------------------------------------
// Octave node: builds a local graph for one noise octave
// ------------------------------------------------------------
class OctaveNode : public INode {
public:
    void execute(Context& ctx) override {
        int width      = std::any_cast<int>(getInputValue(ctx, 0));
        int height     = std::any_cast<int>(getInputValue(ctx, 1));
        float scale    = std::any_cast<float>(getInputValue(ctx, 2));
        int seed       = std::any_cast<int>(getInputValue(ctx, 3));
        float weight   = std::any_cast<float>(getInputValue(ctx, 4));
        int blurRadius = std::any_cast<int>(getInputValue(ctx, 5));

        // Build a local graph: Noise -> Blur
        Graph localGraph;
        auto noiseNode = std::make_unique<NoiseNode>();
        noiseNode->setParameter("scale", scale);
        noiseNode->setParameter("seed", seed + static_cast<int>(scale*1000));
        NodeId noiseId = localGraph.addNode(std::move(noiseNode));

        // Provide width/height as constants
        auto widthConst = std::make_unique<ConstantNode<int>>(width);
        auto heightConst = std::make_unique<ConstantNode<int>>(height);
        NodeId widthId = localGraph.addNode(std::move(widthConst));
        NodeId heightId = localGraph.addNode(std::move(heightConst));
        localGraph.addConnection({widthId, 0, noiseId, 0});
        localGraph.addConnection({heightId, 0, noiseId, 1});

        // Blur node
        auto blurNode = std::make_unique<BlurNode>();
        blurNode->setParameter("radius", blurRadius);
        NodeId blurId = localGraph.addNode(std::move(blurNode));
        localGraph.addConnection({noiseId, 0, blurId, 0});

        // We need to get the texture out. Add an OutputNode.
        // OutputNode expects a single input and writes to ctx.output.
        class OutputTexNode : public INode {
        public:
            void execute(Context& c) override {
                auto val = getInputValue(c, 0);
                c.output = val;
                setOutputValue(c, 0, val);
            }
            ComponentMetadata getMetadata() const override {
                return {"OutputTexNode", {{"in", typeid(std::shared_ptr<TextureAsset>)}}, {{"out", typeid(std::shared_ptr<TextureAsset>)}}, true, false};
            }
            void setParameter(const std::string&, const std::any&) override {}
            std::any getParameter(const std::string&) const override { return {}; }
            void serialize(nlohmann::json&) const override {}
            void deserialize(const nlohmann::json&) override {}
        };
        auto outNode = std::make_unique<OutputTexNode>();
        NodeId outId = localGraph.addNode(std::move(outNode));
        localGraph.addConnection({blurId, 0, outId, 0});

        // Execute local graph with its own context
        Context localCtx;
        Executor exec(1);
        exec.execute(localGraph, localCtx, {outId});

        // The result texture is in localCtx.output
        auto resultTex = std::any_cast<std::shared_ptr<TextureAsset>>(localCtx.output);
        // Optionally multiply by weight? We'll do blending in the parent.
        // Store weight in state for later use
        ctx.setState("octave_weight_" + std::to_string(getId()), weight);
        setOutputValue(ctx, 0, resultTex);
    }

    ComponentMetadata getMetadata() const override {
        return {
            "OctaveNode",
            {
                {"width", typeid(int)},
                {"height", typeid(int)},
                {"scale", typeid(float)},
                {"seed", typeid(int)},
                {"weight", typeid(float)},
                {"blur", typeid(int)}
            },
            {{"texture", typeid(std::shared_ptr<TextureAsset>)}},
            false, false
        };
    }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "OctaveNode"; }
    void deserialize(const nlohmann::json&) override {}
};

// ------------------------------------------------------------
// RockTextureNode: combines multiple octaves
// ------------------------------------------------------------
class RockTextureNode : public INode {
public:
    void execute(Context& ctx) override {
        int width = 256, height = 256;
        int octaves = 3;
        float baseScale = 0.05f;
        float persistence = 0.5f;
        int baseSeed = 12345;
        int blurRadius = 1;

        // Build a graph that creates octaves and blends them
        Graph graph;
        std::vector<NodeId> octaveIds;
        std::vector<float> weights;
        float amp = 1.0f;
        float scale = baseScale;
        for (int i = 0; i < octaves; ++i) {
            auto octNode = std::make_unique<OctaveNode>();
            NodeId id = graph.addNode(std::move(octNode));
            octaveIds.push_back(id);
            weights.push_back(amp);
            amp *= persistence;
            scale *= 2.0f;
        }

        // Provide constant inputs to each octave
        auto widthConst = std::make_unique<ConstantNode<int>>(width);
        auto heightConst = std::make_unique<ConstantNode<int>>(height);
        auto blurConst = std::make_unique<ConstantNode<int>>(blurRadius);
        NodeId wId = graph.addNode(std::move(widthConst));
        NodeId hId = graph.addNode(std::move(heightConst));
        NodeId bId = graph.addNode(std::move(blurConst));

        scale = baseScale;
        for (size_t i = 0; i < octaveIds.size(); ++i) {
            auto scaleConst = std::make_unique<ConstantNode<float>>(scale);
            auto seedConst = std::make_unique<ConstantNode<int>>(baseSeed + i*100);
            auto weightConst = std::make_unique<ConstantNode<float>>(weights[i]);
            NodeId sId = graph.addNode(std::move(scaleConst));
            NodeId seedId = graph.addNode(std::move(seedConst));
            NodeId weightId = graph.addNode(std::move(weightConst));

            graph.addConnection({wId, 0, octaveIds[i], 0});
            graph.addConnection({hId, 0, octaveIds[i], 1});
            graph.addConnection({sId, 0, octaveIds[i], 2});
            graph.addConnection({seedId, 0, octaveIds[i], 3});
            graph.addConnection({weightId, 0, octaveIds[i], 4});
            graph.addConnection({bId, 0, octaveIds[i], 5});
            scale *= 2.0f;
        }

        // Blend octaves: simple addition with weights
        // We'll implement a BlendNode that sums weighted textures.
        class BlendTexturesNode : public INode {
        public:
            BlendTexturesNode(int numInputs) : m_numInputs(numInputs) {}
            void execute(Context& c) override {
                int w = 0, h = 0;
                std::vector<std::shared_ptr<TextureAsset>> texs;
                std::vector<float> weights;
                for (int i = 0; i < m_numInputs; ++i) {
                    auto tex = std::any_cast<std::shared_ptr<TextureAsset>>(getInputValue(c, i));
                    if (tex) { w = tex->width; h = tex->height; texs.push_back(tex); }
                    // weight is stored in state by OctaveNode; we retrieve it via a key convention
                    // In a real implementation, weights would be separate inputs.
                }
                // For simplicity, we'll just average them here (demo)
                // In a real scenario you'd have weight inputs.
                if (texs.empty()) { setOutputValue(c, 0, std::shared_ptr<TextureAsset>()); return; }
                std::vector<uint8_t> out(w * h * 4, 0);
                for (size_t i = 0; i < texs.size(); ++i) {
                    const auto& pix = texs[i]->pixels;
                    float weight = 1.0f / texs.size(); // equal blend
                    for (size_t j = 0; j < pix.size(); ++j) {
                        out[j] = static_cast<uint8_t>(std::min(255, out[j] + (int)(pix[j] * weight)));
                    }
                }
                auto result = std::make_shared<TextureAsset>(w, h, out);
                setOutputValue(c, 0, result);
            }
            ComponentMetadata getMetadata() const override {
                std::vector<PortInfo> inputs;
                for (int i=0;i<m_numInputs;++i) inputs.push_back({"in"+std::to_string(i), typeid(std::shared_ptr<TextureAsset>)});
                return {"BlendTextures", inputs, {{"out", typeid(std::shared_ptr<TextureAsset>)}}, true, false};
            }
            void setParameter(const std::string&, const std::any&) override {}
            std::any getParameter(const std::string&) const override { return {}; }
            void serialize(nlohmann::json&) const override {}
            void deserialize(const nlohmann::json&) override {}
        private:
            int m_numInputs;
        };

        auto blendNode = std::make_unique<BlendTexturesNode>(octaves);
        NodeId blendId = graph.addNode(std::move(blendNode));
        for (size_t i = 0; i < octaveIds.size(); ++i) {
            graph.addConnection({octaveIds[i], 0, blendId, static_cast<int>(i)});
        }

        // Output
        class OutputTexNode : public INode {
        public:
            void execute(Context& c) override {
                auto val = getInputValue(c, 0);
                c.output = val;
                setOutputValue(c, 0, val);
            }
            ComponentMetadata getMetadata() const override {
                return {"OutputTexNode", {{"in", typeid(std::shared_ptr<TextureAsset>)}}, {{"out", typeid(std::shared_ptr<TextureAsset>)}}, true, false};
            }
            void setParameter(const std::string&, const std::any&) override {}
            std::any getParameter(const std::string&) const override { return {}; }
            void serialize(nlohmann::json&) const override {}
            void deserialize(const nlohmann::json&) override {}
        };
        auto outNode = std::make_unique<OutputTexNode>();
        NodeId outId = graph.addNode(std::move(outNode));
        graph.addConnection({blendId, 0, outId, 0});

        // Execute the subgraph with a local context
        Context localCtx;
        Executor exec(1);
        exec.execute(graph, localCtx, {outId});

        auto finalTex = std::any_cast<std::shared_ptr<TextureAsset>>(localCtx.output);
        setOutputValue(ctx, 0, finalTex);
    }

    ComponentMetadata getMetadata() const override {
        return {"RockTextureNode", {}, {{"texture", typeid(std::shared_ptr<TextureAsset>)}}, false, false};
    }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "RockTextureNode"; }
    void deserialize(const nlohmann::json&) override {}
};

// ------------------------------------------------------------
// Node that bakes its subgraph automatically on first run
// ------------------------------------------------------------
class BakedOctaveNode : public INode {
public:
    void execute(Context& ctx) override {
        std::string path = "baked_octave_" + std::to_string(m_octaveIndex) + ".png";
        auto asset = AssetManager::instance().loadTexture(path);
        if (!asset) {
            // Prepare a context with required inputs
            Context localCtx;
            // Simulate inputs that would come from graph connections
            localCtx.setState("in_0_0", m_width);
            localCtx.setState("in_0_1", m_height);
            localCtx.setState("in_0_2", m_scale);
            localCtx.setState("in_0_3", m_seed);
            localCtx.setState("in_0_4", m_weight);
            localCtx.setState("in_0_5", m_blur);
            OctaveNode oct;
            oct.setId(0); // necessary for getInputValue to form correct key
            oct.execute(localCtx);
            asset = std::any_cast<std::shared_ptr<TextureAsset>>(localCtx.output);
            AssetManager::instance().saveAsset(path, asset);
        }
        setOutputValue(ctx, 0, asset);
    }

    ComponentMetadata getMetadata() const override {
        return {"BakedOctaveNode", {}, {{"texture", typeid(std::shared_ptr<TextureAsset>)}}, true, false};
    }
    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "index") m_octaveIndex = std::any_cast<int>(value);
        else if (name == "width") m_width = std::any_cast<int>(value);
        else if (name == "height") m_height = std::any_cast<int>(value);
        else if (name == "scale") m_scale = std::any_cast<float>(value);
        else if (name == "seed") m_seed = std::any_cast<int>(value);
        else if (name == "weight") m_weight = std::any_cast<float>(value);
        else if (name == "blur") m_blur = std::any_cast<int>(value);
    }
    std::any getParameter(const std::string& name) const override {
        if (name == "index") return m_octaveIndex;
        if (name == "width") return m_width;
        if (name == "height") return m_height;
        if (name == "scale") return m_scale;
        if (name == "seed") return m_seed;
        if (name == "weight") return m_weight;
        if (name == "blur") return m_blur;
        return {};
    }
    void serialize(nlohmann::json& j) const override {
        j["type"] = "BakedOctaveNode";
        j["params"]["index"] = m_octaveIndex;
        j["params"]["width"] = m_width;
        j["params"]["height"] = m_height;
        j["params"]["scale"] = m_scale;
        j["params"]["seed"] = m_seed;
        j["params"]["weight"] = m_weight;
        j["params"]["blur"] = m_blur;
    }
    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params")) {
            auto& p = j["params"];
            if (p.contains("index")) m_octaveIndex = p["index"];
            if (p.contains("width")) m_width = p["width"];
            if (p.contains("height")) m_height = p["height"];
            if (p.contains("scale")) m_scale = p["scale"];
            if (p.contains("seed")) m_seed = p["seed"];
            if (p.contains("weight")) m_weight = p["weight"];
            if (p.contains("blur")) m_blur = p["blur"];
        }
    }

private:
    int m_octaveIndex = 0;
    int m_width = 256;
    int m_height = 256;
    float m_scale = 0.1f;
    int m_seed = 42;
    float m_weight = 1.0f;
    int m_blur = 1;
};

// ------------------------------------------------------------
// Registration
// ------------------------------------------------------------
void registerPracticalNodes() {
    auto& f = NodeFactory::instance();
    f.registerNode("NoiseNode", []() { return std::make_unique<NoiseNode>(); });
    f.registerNode("BlurNode", []() { return std::make_unique<BlurNode>(); });
    f.registerNode("OctaveNode", []() { return std::make_unique<OctaveNode>(); });
    f.registerNode("RockTextureNode", []() { return std::make_unique<RockTextureNode>(); });
    f.registerNode("BakedOctaveNode", []() { return std::make_unique<BakedOctaveNode>(); });
}

// ------------------------------------------------------------
// Test 1: Generate rock texture and measure time
// ------------------------------------------------------------
void testRockTexture() {
    std::cout << "\n========== Test: Procedural Rock Texture ==========" << std::endl;
    Graph g;
    auto rock = std::make_unique<RockTextureNode>();
    NodeId rockId = g.addNode(std::move(rock));
    // Add a consumer to force execution (we just need the texture)
    class TexConsumer : public INode {
    public:
        void execute(Context& ctx) override {
            auto tex = std::any_cast<std::shared_ptr<TextureAsset>>(getInputValue(ctx, 0));
            if (tex) {
                std::cout << "Generated texture " << tex->width << "x" << tex->height << std::endl;
                // Save to file for inspection
                AssetManager::instance().saveAsset("rock_output.png", tex);
            }
            setOutputValue(ctx, 0, 0);
        }
        ComponentMetadata getMetadata() const override {
            return {"TexConsumer", {{"in", typeid(std::shared_ptr<TextureAsset>)}}, {}, false, false};
        }
        void setParameter(const std::string&, const std::any&) override {}
        std::any getParameter(const std::string&) const override { return {}; }
        void serialize(nlohmann::json&) const override {}
        void deserialize(const nlohmann::json&) override {}
    };
    auto cons = std::make_unique<TexConsumer>();
    NodeId consId = g.addNode(std::move(cons));
    g.addConnection({rockId, 0, consId, 0});

    Context ctx;
    Executor ex(1);
    {
        Timer t("Rock texture generation (first run)");
        ex.execute(g, ctx, {consId});
    }
    // Second run (cached)
    {
        Timer t("Rock texture generation (second run, cached)");
        ex.execute(g, ctx, {consId});
    }
}

// ------------------------------------------------------------
// Test 2: Baked octave vs computed
// ------------------------------------------------------------
void testBaking() {
    std::cout << "\n========== Test: Baking octaves ==========" << std::endl;
    // Compute fresh
    {
        Timer t("Compute octave 0");
        OctaveNode oct;
        Context ctx;
        ctx.setState("in_0_0", 256);
        ctx.setState("in_0_1", 256);
        ctx.setState("in_0_2", 0.1f);
        ctx.setState("in_0_3", 42);
        ctx.setState("in_0_4", 1.0f);
        ctx.setState("in_0_5", 1);
        oct.setId(0);
        oct.execute(ctx);
        auto tex = std::any_cast<std::shared_ptr<TextureAsset>>(ctx.output);
        AssetManager::instance().saveAsset("octave0_computed.png", tex);
    }
    // Baked version
    {
        Timer t("Baked octave (first time, bakes)");
        BakedOctaveNode baked;
        baked.setParameter("index", 0);
        baked.setParameter("width", 256);
        baked.setParameter("height", 256);
        baked.setParameter("scale", 0.1f);
        baked.setParameter("seed", 42);
        baked.setParameter("weight", 1.0f);
        baked.setParameter("blur", 1);
        Context ctx;
        baked.execute(ctx);
    }
    {
        Timer t("Baked octave (second time, loads from cache)");
        BakedOctaveNode baked;
        baked.setParameter("index", 0);
        baked.setParameter("width", 256);
        baked.setParameter("height", 256);
        baked.setParameter("scale", 0.1f);
        baked.setParameter("seed", 42);
        baked.setParameter("weight", 1.0f);
        baked.setParameter("blur", 1);
        Context ctx;
        baked.execute(ctx);
    }
}

// ------------------------------------------------------------
// Test 3: Deep nesting with components
// ------------------------------------------------------------
void testDeepNesting() {
    std::cout << "\n========== Test: Deep nesting (INode -> Component -> INode) ==========" << std::endl;
    // Node that uses a component which itself wraps an INode (via NodeComponent)
    // We'll create a chain: Constant -> (MultiplyNode wrapped in component) -> Output
    class NestedWrapperNode : public INode {
    public:
        void execute(Context& ctx) override {
            int input = std::any_cast<int>(getInputValue(ctx, 0));
            // Component that wraps a MultiplyNode
            class MultiplyNode : public INode {
            public:
                void execute(Context& c) override {
                    int x = std::any_cast<int>(getInputValue(c, 0));
                    setOutputValue(c, 0, x * 3);
                }
                ComponentMetadata getMetadata() const override {
                    return {"MultiplyNode", {{"in", typeid(int)}}, {{"out", typeid(int)}}, true, false};
                }
                void setParameter(const std::string&, const std::any&) override {}
                std::any getParameter(const std::string&) const override { return {}; }
                void serialize(nlohmann::json&) const override {}
                void deserialize(const nlohmann::json&) override {}
            };
            auto multComp = [node = std::make_unique<MultiplyNode>()](Context& c, int val) -> int {
                node->setId(999);
                c.setState("in_999_0", val);
                node->execute(c);
                return std::any_cast<int>(c.output);
            };
            int result = multComp(ctx, input);
            setOutputValue(ctx, 0, result);
        }
        ComponentMetadata getMetadata() const override {
            return {"NestedWrapperNode", {{"in", typeid(int)}}, {{"out", typeid(int)}}, false, false};
        }
        void setParameter(const std::string&, const std::any&) override {}
        std::any getParameter(const std::string&) const override { return {}; }
        void serialize(nlohmann::json& j) const override { j["type"] = "NestedWrapperNode"; }
        void deserialize(const nlohmann::json&) override {}
    };

    Graph g;
    auto c = std::make_unique<ConstantNode<int>>(10);
    NodeId cid = g.addNode(std::move(c));
    auto wrap = std::make_unique<NestedWrapperNode>();
    NodeId wid = g.addNode(std::move(wrap));
    auto cons = std::make_unique<FloatConsumerNode>();
    NodeId outId = g.addNode(std::move(cons));
    g.addConnection({cid, 0, wid, 0});
    g.addConnection({wid, 0, outId, 0});

    Context ctx;
    Executor ex(1);
    ex.execute(g, ctx, {outId});
    std::cout << "Result: " << std::any_cast<float>(ctx.output) << " (expected 30)" << std::endl;
}

// ------------------------------------------------------------
int main() {
    try {
        initBasicTypes();
        registerBasicNodes();
        registerPracticalNodes();

        testRockTexture();
        testBaking();
        testDeepNesting();

        std::cout << "\nAll practical tests completed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}