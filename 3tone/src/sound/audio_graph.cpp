#include "audio_graph.hpp"
#include "../nodes/soundn/io_nodes.hpp"
#include "../core/executor.hpp"
#include "../core/context.hpp"
#include <stdexcept>
#include <chrono>
#include <fstream>

namespace arxglue::sound {

AudioGraph::AudioGraph() = default;

void AudioGraph::collectAudioNodes() {
    m_audioNodes.clear();
    // Получаем все узлы графа в топологическом порядке
    std::vector<NodeId> sortedIds = topologicalSort();
    for (NodeId id : sortedIds) {
        INode* node = getNode(id);
        if (auto* audioNode = dynamic_cast<AudioNode*>(node)) {
            m_audioNodes.push_back(audioNode);
        }
    }
}

void AudioGraph::prepare(const AudioSpec& spec) {
    m_spec = spec;
    collectAudioNodes();
    for (auto* node : m_audioNodes) {
        node->prepare(spec);
    }
    m_prepared = true;
}

void AudioGraph::processBlock(AudioBuffer& output) {
    if (!m_prepared) {
        throw std::runtime_error("AudioGraph not prepared");
    }

    // Создаём контекст и помещаем в него выходной буфер
    Context ctx;
    ctx.output = std::shared_ptr<AudioBuffer>(&output, [](AudioBuffer*){}); // non-owning shared_ptr для совместимости

    // Устанавливаем аудиоконтекст
    AudioContextData audioCtx;
    audioCtx.spec = m_spec;
    audioCtx.isRealtime = true;
    setAudioContextData(ctx, &audioCtx);

    // Выполняем граф (предполагается, что узлы будут использовать ctx.output)
    Executor executor(1); // в реальном времени используем один поток
    executor.execute(*this, ctx, {});

    // Если output был заменён внутри, копируем обратно
    if (ctx.output.has_value() && ctx.output.type() == typeid(std::shared_ptr<AudioBuffer>)) {
        auto bufPtr = std::any_cast<std::shared_ptr<AudioBuffer>>(ctx.output);
        if (bufPtr.get() != &output) {
            output = std::move(*bufPtr);
        }
    }
}

void AudioGraph::renderToFile(const std::string& path, double durationSeconds) {
    if (!m_prepared) {
        throw std::runtime_error("AudioGraph not prepared");
    }

    size_t totalFrames = static_cast<size_t>(m_spec.sampleRate * durationSeconds);
    AudioBuffer fullBuffer(m_spec, totalFrames);
    fullBuffer.clear();

    size_t blockSize = m_spec.blockSize;
    size_t numBlocks = totalFrames / blockSize;
    size_t remainder = totalFrames % blockSize;

    AudioBuffer blockBuffer(m_spec, blockSize);

    // Создаём контекст для офлайн-рендеринга
    Context ctx;
    AudioContextData audioCtx;
    audioCtx.spec = m_spec;
    audioCtx.isRealtime = false;
    setAudioContextData(ctx, &audioCtx);

    Executor executor(1);

    for (size_t b = 0; b < numBlocks; ++b) {
        blockBuffer.clear();
        ctx.output = std::shared_ptr<AudioBuffer>(&blockBuffer, [](AudioBuffer*){});
        executor.execute(*this, ctx, {});

        if (ctx.output.has_value() && ctx.output.type() == typeid(std::shared_ptr<AudioBuffer>)) {
            auto bufPtr = std::any_cast<std::shared_ptr<AudioBuffer>>(ctx.output);
            if (bufPtr.get() != &blockBuffer) {
                blockBuffer = std::move(*bufPtr);
            }
        }

        // Копируем блок в полный буфер
        float* dest = fullBuffer.data() + b * blockSize * m_spec.numChannels;
        const float* src = blockBuffer.data();
        std::copy_n(src, blockSize * m_spec.numChannels, dest);
    }

    if (remainder > 0) {
        AudioBuffer remBuffer(m_spec, remainder);
        remBuffer.clear();
        ctx.output = std::shared_ptr<AudioBuffer>(&remBuffer, [](AudioBuffer*){});
        executor.execute(*this, ctx, {});

        if (ctx.output.has_value() && ctx.output.type() == typeid(std::shared_ptr<AudioBuffer>)) {
            auto bufPtr = std::any_cast<std::shared_ptr<AudioBuffer>>(ctx.output);
            if (bufPtr.get() != &remBuffer) {
                remBuffer = std::move(*bufPtr);
            }
        }

        float* dest = fullBuffer.data() + numBlocks * blockSize * m_spec.numChannels;
        const float* src = remBuffer.data();
        std::copy_n(src, remainder * m_spec.numChannels, dest);
    }

    // Сохраняем результат
    auto asset = std::make_shared<AudioAsset>(std::make_unique<AudioBuffer>(std::move(fullBuffer)));
    asset->saveToFile(path);
}

} // namespace arxglue::sound