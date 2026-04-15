// tests/main.cpp
#include "arxsound.hpp"
#include "test_utils.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>
#include <atomic>

#if defined(AS_HAS_ARXGLUE)
#include "arxsound_node.hpp"
#include "../src/core/executor.hpp"
#include "../src/core/graph.hpp"
#endif

// ============================================================================
// Test Registry
// ============================================================================
using TestFunction = void(*)();

struct TestInfo {
    const char* name;
    TestFunction func;
    bool passed;
    double duration_ms;
};

static std::vector<TestInfo> g_tests;
static std::atomic<int> g_total_tests{0};
static std::atomic<int> g_passed_tests{0};
static std::atomic<int> g_failed_tests{0};

#define REGISTER_TEST(name, func) \
    static void __test_##name(); \
    static struct __TestRegistrar_##name { \
        __TestRegistrar_##name() { \
            g_tests.push_back({#name, __test_##name, false, 0.0}); \
        } \
    } __registrar_##name; \
    static void __test_##name()

// ============================================================================
// Test Macros
// ============================================================================
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            g_passed_tests++; \
            std::cout << "  [PASS] " << message << std::endl; \
        } else { \
            g_failed_tests++; \
            std::cout << "  [FAIL] " << message << std::endl; \
        } \
        g_total_tests++; \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

#define TEST_ASSERT_NEAR(expected, actual, epsilon, message) \
    TEST_ASSERT(std::abs((expected) - (actual)) < (epsilon), message)

// ============================================================================
// Core Tests
// ============================================================================

REGISTER_TEST(ResultCodes, {
    TEST_ASSERT_EQ(0, static_cast<int>(arxsound::AS_result::SUCCESS), "SUCCESS should be 0");
    TEST_ASSERT_EQ(-1, static_cast<int>(arxsound::AS_result::ERROR), "ERROR should be -1");
    TEST_ASSERT_EQ(-2, static_cast<int>(arxsound::AS_result::INVALID_ARGS), "INVALID_ARGS should be -2");
    TEST_ASSERT(arxsound::AS_result_is_success(arxsound::AS_result::SUCCESS), "SUCCESS check");
    TEST_ASSERT(arxsound::AS_result_is_error(arxsound::AS_result::ERROR), "ERROR check");
});

REGISTER_TEST(FormatUtilities, {
    TEST_ASSERT_EQ(1, arxsound::AS_bytes_per_sample(arxsound::AS_format::U8), "U8 = 1 byte");
    TEST_ASSERT_EQ(2, arxsound::AS_bytes_per_sample(arxsound::AS_format::S16), "S16 = 2 bytes");
    TEST_ASSERT_EQ(3, arxsound::AS_bytes_per_sample(arxsound::AS_format::S24), "S24 = 3 bytes");
    TEST_ASSERT_EQ(4, arxsound::AS_bytes_per_sample(arxsound::AS_format::F32), "F32 = 4 bytes");
    TEST_ASSERT_EQ(0, arxsound::AS_bytes_per_sample(arxsound::AS_format::UNKNOWN), "UNKNOWN = 0 bytes");
    
    TEST_ASSERT_EQ(4, arxsound::AS_bytes_per_frame(arxsound::AS_format::F32, 1), "F32 mono");
    TEST_ASSERT_EQ(8, arxsound::AS_bytes_per_frame(arxsound::AS_format::F32, 2), "F32 stereo");
});

REGISTER_TEST(LogSystem, {
    arxsound::AS_log log;
    arxsound::AS_log_config config = arxsound::AS_log_config_init();
    config.min_level = arxsound::AS_log_level::DEBUG;
    
    arxsound::AS_result result = log.init(&config);
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Log init");
    
    log.post(arxsound::AS_log_level::INFO, "Test message %d", 42);
    log.post(arxsound::AS_log_level::ERROR, "Error message");
    
    log.uninit();
});

REGISTER_TEST(ContextInitialization, {
    arxsound::AS_context context;
    arxsound::AS_context_config config = arxsound::AS_context_config_init();
    config.log = arxsound::AS_log::default_log();
    
    arxsound::AS_result result = context.init(nullptr, 0, &config);
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Context init");
    TEST_ASSERT(context.log() != nullptr, "Context has log");
    
    context.uninit();
});

