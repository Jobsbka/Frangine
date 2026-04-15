// tests/test_arxsound_arxglue_integration.cpp
#include "../include/arxsound.hpp"
#include "../arxglue/arxsound_node.hpp"
#include "../src/core/executor.hpp"
#include "../src/core/graph.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <memory>

namespace arxsound::arxglue::test {

// ============================================================================
// Test utilities
// ============================================================================

static int g_integration_tests_passed = 0;
static int g_integration_tests_failed = 0;

#define INTEGRATION_TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            std::cout << "[PASS] " << message << std::endl; \
            g_integration_tests_passed++; \
        } else { \
            std::cout << "[FAIL] " << message << std::endl; \
            g_integration_tests_failed++; \
        } \
    } while(0)

// ============================================================================
// Test: AudioDeviceNode creation
// ============================================================================

static void test_audio_device_node_creation() {
    std::cout << "\n=== Integration Test: AudioDeviceNode Creation ===" << std::endl;
    
    auto node = std::make_unique<AudioDeviceNode>();
    INTEGRATION_TEST_ASSERT(node != nullptr, "AudioDeviceNode should be created");
    
    auto metadata = node->getMetadata();
    INTEGRATION_TEST_ASSERT(metadata.name == "AudioDevice", "Node name should be AudioDevice");
    INTEGRATION_TEST_ASSERT(metadata.threadSafe, "Node should be thread-safe");
    INTEGRATION_TEST_ASSERT(!metadata.inputs.empty(), "Node should have inputs");
    INTEGRATION_TEST_ASSERT(!metadata.outputs.empty(), "Node should have outputs");
    
    // Test parameter setting
    node->setParameter("channels", std::any(2));
    node->setParameter("sample_rate", std::any(48000));
    node->setParameter("volume", std::any(0.8f));
    
    auto channels = node->getParameter("channels");
    INTEGRATION_TEST_ASSERT(std::any_cast<uint32_t>(channels) == 2, "Channels should be 2");
    
    auto sample_rate = node->getParameter("sample_rate");
    INTEGRATION_TEST_ASSERT(std::any_cast<uint32_t>(sample_rate) == 48000, "Sample rate should be 48000");
}

// ============================================================================
// Test: AudioDeviceNode start/stop
// ============================================================================

static void test_audio_device_node_start_stop() {
    std::cout << "\n=== Integration Test: AudioDeviceNode Start/Stop ===" << std::endl;
    
    auto node = std::make_unique<AudioDeviceNode>();
    
    // Set auto-start to false for manual control
    node->setParameter("auto_start", std::any(false));
    
    // Create context and execute
    arxglue::Context ctx;
    ctx.setState("start", true);
    
    node->execute(ctx);
    
    // Give it time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    bool is_running = false;
    try {
        is_running = ctx.getState<bool>("is_running");
    } catch (...) {}
    
    INTEGRATION_TEST_ASSERT(is_running, "Device should be running after start");
    
    // Stop
    ctx.setState("stop", true);
    ctx.setState("start", false);
    node->execute(ctx);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    try {
        is_running = ctx.getState<bool>("is_running");
    } catch (...) {}
    
    INTEGRATION_TEST_ASSERT(!is_running, "Device should be stopped after stop command");
}

// ============================================================================
// Test: AudioBufferNode
// ============================================================================

