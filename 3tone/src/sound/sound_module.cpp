#include "audio_buffer.hpp"
#include "audio_asset.hpp"
#include "audio_device.hpp"
#include "audio_graph.hpp"
#include "nodes/generator_nodes.hpp"
#include "nodes/filter_nodes.hpp"
#include "nodes/mixer_nodes.hpp"
#include "nodes/io_nodes.hpp"
#include "nodes/spatial_nodes.hpp"
#include "../types/type_system.hpp"
#include "../nodes/node_factory.hpp"

namespace arxglue::sound {

void initializeModule() {
    auto& ts = TypeSystem::instance();
    auto& factory = NodeFactory::instance();

    // Регистрация типов
    ts.registerType(TypeId::AudioBuffer, typeid(std::shared_ptr<AudioBuffer>));
    ts.registerType(TypeId::AudioAsset,  typeid(std::shared_ptr<AudioAsset>));
    ts.registerType(TypeId::AudioSpec,   typeid(AudioSpec));
    ts.registerType(TypeId::AudioDevice, typeid(std::shared_ptr<AudioDevice>));
    ts.registerType(TypeId::AudioGraph,  typeid(std::shared_ptr<AudioGraph>));

    // Конвертеры
    ts.registerConverter(TypeId::AudioAsset, TypeId::AudioBuffer,
        [](const std::any& v) -> std::any {
            auto asset = std::any_cast<std::shared_ptr<AudioAsset>>(v);
            if (asset && asset->getBuffer()) {
                return std::make_shared<AudioBuffer>(*asset->getBuffer());
            }
            return std::shared_ptr<AudioBuffer>();
        });
    ts.registerConverter(TypeId::AudioBuffer, TypeId::AudioAsset,
        [](const std::any& v) -> std::any {
            auto buf = std::any_cast<std::shared_ptr<AudioBuffer>>(v);
            auto asset = std::make_shared<AudioAsset>();
            if (buf) {
                asset->setBuffer(std::make_unique<AudioBuffer>(*buf));
            }
            return asset;
        });

    // Регистрация аудиоузлов
    factory.registerNode("SineOscillator", []() {
        return std::make_unique<nodes::SineOscillatorNode>();
    });
    factory.registerNode("WhiteNoise", []() {
        return std::make_unique<nodes::WhiteNoiseNode>();
    });
    factory.registerNode("BiquadFilter", []() {
        return std::make_unique<nodes::BiquadFilterNode>();
    });
    factory.registerNode("Gain", []() {
        return std::make_unique<nodes::GainNode>();
    });
    factory.registerNode("Mixer", []() {
        return std::make_unique<nodes::MixerNode>();
    });
    factory.registerNode("Panner", []() {
        return std::make_unique<nodes::PannerNode>();
    });
    factory.registerNode("AudioOutput", []() {
        return std::make_unique<nodes::AudioOutputNode>();
    });
    factory.registerNode("AudioFileWriter", []() {
        return std::make_unique<nodes::AudioFileWriterNode>();
    });
    factory.registerNode("AudioFileReader", []() {
        return std::make_unique<nodes::AudioFileReaderNode>();
    });
    factory.registerNode("GASpatializer", []() {
        return std::make_unique<nodes::GASpatializerNode>();
    });
}

} // namespace arxglue::sound