REGISTER_TEST(DeviceInitialization, {
    arxsound::AS_context context;
    arxsound::AS_context_config ctx_config = arxsound::AS_context_config_init();
    ctx_config.log = arxsound::AS_log::default_log();
    context.init(nullptr, 0, &ctx_config);
    
    static uint32_t callback_count = 0;
    auto callback = [](arxsound::AS_device* dev, void* out, const void* in, uint32_t frames) {
        callback_count += frames;
        if (out) {
            auto info = dev->playback_info();
            memset(out, 0, frames * info.channels * arxsound::AS_bytes_per_sample(info.format));
        }
    };
    
    arxsound::AS_device device;
    arxsound::AS_device_config dev_config = arxsound::AS_device_config_init(arxsound::AS_device_type::PLAYBACK);
    dev_config.playback.format = arxsound::AS_format::F32;
    dev_config.playback.channels = 2;
    dev_config.sample_rate = 48000;
    dev_config.data_callback = callback;
    
    arxsound::AS_result result = device.init(&context, &dev_config);
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Device init");
    TEST_ASSERT_EQ(arxsound::AS_device_state::STOPPED, device.state(), "Device state");
    
    device.uninit();
    context.uninit();
});

REGISTER_TEST(DeviceStartStop, {
    arxsound::AS_context context;
    arxsound::AS_context_config ctx_config = arxsound::AS_context_config_init();
    ctx_config.log = arxsound::AS_log::default_log();
    context.init(nullptr, 0, &ctx_config);
    
    static std::atomic<uint32_t> callback_frames{0};
    auto callback = [](arxsound::AS_device* dev, void* out, const void* in, uint32_t frames) {
        callback_frames += frames;
        if (out) {
            auto info = dev->playback_info();
            memset(out, 0, frames * info.channels * arxsound::AS_bytes_per_sample(info.format));
        }
    };
    
    arxsound::AS_device device;
    arxsound::AS_device_config dev_config = arxsound::AS_device_config_init(arxsound::AS_device_type::PLAYBACK);
    dev_config.playback.format = arxsound::AS_format::F32;
    dev_config.playback.channels = 2;
    dev_config.sample_rate = 48000;
    dev_config.data_callback = callback;
    dev_config.period_size_in_frames = 256;
    
    device.init(&context, &dev_config);
    
    arxsound::AS_result result = device.start();
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Device start");
    TEST_ASSERT_EQ(arxsound::AS_device_state::STARTED, device.state(), "Device started state");
    TEST_ASSERT_EQ(arxsound::AS_TRUE, device.is_started(), "Device is_started");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    uint32_t frames = callback_frames.load();
    TEST_ASSERT(frames > 0, "Callback was called");
    
    result = device.stop();
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Device stop");
    TEST_ASSERT_EQ(arxsound::AS_device_state::STOPPED, device.state(), "Device stopped state");
    
    device.uninit();
    context.uninit();
});

REGISTER_TEST(VolumeControl, {
    arxsound::AS_context context;
    arxsound::AS_context_config ctx_config = arxsound::AS_context_config_init();
    ctx_config.log = arxsound::AS_log::default_log();
    context.init(nullptr, 0, &ctx_config);
    
    arxsound::AS_device device;
    arxsound::AS_device_config dev_config = arxsound::AS_device_config_init(arxsound::AS_device_type::PLAYBACK);
    dev_config.playback.format = arxsound::AS_format::F32;
    dev_config.playback.channels = 2;
    dev_config.sample_rate = 48000;
    dev_config.data_callback = [](arxsound::AS_device*, void*, const void*, uint32_t) {};
    
    device.init(&context, &dev_config);
    
    arxsound::AS_result result = device.set_master_volume(0.5f);
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Set volume");
    
    float volume = 0.0f;
    result = device.get_master_volume(&volume);
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Get volume");
    TEST_ASSERT_NEAR(0.5f, volume, 0.001f, "Volume value");
    
    result = device.set_master_volume(-0.1f);
    TEST_ASSERT_EQ(arxsound::AS_result::INVALID_ARGS, result, "Negative volume rejected");
    
    device.uninit();
    context.uninit();
});

