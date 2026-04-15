// tests/test_arxsound_core.cpp
#include "../include/arxsound.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>

namespace arxsound::test {

// ============================================================================
// Test utilities
// ============================================================================

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            std::cout << "[PASS] " << message << std::endl; \
            g_tests_passed++; \
        } else { \
            std::cout << "[FAIL] " << message << std::endl; \
            g_tests_failed++; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

// ============================================================================
// Test: Result codes
// ============================================================================

static void test_result_codes() {
    std::cout << "\n=== Test: Result Codes ===" << std::endl;
    
    TEST_ASSERT_EQ(0, static_cast<int>(AS_result::SUCCESS), "SUCCESS should be 0");
    TEST_ASSERT_EQ(-1, static_cast<int>(AS_result::ERROR), "ERROR should be -1");
    TEST_ASSERT_EQ(-2, static_cast<int>(AS_result::INVALID_ARGS), "INVALID_ARGS should be -2");
    
    TEST_ASSERT(AS_result_is_success(AS_result::SUCCESS), "SUCCESS should be success");
    TEST_ASSERT(!AS_result_is_success(AS_result::ERROR), "ERROR should not be success");
    TEST_ASSERT(AS_result_is_error(AS_result::ERROR), "ERROR should be error");
    TEST_ASSERT(!AS_result_is_error(AS_result::SUCCESS), "SUCCESS should not be error");
}

// ============================================================================
// Test: Format utilities
// ============================================================================

static void test_format_utilities() {
    std::cout << "\n=== Test: Format Utilities ===" << std::endl;
    
    TEST_ASSERT_EQ(1, AS_bytes_per_sample(AS_format::U8), "U8 should be 1 byte");
    TEST_ASSERT_EQ(2, AS_bytes_per_sample(AS_format::S16), "S16 should be 2 bytes");
    TEST_ASSERT_EQ(3, AS_bytes_per_sample(AS_format::S24), "S24 should be 3 bytes");
    TEST_ASSERT_EQ(4, AS_bytes_per_sample(AS_format::S32), "S32 should be 4 bytes");
    TEST_ASSERT_EQ(4, AS_bytes_per_sample(AS_format::F32), "F32 should be 4 bytes");
    TEST_ASSERT_EQ(0, AS_bytes_per_sample(AS_format::UNKNOWN), "UNKNOWN should be 0 bytes");
    
    TEST_ASSERT_EQ(2, AS_bytes_per_frame(AS_format::F32, 1), "F32 mono should be 4 bytes");
    TEST_ASSERT_EQ(4, AS_bytes_per_frame(AS_format::F32, 2), "F32 stereo should be 8 bytes");
    TEST_ASSERT_EQ(8, AS_bytes_per_frame(AS_format::S16, 2), "S16 stereo should be 4 bytes");
}

// ============================================================================
// Test: Log system
// ============================================================================

static std::vector<std::string> g_log_messages;

static void log_callback(const AS_log_message& msg) {
    g_log_messages.push_back(std::string(msg.message, msg.message_length));
}

static void test_log_system() {
    std::cout << "\n=== Test: Log System ===" << std::endl;
    
    AS_log log;
    AS_log_config config = AS_log_config_init();
    config.callback = log_callback;
    config.min_level = AS_log_level::DEBUG;
    
    AS_result result = log.init(&config);
    TEST_ASSERT_EQ(AS_result::SUCCESS, result, "Log init should succeed");
    
    log.post(AS_log_level::INFO, "Test message %d", 42);
    log.post(AS_log_level::ERROR, "Error message");
    
    TEST_ASSERT_EQ(2, static_cast<int>(g_log_messages.size()), "Should have 2 log messages");
    
    log.uninit();
    g_log_messages.clear();
}

// ============================================================================
// Test: Context initialization
// ============================================================================

static void test_context_init() {
    std::cout << "\n=== Test: Context Initialization ===" << std::endl;
    
    AS_context context;
    AS_context_config config = AS_context_config_init();
    config.log = AS_log::default_log();
    
    AS_result result = context.init(nullptr, 0, &config);
    TEST_ASSERT(AS_result_is_success(result), "Context init should succeed");
    
    TEST_ASSERT(context.log() != nullptr, "Context should have log");
    TEST_ASSERT_EQ(AS_backend::NULL_BACKEND, context.active_backend(), 
                   "Should use Null backend by default");
    
    context.uninit();
}

// ============================================================================
// Test: Device initialization (Null backend)
// ============================================================================

static uint32_t g_callback_frame_count = 0;
static void test_data_callback(AS_device* device, void* output, const void* input, uint32_t frame_count) {
    (void)device;
    (void)input;
    
    g_callback_frame_count += frame_count;
    
    // Fill with silence (F32)
    if (output) {
        float* f32_out = static_cast<float*>(output);
        AS_device::stream_info info = device->playback_info();
        uint32_t total_samples = frame_count * info.channels;
        for (uint32_t i = 0; i < total_samples; ++i) {
            f32_out[i] = 0.0f;
        }
    }
}

static void test_device_init() {
    std::cout << "\n=== Test: Device Initialization ===" << std::endl;
    
    AS_context context;
    AS_context_config ctx_config = AS_context_config_init();
    ctx_config.log = AS_log::default_log();
    
    AS_result result = context.init(nullptr, 0, &ctx_config);
    TEST_ASSERT(AS_result_is_success(result), "Context init should succeed");
    
    AS_device device;
    AS_device_config dev_config = AS_device_config_init(AS_device_type::PLAYBACK);
    dev_config.playback.format = AS_format::F32;
    dev_config.playback.channels = 2;
    dev_config.sample_rate = 48000;
    dev_config.data_callback = test_data_callback;
    
    result = device.init(&context, &dev_config);
    TEST_ASSERT(AS_result_is_success(result), "Device init should succeed");
    
    TEST_ASSERT_EQ(AS_device_state::STOPPED, device.state(), "Device should be stopped");
    TEST_ASSERT_EQ(AS_FALSE, device.is_started(), "Device should not be started");
    TEST_ASSERT_EQ(48000, device.sample_rate(), "Sample rate should be 48000");
    
    device.uninit();
    context.uninit();
}

// ============================================================================
// Test: Device start/stop
// ============================================================================

static void test_device_start_stop() {
    std::cout << "\n=== Test: Device Start/Stop ===" << std::endl;
    
    AS_context context;
    AS_context_config ctx_config = AS_context_config_init();
    ctx_config.log = AS_log::default_log();
    context.init(nullptr, 0, &ctx_config);
    
    AS_device device;
    AS_device_config dev_config = AS_device_config_init(AS_device_type::PLAYBACK);
    dev_config.playback.format = AS_format::F32;
    dev_config.playback.channels = 2;
    dev_config.sample_rate = 48000;
    dev_config.data_callback = test_data_callback;
    dev_config.period_size_in_frames = 256;
    
    device.init(&context, &dev_config);
    
    // Start
    AS_result result = device.start();
    TEST_ASSERT(AS_result_is_success(result), "Device start should succeed");
    TEST_ASSERT_EQ(AS_device_state::STARTED, device.state(), "Device should be started");
    TEST_ASSERT_EQ(AS_TRUE, device.is_started(), "Device should report started");
    
    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    uint32_t frames_before = g_callback_frame_count;
    TEST_ASSERT(frames_before > 0, "Callback should have been called");
    
    // Stop
    result = device.stop();
    TEST_ASSERT(AS_result_is_success(result), "Device stop should succeed");
    TEST_ASSERT_EQ(AS_device_state::STOPPED, device.state(), "Device should be stopped");
    TEST_ASSERT_EQ(AS_FALSE, device.is_started(), "Device should report stopped");
    
    device.uninit();
    context.uninit();
    g_callback_frame_count = 0;
}

// ============================================================================
// Test: Volume control
// ============================================================================

static void test_volume_control() {
    std::cout << "\n=== Test: Volume Control ===" << std::endl;
    
    AS_context context;
    AS_context_config ctx_config = AS_context_config_init();
    ctx_config.log = AS_log::default_log();
    context.init(nullptr, 0, &ctx_config);
    
    AS_device device;
    AS_device_config dev_config = AS_device_config_init(AS_device_type::PLAYBACK);
    dev_config.playback.format = AS_format::F32;
    dev_config.playback.channels = 2;
    dev_config.sample_rate = 48000;
    dev_config.data_callback = test_data_callback;
    
    device.init(&context, &dev_config);
    
    // Test volume set/get
    AS_result result = device.set_master_volume(0.5f);
    TEST_ASSERT(AS_result_is_success(result), "Set volume should succeed");
    
    float volume = 0.0f;
    result = device.get_master_volume(&volume);
    TEST_ASSERT(AS_result_is_success(result), "Get volume should succeed");
    TEST_ASSERT_EQ(0.5f, volume, "Volume should be 0.5");
    
    // Test invalid volume
    result = device.set_master_volume(-0.1f);
    TEST_ASSERT_EQ(AS_result::INVALID_ARGS, result, "Negative volume should fail");
    
    result = device.set_master_volume(1.5f);
    TEST_ASSERT_EQ(AS_result::INVALID_ARGS, result, "Volume > 1.0 should fail");
    
    device.uninit();
    context.uninit();
}

// ============================================================================
// Test: Frame conversion
// ============================================================================

static void test_frame_conversion() {
    std::cout << "\n=== Test: Frame Conversion ===" << std::endl;
    
    const uint32_t frames = 256;
    const uint32_t channels = 2;
    
    // F32 -> S16
    std::vector<float> input_f32(frames * channels, 0.5f);
    std::vector<int16_t> output_s16(frames * channels);
    
    AS_result result = AS_convert_frames(
        output_s16.data(), frames, AS_format::S16, channels,
        input_f32.data(), frames, AS_format::F32, channels,
        AS_FALSE
    );
    TEST_ASSERT(AS_result_is_success(result), "F32->S16 conversion should succeed");
    
    // Check conversion (0.5f should be ~16384 in S16)
    TEST_ASSERT(output_s16[0] > 16000 && output_s16[0] < 17000, 
                "F32 0.5 should convert to ~16384 S16");
    
    // S16 -> F32
    std::vector<float> output_f32(frames * channels);
    result = AS_convert_frames(
        output_f32.data(), frames, AS_format::F32, channels,
        output_s16.data(), frames, AS_format::S16, channels,
        AS_FALSE
    );
    TEST_ASSERT(AS_result_is_success(result), "S16->F32 conversion should succeed");
    
    // Check round-trip
    float diff = std::abs(output_f32[0] - input_f32[0]);
    TEST_ASSERT(diff < 0.01f, "Round-trip conversion should be accurate");
    
    // Channel conversion (mono -> stereo)
    std::vector<float> input_mono(frames, 0.5f);
    std::vector<float> output_stereo(frames * 2);
    
    result = AS_convert_frames(
        output_stereo.data(), frames, AS_format::F32, 2,
        input_mono.data(), frames, AS_format::F32, 1,
        AS_FALSE
    );
    TEST_ASSERT(AS_result_is_success(result), "Mono->Stereo conversion should succeed");
    TEST_ASSERT_EQ(output_stereo[0], output_stereo[1], "Mono->Stereo should duplicate channels");
}

// ============================================================================
// Test: Resampler
// ============================================================================

static void test_resampler() {
    std::cout << "\n=== Test: Resampler ===" << std::endl;
    
    AS_linear_resampler resampler;
    AS_linear_resampler_config config{};
    config.format = AS_format::F32;
    config.channels = 1;
    config.sample_rate_in = 48000;
    config.sample_rate_out = 44100;
    
    AS_result result = resampler.init(&config);
    TEST_ASSERT(AS_result_is_success(result), "Resampler init should succeed");
    
    TEST_ASSERT_EQ(48000, resampler.input_sample_rate(), "Input sample rate should be 48000");
    TEST_ASSERT_EQ(44100, resampler.output_sample_rate(), "Output sample rate should be 44100");
    TEST_ASSERT_EQ(1, resampler.channels(), "Channels should be 1");
    
    // Process some frames
    const uint32_t input_frames = 480;
    const uint32_t output_frames_max = 441;
    std::vector<float> input(input_frames, 0.5f);
    std::vector<float> output(output_frames_max);
    uint64_t output_frame_count = output_frames_max;
    
    result = resampler.process(
        output.data(), &output_frame_count,
        input.data(), input_frames
    );
    TEST_ASSERT(AS_result_is_success(result), "Resampler process should succeed");
    TEST_ASSERT(output_frame_count > 0, "Should produce output frames");
    
    resampler.uninit();
}

// ============================================================================
// Test: Quick start API
// ============================================================================

static void test_quick_start_api() {
    std::cout << "\n=== Test: Quick Start API ===" << std::endl;
    
    AS_device* device = nullptr;
    AS_result result = AS_init_and_start_device(
        AS_device_type::PLAYBACK,
        AS_format::F32,
        2,
        48000,
        test_data_callback,
        nullptr,
        &device
    );
    
    TEST_ASSERT(AS_result_is_success(result), "Quick start should succeed");
    TEST_ASSERT(device != nullptr, "Device should be created");
    TEST_ASSERT_EQ(AS_TRUE, device->is_started(), "Device should be started");
    
    AS_stop_and_uninit_device(device);
}

// ============================================================================
// Test: Backend enumeration
// ============================================================================

static void test_backend_enumeration() {
    std::cout << "\n=== Test: Backend Enumeration ===" << std::endl;
    
    AS_context context;
    AS_context_config config = AS_context_config_init();
    config.log = AS_log::default_log();
    context.init(nullptr, 0, &config);
    
    uint32_t device_count = 0;
    AS_result result = context.enumerate_devices(
        AS_device_type::PLAYBACK,
        [](AS_context*, AS_device_type, const AS_device_info* info, void* user_data) {
            uint32_t* count = static_cast<uint32_t*>(user_data);
            (*count)++;
            std::cout << "  Found device: " << info->name << std::endl;
            return AS_TRUE;
        },
        &device_count
    );
    
    TEST_ASSERT(AS_result_is_success(result), "Device enumeration should succeed");
    TEST_ASSERT(device_count > 0, "Should find at least one device");
    
    context.uninit();
}

// ============================================================================
// Run all tests
// ============================================================================

static void run_all_tests() {
    std::cout << "========================================" << std::endl;
    std::cout << "ArxSound Core Library Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_result_codes();
    test_format_utilities();
    test_log_system();
    test_context_init();
    test_device_init();
    test_device_start_stop();
    test_volume_control();
    test_frame_conversion();
    test_resampler();
    test_quick_start_api();
    test_backend_enumeration();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Results:" << std::endl;
    std::cout << "  Passed: " << g_tests_passed << std::endl;
    std::cout << "  Failed: " << g_tests_failed << std::endl;
    std::cout << "========================================" << std::endl;
}

} // namespace arxsound::test

int main() {
    arxsound::test::run_all_tests();
    return (arxsound::test::g_tests_failed > 0) ? 1 : 0;
}