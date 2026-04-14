#pragma once
#include "../core/node.hpp"
#include "audio_buffer.hpp"
#include "audio_spec.hpp"
#include "audio_context.hpp"

namespace arxglue::sound {

class AudioNode : public INode {
public:
    virtual ~AudioNode() = default;

    // Вызывается при изменении спецификации аудиопотока
    virtual void prepare(const AudioSpec& spec) = 0;

    // Основной метод обработки аудиобуфера
    virtual void process(AudioBuffer& output) = 0;

    // Сброс внутреннего состояния (например, фазы осциллятора)
    virtual void reset() = 0;

    // Реализация INode::execute для аудиоузлов.
    // По умолчанию предполагаем, что контекст содержит указатель на AudioBuffer
    // в ctx.input и ctx.output, или использует соглашение через state.
    void execute(Context& ctx) override {
        // Получаем входной буфер (может отсутствовать)
        AudioBuffer* inputBuffer = nullptr;
        if (ctx.input.has_value()) {
            if (ctx.input.type() == typeid(std::shared_ptr<AudioBuffer>)) {
                auto ptr = std::any_cast<std::shared_ptr<AudioBuffer>>(ctx.input);
                inputBuffer = ptr.get();
            } else if (ctx.input.type() == typeid(AudioBuffer*)) {
                inputBuffer = std::any_cast<AudioBuffer*>(ctx.input);
            }
        }

        // Получаем или создаём выходной буфер
        std::shared_ptr<AudioBuffer> outputBuffer;
        if (ctx.output.has_value() && ctx.output.type() == typeid(std::shared_ptr<AudioBuffer>)) {
            outputBuffer = std::any_cast<std::shared_ptr<AudioBuffer>>(ctx.output);
        } else {
            // Если выходной буфер не передан, создаём новый с той же спецификацией, что и входной
            AudioSpec spec;
            if (inputBuffer) {
                spec = inputBuffer->getSpec();
            } else {
                // Берём спецификацию из аудиоконтекста
                auto* audioCtx = getAudioContextData(ctx);
                if (audioCtx) spec = audioCtx->spec;
            }
            outputBuffer = std::make_shared<AudioBuffer>(spec);
            ctx.output = outputBuffer;
        }

        // Если есть входной буфер и его спецификация отличается от подготовленной,
        // вызываем prepare с новой спецификацией.
        if (inputBuffer) {
            const AudioSpec& inputSpec = inputBuffer->getSpec();
            if (!m_prepared || inputSpec != m_lastSpec) {
                prepare(inputSpec);
                m_lastSpec = inputSpec;
                m_prepared = true;
            }
            // Копируем входные данные в выходной буфер (если узел не модифицирует на месте)
            // Примечание: конкретные узлы могут переопределять execute для оптимизации.
            *outputBuffer = std::move(*inputBuffer); // предполагаем наличие перемещающего присваивания
        } else {
            // Узел-генератор: подготавливаемся с текущей спецификацией из аудиоконтекста
            auto* audioCtx = getAudioContextData(ctx);
            if (audioCtx) {
                if (!m_prepared || audioCtx->spec != m_lastSpec) {
                    prepare(audioCtx->spec);
                    m_lastSpec = audioCtx->spec;
                    m_prepared = true;
                }
            }
        }

        // Выполняем обработку
        process(*outputBuffer);
    }

    // Утилита для доступа к AudioContextData из Context
    static AudioContextData* getAudioContextData(Context& ctx) {
        return sound::getAudioContextData(ctx);
    }

protected:
    bool m_prepared = false;
    AudioSpec m_lastSpec;
};

} // namespace arxglue::sound