// ============================================================================
// Conversion Tests
// ============================================================================

REGISTER_TEST(FrameConversion, {
    const uint32_t frames = 256;
    const uint32_t channels = 2;
    
    std::vector<float> input_f32(frames * channels, 0.5f);
    std::vector<int16_t> output_s16(frames * channels);
    
    arxsound::AS_result result = arxsound::AS_convert_frames(
        output_s16.data(), frames, arxsound::AS_format::S16, channels,
        input_f32.data(), frames, arxsound::AS_format::F32, channels,
        arxsound::AS_FALSE
    );
    TEST_ASSERT(arxsound::AS_result_is_success(result), "F32->S16 conversion");
    TEST_ASSERT(output_s16[0] > 16000 && output_s16[0] < 17000, "F32 0.5 -> S16 ~16384");
    
    // Round-trip
    std::vector<float> output_f32(frames * channels);
    result = arxsound::AS_convert_frames(
        output_f32.data(), frames, arxsound::AS_format::F32, channels,
        output_s16.data(), frames, arxsound::AS_format::S16, channels,
        arxsound::AS_FALSE
    );
    TEST_ASSERT(arxsound::AS_result_is_success(result), "S16->F32 conversion");
    
    float diff = std::abs(output_f32[0] - input_f32[0]);
    TEST_ASSERT(diff < 0.01f, "Round-trip accuracy");
});

REGISTER_TEST(Resampler, {
    arxsound::AS_linear_resampler resampler;
    arxsound::AS_linear_resampler_config config{};
    config.format = arxsound::AS_format::F32;
    config.channels = 1;
    config.sample_rate_in = 48000;
    config.sample_rate_out = 44100;
    
    arxsound::AS_result result = resampler.init(&config);
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Resampler init");
    TEST_ASSERT_EQ(48000, resampler.input_sample_rate(), "Input sample rate");
    TEST_ASSERT_EQ(44100, resampler.output_sample_rate(), "Output sample rate");
    
    const uint32_t input_frames = 480;
    const uint32_t output_frames_max = 441;
    std::vector<float> input(input_frames, 0.5f);
    std::vector<float> output(output_frames_max);
    uint64_t output_frame_count = output_frames_max;
    
    result = resampler.process(
        output.data(), &output_frame_count,
        input.data(), input_frames
    );
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Resampler process");
    TEST_ASSERT(output_frame_count > 0, "Output frames produced");
    
    resampler.uninit();
});

REGISTER_TEST(ChannelRemapping, {
    const uint32_t frames = 256;
    
    // Mono to stereo
    std::vector<float> input_mono(frames, 0.5f);
    std::vector<float> output_stereo(frames * 2);
    
    arxsound::AS_result result = arxsound::AS_convert_frames(
        output_stereo.data(), frames, arxsound::AS_format::F32, 2,
        input_mono.data(), frames, arxsound::AS_format::F32, 1,
        arxsound::AS_FALSE
    );
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Mono->Stereo");
    TEST_ASSERT_EQ(output_stereo[0], output_stereo[1], "Channels duplicated");
    
    // Stereo to mono
    std::vector<float> input_stereo(frames * 2, 0.5f);
    std::vector<float> output_mono(frames);
    
    result = arxsound::AS_convert_frames(
        output_mono.data(), frames, arxsound::AS_format::F32, 1,
        input_stereo.data(), frames, arxsound::AS_format::F32, 2,
        arxsound::AS_FALSE
    );
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Stereo->Mono");
});

// ============================================================================
// Quick Start API Tests
// ============================================================================

REGISTER_TEST(QuickStartAPI, {
    arxsound::AS_device* device = nullptr;
    
    arxsound::AS_result result = arxsound::AS_init_and_start_device(
        arxsound::AS_device_type::PLAYBACK,
        arxsound::AS_format::F32,
        2,
        48000,
        [](arxsound::AS_device* dev, void* out, const void* in, uint32_t frames) {
            if (out) {
                auto info = dev->playback_info();
                memset(out, 0, frames * info.channels * arxsound::AS_bytes_per_sample(info.format));
            }
        },
        nullptr,
        &device
    );
    
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Quick start");
    TEST_ASSERT(device != nullptr, "Device created");
    TEST_ASSERT_EQ(arxsound::AS_TRUE, device->is_started(), "Device started");
    
    arxsound::AS_stop_and_uninit_device(device);
});

