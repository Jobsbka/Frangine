#include "core/core_module.hpp"
#include "render/render_module.hpp"
#include "sound/sound_module.hpp"
#include "game/game_module.hpp"
#include "ui/ui_module.hpp"
#include "game/DoodleJumpGame.hpp"
#include <iostream>

int main() {
    try {
        arxglue::core::initializeModule();
        arxglue::render::initializeModule();
        arxglue::sound::initializeModule();
        arxglue::game::initializeModule();
        arxglue::ui::initializeModule();

        arxglue::game::DoodleJumpGame game;
        return game.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}