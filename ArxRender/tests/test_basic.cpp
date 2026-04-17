#include <iostream>
#include <vector>
#include <stdexcept>
#include <cassert>

#include <arxrender.hpp>
#include <arxrender_context.hpp>
#include <arxrender_device.hpp>
#include <arxrender_surface.hpp>
#include <arxrender_command.hpp>

#define ASSERT_OK(expr) do { \
    auto res = (expr); \
    if (res != arxrender::AR_result::SUCCESS) { \
        std::cerr << "[FAIL] " #expr " -> " << static_cast<int>(res) << "\n"; \
        return 1; \
    } \
} while(0)

int main() {
    std::cout << "=== ArxRender Standalone Test (Headless) ===\n";

    try {
        // 1. Инициализация контекста с принудительным Null-бэкендом
        arxrender::AR_context ctx;
        std::vector<arxrender::AR_backend> priority = {arxrender::AR_backend::NULL_BACKEND};
        ASSERT_OK(ctx.init(priority.data(), priority.size(), nullptr));
        std::cout << "[PASS] Context initialized. Backend: " 
                  << arxrender::AR_context::backend_name(ctx.active_backend()) << "\n";

        // 2. Создание логического устройства
        arxrender::AR_device device;
        arxrender::AR_device_config dev_cfg{};
        dev_cfg.debug_layer = false;
        ASSERT_OK(device.init(&ctx, &dev_cfg));
        std::cout << "[PASS] Device initialized.\n";

        // 3. Создание поверхности (в Null-бэкенде хранит только размеры)
        arxrender::AR_surface surface;
        arxrender::AR_surface_config surf_cfg{};
        surf_cfg.width = 800;
        surf_cfg.height = 600;
        surf_cfg.native_window_handle = nullptr; // Игнорируется Null/Vulkan/Metal в headless
        ASSERT_OK(surface.init(&ctx, &surf_cfg));
        std::cout << "[PASS] Surface initialized (" << surface.width() << "x" << surface.height() << ").\n";

        // 4. Запись команд в буфер
        arxrender::AR_command_buffer* cmd_buf = nullptr;
        ASSERT_OK(device.create_command_buffer(&cmd_buf));
        
        ASSERT_OK(cmd_buf->begin());
        cmd_buf->clear_color(0.15f, 0.2f, 0.25f, 1.0f);
        cmd_buf->set_viewport(0.0f, 0.0f, 800.0f, 600.0f);
        // Здесь можно добавить bind_pipeline, draw, bind_material и т.д.
        ASSERT_OK(cmd_buf->end());
        std::cout << "[PASS] Command buffer recorded successfully.\n";

        // 5. Исполнение и презентация
        ASSERT_OK(cmd_buf->execute());
        ASSERT_OK(surface.present(cmd_buf));
        std::cout << "[PASS] Command executed & surface presented.\n";

        // 6. Корректная очистка
        delete cmd_buf;
        surface.uninit();
        device.uninit();
        ctx.uninit();

        std::cout << "=== All Tests Passed ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception: " << e.what() << "\n";
        return 1;
    }
}