REGISTER_TEST(BackendEnumeration, {
    arxsound::AS_context context;
    arxsound::AS_context_config config = arxsound::AS_context_config_init();
    config.log = arxsound::AS_log::default_log();
    context.init(nullptr, 0, &config);
    
    uint32_t device_count = 0;
    arxsound::AS_result result = context.enumerate_devices(
        arxsound::AS_device_type::PLAYBACK,
        [](arxsound::AS_context*, arxsound::AS_device_type, const arxsound::AS_device_info* info, void* user_data) {
            uint32_t* count = static_cast<uint32_t*>(user_data);
            (*count)++;
            std::cout << "    Found: " << info->name << std::endl;
            return arxsound::AS_TRUE;
        },
        &device_count
    );
    
    TEST_ASSERT(arxsound::AS_result_is_success(result), "Enumeration");
    TEST_ASSERT(device_count > 0, "At least one device");
    
    context.uninit();
});

// ============================================================================
// ArxGlue Integration Tests
// ============================================================================

#if defined(AS_HAS_ARXGLUE)

REGISTER_TEST(AudioDeviceNodeCreation, {
    auto node = std::make_unique<arxsound::arxglue::AudioDeviceNode>();
    TEST_ASSERT(node != nullptr, "Node created");
    
    auto metadata = node->getMetadata();
    TEST_ASSERT(metadata.name == "AudioDevice", "Node name");
    TEST_ASSERT(metadata.threadSafe, "Thread safe");
    
    node->setParameter("channels", std::any(static_cast<uint32_t>(2)));
    node->setParameter("sample_rate", std::any(static_cast<uint32_t>(48000)));
    
    auto channels = node->getParameter("channels");
    TEST_ASSERT_EQ(2, std::any_cast<uint32_t>(channels), "Channels set");
});

REGISTER_TEST(AudioBufferNode, {
    auto node = std::make_unique<arxsound::arxglue::AudioBufferNode>();
    
    const uint32_t sample_rate = 48000;
    const uint32_t channels = 2;
    const uint64_t frame_count = sample_rate;
    std::vector<float> test_buffer(frame_count * channels, 0.0f);
    
    node->set_buffer(
        test_buffer.data(),
        frame_count,
        arxsound::AS_format::F32,
        channels,
        sample_rate
    );
    
    auto metadata = node->getMetadata();
    TEST_ASSERT(metadata.name == "AudioBuffer", "Node name");
    TEST_ASSERT(metadata.pure, "Pure node");
    
    arxglue::Context ctx;
    ctx.setState("trigger", true);
    node->execute(ctx);
    
    uint64_t cursor = ctx.getState<uint64_t>("cursor");
    TEST_ASSERT(cursor > 0, "Cursor advanced");
});

REGISTER_TEST(GraphIntegration, {
    arxglue::Graph graph;
    
    auto device_node = std::make_unique<arxsound::arxglue::AudioDeviceNode>();
    device_node->setParameter("auto_start", std::any(false));
    arxglue::NodeId device_id = graph.addNode(std::move(device_node));
    
    auto buffer_node = std::make_unique<arxsound::arxglue::AudioBufferNode>();
    arxglue::NodeId buffer_id = graph.addNode(std::move(buffer_node));
    
    TEST_ASSERT(device_id > 0, "Device node added");
    TEST_ASSERT(buffer_id > 0, "Buffer node added");
    
    arxglue::Executor executor(1);
    arxglue::Context ctx;
    ctx.setState("start", true);
    
    executor.execute(graph, ctx, {device_id});
    
    bool running = ctx.getState<bool>("is_running");
    TEST_ASSERT(running, "Device running in graph");
});