static void test_audio_buffer_node() {
    std::cout << "\n=== Integration Test: AudioBufferNode ===" << std::endl;
    
    auto node = std::make_unique<AudioBufferNode>();
    
    // Create test buffer (1 second of silence at 48kHz, stereo, F32)
    const uint32_t sample_rate = 48000;
    const uint32_t channels = 2;
    const uint32_t duration_sec = 1;
    const uint64_t frame_count = sample_rate * duration_sec;
    const size_t buffer_size = frame_count * channels * sizeof(float);
    
    std::vector<float> test_buffer(frame_count * channels, 0.0f);
    
    // Add a simple tone (440 Hz sine wave)
    for (uint64_t i = 0; i < frame_count; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sample_rate);
        float sample = std::sin(2.0f * 3.14159f * 440.0f * t) * 0.5f;
        test_buffer[i * channels] = sample;
        test_buffer[i * channels + 1] = sample;
    }
    
    node->set_buffer(
        test_buffer.data(),
        frame_count,
        AS_format::F32,
        channels,
        sample_rate
    );
    
    auto metadata = node->getMetadata();
    INTEGRATION_TEST_ASSERT(metadata.name == "AudioBuffer", "Node name should be AudioBuffer");
    INTEGRATION_TEST_ASSERT(metadata.pure, "AudioBufferNode should be pure");
    
    // Execute with trigger
    arxglue::Context ctx;
    ctx.setState("trigger", true);
    
    node->execute(ctx);
    
    uint64_t cursor = 0;
    try {
        cursor = ctx.getState<uint64_t>("cursor");
    } catch (...) {}
    
    INTEGRATION_TEST_ASSERT(cursor > 0, "Cursor should advance after trigger");
    
    // Test reset
    ctx.setState("reset", true);
    node->execute(ctx);
    
    try {
        cursor = ctx.getState<uint64_t>("cursor");
    } catch (...) {}
    
    INTEGRATION_TEST_ASSERT(cursor == 0, "Cursor should reset to 0");
}

// ============================================================================
// Test: Graph integration
// ============================================================================

static void test_graph_integration() {
    std::cout << "\n=== Integration Test: Graph Integration ===" << std::endl;
    
    arxglue::Graph graph;
    
    // Create AudioDeviceNode
    auto device_node = std::make_unique<AudioDeviceNode>();
    device_node->setParameter("auto_start", std::any(false));
    device_node->setParameter("channels", std::any(2));
    device_node->setParameter("sample_rate", std::any(48000));
    arxglue::NodeId device_id = graph.addNode(std::move(device_node));
    
    // Create AudioBufferNode
    auto buffer_node = std::make_unique<AudioBufferNode>();
    const uint64_t frame_count = 48000;  // 1 second
    std::vector<float> test_buffer(frame_count * 2, 0.0f);
    buffer_node->set_buffer(
        test_buffer.data(),
        frame_count,
        AS_format::F32,
        2,
        48000
    );
    arxglue::NodeId buffer_id = graph.addNode(std::move(buffer_node));
    
    INTEGRATION_TEST_ASSERT(device_id > 0, "Device node should be added");
    INTEGRATION_TEST_ASSERT(buffer_id > 0, "Buffer node should be added");
    
    // Connect nodes (buffer output -> device input)
    arxglue::Connection conn;
    conn.srcNode = buffer_id;
    conn.srcPort = 0;
    conn.dstNode = device_id;
    conn.dstPort = 0;
    
    AS_result result = static_cast<AS_result>(graph.addConnection(conn));
    INTEGRATION_TEST_ASSERT(result == AS_result::SUCCESS || result == AS_result(0), 
                           "Connection should be added");
    
    // Execute graph
    arxglue::Executor executor(1);
    arxglue::Context ctx;
    
    ctx.setState("trigger", true);
    ctx.setState("start", true);
    
    executor.execute(graph, ctx, {device_id});
    
    // Check execution
    bool device_running = false;
    try {
        device_running = ctx.getState<bool>("is_running");
    } catch (...) {}
    
    INTEGRATION_TEST_ASSERT(device_running, "Device should be running in graph");
}

// ============================================================================
// Test: Serialization/Deserialization
// ============================================================================

static void test_serialization() {
    std::cout << "\n=== Integration Test: Serialization ===" << std::endl;
    
    auto node = std::make_unique<AudioDeviceNode>();
    node->setParameter("channels", std::any(4));
    node->setParameter("sample_rate", std::any(96000));
    node->setParameter("volume", std::any(0.75f));
    node->setParameter("auto_start", std::any(true));
    
    // Serialize
    nlohmann::json j;
    node->serialize(j);
    
    INTEGRATION_TEST_ASSERT(j.contains("type"), "Serialized JSON should contain type");
    INTEGRATION_TEST_ASSERT(j.contains("params"), "Serialized JSON should contain params");
    INTEGRATION_TEST_ASSERT(j["params"]["channels"] == 4, "Channels should be serialized");
    INTEGRATION_TEST_ASSERT(j["params"]["sample_rate"] == 96000, "Sample rate should be serialized");
    
    // Deserialize
    auto node2 = std::make_unique<AudioDeviceNode>();
    node2->deserialize(j);
    
    auto channels = node2->getParameter("channels");
    INTEGRATION_TEST_ASSERT(std::any_cast<uint32_t>(channels) == 4, 
                           "Channels should be deserialized correctly");
    
    auto sample_rate = node2->getParameter("sample_rate");
    INTEGRATION_TEST_ASSERT(std::any_cast<uint32_t>(sample_rate) == 96000, 
                           "Sample rate should be deserialized correctly");
}

