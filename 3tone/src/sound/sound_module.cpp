#include "sound_module.hpp"
#include "audio_buffer.hpp"
#include "audio_asset.hpp"
#include "audio_device.hpp"
#include "audio_graph.hpp"
#include "miniaudio_backend.hpp"
#include "../core/type_system.hpp"
#include "../nodes/node_factory.hpp"
#include "../nodes/soundn/generator_nodes.hpp"
#include "../nodes/soundn/filter_nodes.hpp"
#include "../nodes/soundn/mixer_nodes.hpp"
#include "../nodes/soundn/io_nodes.hpp"
#include "../nodes/soundn/spatial_nodes.hpp"
#include <algorithm>

namespace arxglue::sound {

void initializeModule() {
    auto& ts = TypeSystem::instance();
    auto& factory = NodeFactory::instance();

    ts.registerType(TypeId::AudioBuffer, typeid(std::shared_ptr<AudioBuffer>));
    ts.registerType(TypeId::AudioAsset,  typeid(std::shared_ptr<AudioAsset>));
    ts.registerType(TypeId::AudioSpec,   typeid(AudioSpec));
    ts.registerType(TypeId::AudioDevice, typeid(std::shared_ptr<AudioDevice>));
    ts.registerType(TypeId::AudioGraph,  typeid(std::shared_ptr<AudioGraph>));

    // Конвертер AudioAsset -> AudioBuffer
    ts.registerConverter(TypeId::AudioAsset, TypeId::AudioBuffer,
        [](const std::any& v) -> std::any {
            auto asset = std::any_cast<std::shared_ptr<AudioAsset>>(v);
            if (asset && asset->getBuffer()) {
                auto buf = asset->getBuffer();
                auto copy = std::make_shared<AudioBuffer>(buf->getSpec(), buf->getNumFrames());
                std::copy_n(buf->data(), buf->getNumSamples(), copy->data());
                return copy;
            }
            return std::shared_ptr<AudioBuffer>();
        });

    // Конвертер AudioBuffer -> AudioAsset
    ts.registerConverter(TypeId::AudioBuffer, TypeId::AudioAsset,
        [](const std::any& v) -> std::any {
            auto buf = std::any_cast<std::shared_ptr<AudioBuffer>>(v);
            auto asset = std::make_shared<AudioAsset>();
            if (buf) {
                auto newBuf = std::make_unique<AudioBuffer>(buf->getSpec(), buf->getNumFrames());
                std::copy_n(buf->data(), buf->getNumSamples(), newBuf->data());
                asset->setBuffer(std::move(newBuf));
            }
            return asset;
        });

    // Регистрация узлов
    factory.registerNode("SineOscillator",   [](){ return std::make_unique<nodes::SineOscillatorNode>(); });
    factory.registerNode("WhiteNoise",       [](){ return std::make_unique<nodes::WhiteNoiseNode>(); });
    factory.registerNode("BiquadFilter",     [](){ return std::make_unique<nodes::BiquadFilterNode>(); });
    factory.registerNode("Gain",             [](){ return std::make_unique<nodes::GainNode>(); });
    factory.registerNode("Mixer",            [](){ return std::make_unique<nodes::MixerNode>(); });
    factory.registerNode("Panner",           [](){ return std::make_unique<nodes::PannerNode>(); });
    factory.registerNode("AudioOutput",      [](){ return std::make_unique<nodes::AudioOutputNode>(); });
    factory.registerNode("AudioFileWriter",  [](){ return std::make_unique<nodes::AudioFileWriterNode>(); });
    factory.registerNode("AudioFileReader",  [](){ return std::make_unique<nodes::AudioFileReaderNode>(); });
    factory.registerNode("GASpatializer",    [](){ return std::make_unique<nodes::GASpatializerNode>(); });
}

} // namespace arxglue::sound