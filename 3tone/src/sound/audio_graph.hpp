#pragma once
#include "../core/graph.hpp"
#include "audio_node.hpp"
#include "audio_spec.hpp"
#include <memory>
#include <vector>

namespace arxglue::sound {

class AudioGraph : public Graph {
public:
    AudioGraph();
    ~AudioGraph() = default; 

    // Подготовка всех аудиоузлов в графе к заданной спецификации
    void prepare(const AudioSpec& spec);

    // Обработка одного блока аудио (режим реального времени)
    // Входной буфер может быть передан через контекст
    void processBlock(AudioBuffer& output);

    // Офлайн-рендеринг в файл указанной длительности
    void renderToFile(const std::string& path, double durationSeconds);

    // Получить список аудиоузлов в порядке выполнения
    const std::vector<AudioNode*>& getAudioNodes() const { return m_audioNodes; }

private:
    AudioSpec m_spec;
    std::vector<AudioNode*> m_audioNodes;
    bool m_prepared = false;

    void collectAudioNodes();
};

} // namespace arxglue::sound