REGISTER_TEST(Serialization, {
    auto node = std::make_unique<arxsound::arxglue::AudioDeviceNode>();
    node->setParameter("channels", std::any(static_cast<uint32_t>(4)));
    node->setParameter("sample_rate", std::any(static_cast<uint32_t>(96000)));
    
    nlohmann::json j;
    node->serialize(j);
    
    TEST_ASSERT(j.contains("type"), "Has type");
    TEST_ASSERT(j.contains("params"), "Has params");
    TEST_ASSERT_EQ(4, j["params"]["channels"], "Channels serialized");
    
    auto node2 = std::make_unique<arxsound::arxglue::AudioDeviceNode>();
    node2->deserialize(j);
    
    auto channels = node2->getParameter("channels");
    TEST_ASSERT_EQ(4, std::any_cast<uint32_t>(channels), "Deserialized");
});

REGISTER_TEST(ThreadSafety, {
    auto node = std::make_unique<arxsound::arxglue::AudioDeviceNode>();
    std::atomic<int> success_count{0};
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&node, &success_count]() {
            try {
                arxglue::Context ctx;
                ctx.setState("volume", std::any(0.5f));
                node->execute(ctx);
                success_count++;
            } catch (...) {}
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    TEST_ASSERT_EQ(4, success_count.load(), "All threads completed");
});

#endif // AS_HAS_ARXGLUE

// ============================================================================
// Test Runner
// ============================================================================

void print_header() {
    std::cout << "========================================" << std::endl;
    std::cout << "  ArxSound Library Test Suite" << std::endl;
    std::cout << "  Version: " << AS_VERSION_STRING << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
}

void print_summary() {
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Total Tests:  " << g_total_tests.load() << std::endl;
    std::cout << "  Passed:       " << g_passed_tests.load() << std::endl;
    std::cout << "  Failed:       " << g_failed_tests.load() << std::endl;
    
    double pass_rate = (g_total_tests.load() > 0) 
        ? (100.0 * g_passed_tests.load() / g_total_tests.load()) 
        : 0.0;
    std::cout << "  Pass Rate:    " << pass_rate << "%" << std::endl;
    std::cout << "========================================" << std::endl;
    
    if (g_failed_tests.load() > 0) {
        std::cout << std::endl;
        std::cout << "  [WARNING] Some tests failed!" << std::endl;
        std::cout << std::endl;
    } else {
        std::cout << std::endl;
        std::cout << "  [SUCCESS] All tests passed!" << std::endl;
        std::cout << std::endl;
    }
}

int main(int argc, char* argv[]) {
    print_header();
    
    // Parse command line arguments
    bool run_all = true;
    std::string filter;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --filter <name>  Run only tests matching <name>" << std::endl;
            std::cout << "  --list           List all available tests" << std::endl;
            std::cout << "  --help, -h       Show this help" << std::endl;
            return 0;
        } else if (arg == "--list") {
            std::cout << "Available tests:" << std::endl;
            for (const auto& test : g_tests) {
                std::cout << "  " << test.name << std::endl;
            }
            return 0;
        } else if (arg == "--filter" && i + 1 < argc) {
            filter = argv[++i];
            run_all = false;
        }
    }
    
    // Run tests
    std::cout << "Running tests..." << std::endl;
    std::cout << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (auto& test : g_tests) {
        // Apply filter
        if (!run_all && filter.size() > 0) {
            if (std::string(test.name).find(filter) == std::string::npos) {
                continue;
            }
        }
        
        std::cout << "[" << test.name << "]" << std::endl;
        
        auto test_start = std::chrono::high_resolution_clock::now();
        
        try {
            test.func();
            test.passed = (g_failed_tests.load() == 0);
        } catch (const std::exception& e) {
            std::cout << "  [EXCEPTION] " << e.what() << std::endl;
            g_failed_tests++;
            g_total_tests++;
            test.passed = false;
        } catch (...) {
            std::cout << "  [EXCEPTION] Unknown exception" << std::endl;
            g_failed_tests++;
            g_total_tests++;
            test.passed = false;
        }
        
        auto test_end = std::chrono::high_resolution_clock::now();
        test.duration_ms = std::chrono::duration<double, std::milli>(test_end - test_start).count();
        
        std::cout << "  Duration: " << test.duration_ms << " ms" << std::endl;
        std::cout << std::endl;
        
        // Reset failed count for next test
        g_failed_tests = 0;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double total_duration = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    std::cout << "Total duration: " << total_duration << " ms" << std::endl;
    
    print_summary();
    
    return (g_failed_tests.load() > 0) ? 1 : 0;
}