// ============================================================================
// Test: Thread safety
// ============================================================================

static void test_thread_safety() {
    std::cout << "\n=== Integration Test: Thread Safety ===" << std::endl;
    
    auto node = std::make_unique<AudioDeviceNode>();
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    
    // Launch multiple threads accessing the same node
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&node, &success_count, &error_count, i]() {
            try {
                arxglue::Context ctx;
                ctx.setState("volume", std::any(0.5f + i * 0.1f));
                node->execute(ctx);
                success_count++;
            } catch (...) {
                error_count++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    INTEGRATION_TEST_ASSERT(success_count == 4, "All threads should complete successfully");
    INTEGRATION_TEST_ASSERT(error_count == 0, "No threads should error");
}

// ============================================================================
// Test: Executor integration
// ============================================================================

static void test_executor_integration() {
    std::cout << "\n=== Integration Test: Executor Integration ===" << std::endl;
    
    arxglue::Graph graph;
    
    // Create multiple audio nodes
    auto device_node = std::make_unique<AudioDeviceNode>();
    device_node->setParameter("auto_start", std::any(false));
    arxglue::NodeId device_id = graph.addNode(std::move(device_node));
    
    auto buffer_node1 = std::make_unique<AudioBufferNode>();
    arxglue::NodeId buffer1_id = graph.addNode(std::move(buffer_node1));
    
    auto buffer_node2 = std::make_unique<AudioBufferNode>();
    arxglue::NodeId buffer2_id = graph.addNode(std::move(buffer_node2));
    
    // Execute with multiple threads
    arxglue::Executor executor(2);  // 2 worker threads
    arxglue::Context ctx;
    
    ctx.setState("start", true);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    executor.execute(graph, ctx, {device_id});
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    INTEGRATION_TEST_ASSERT(duration.count() < 5000, "Execution should complete in reasonable time");
    
    // Verify graph state
    INTEGRATION_TEST_ASSERT(graph.getNodes().size() == 3, "Graph should have 3 nodes");
}

// ============================================================================
// Test: Resource management
// ============================================================================

static void test_resource_management() {
    std::cout << "\n=== Integration Test: Resource Management ===" << std::endl;
    
    // Test RAII behavior
    {
        auto node = std::make_unique<AudioDeviceNode>();
        node->setParameter("auto_start", std::any(true));
        
        arxglue::Context ctx;
        node->execute(ctx);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Node should be running
        bool is_running = false;
        try {
            is_running = ctx.getState<bool>("is_running");
        } catch (...) {}
        
        INTEGRATION_TEST_ASSERT(is_running, "Device should be running");
        
        // Node will be destroyed here (RAII)
    }
    
    // After destruction, resources should be cleaned up
    // (No crash = success)
    INTEGRATION_TEST_ASSERT(true, "RAII cleanup should not crash");
}

// ============================================================================
// Run all integration tests
// ============================================================================

static void run_all_integration_tests() {
    std::cout << "========================================" << std::endl;
    std::cout << "ArxSound ArxGlue Integration Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_audio_device_node_creation();
    test_audio_device_node_start_stop();
    test_audio_buffer_node();
    test_graph_integration();
    test_serialization();
    test_thread_safety();
    test_executor_integration();
    test_resource_management();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Integration Test Results:" << std::endl;
    std::cout << "  Passed: " << g_integration_tests_passed << std::endl;
    std::cout << "  Failed: " << g_integration_tests_failed << std::endl;
    std::cout << "========================================" << std::endl;
}

} // namespace arxsound::arxglue::test

int main() {
    // Initialize ArxGlue core module
    arxglue::core::initializeModule();
    
    // Register audio nodes
    arxsound::arxglue::register_audio_nodes();
    
    arxsound::arxglue::test::run_all_integration_tests();
    return (arxsound::arxglue::test::g_integration_tests_failed > 0) ? 1 : 0;
}