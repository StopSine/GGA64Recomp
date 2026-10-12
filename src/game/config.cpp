#include <algorithm>
#include <filesystem>
#include <variant>

#include "recompinput/recompinput.h"
#include "recompui/config.h"
#include "util/file.h"

#include "goemon_config.h"
#include "goemon_sound.h"

// recompui persists general.json, graphics.json and sound.json; recompinput
// persists controls.json. Everything here only describes what goes in them.

static const std::string bgm_volume_option = "bgm_volume";
static const std::string se_volume_option = "se_volume";

static const std::string cheats_tab_id = "cheats";
static const std::string noclip_option = "noclip";
static const std::string speed_boost_option = "speed_boost";
static const std::string invincible_option = "invincible";

static recomp::config::Config* cheats_config = nullptr;

static void add_cheat_options(recomp::config::Config& config) {
    cheats_config = &config;
    config.add_bool_option(noclip_option, "Noclip",
        "Move freely with WASD, Q and E, passing through walls and ignoring gravity.", false);
    config.add_bool_option(speed_boost_option, "Fast Movement",
        "Raises the walking speed the game moves you at.", false);
    config.add_bool_option(invincible_option, "No Damage",
        "Stops enemies and hazards hurting you, including the knockback.", false);
}

static bool cheat_enabled(const std::string& option_id) {
    if (cheats_config == nullptr) {
        return false;
    }
    return std::get<bool>(cheats_config->get_option_value(option_id));
}

bool goemon64::get_noclip_enabled() {
    return cheat_enabled(noclip_option);
}

bool goemon64::get_speed_boost_enabled() {
    return cheat_enabled(speed_boost_option);
}

bool goemon64::get_invincible_enabled() {
    return cheat_enabled(invincible_option);
}

void goemon64::toggle_noclip() {
    if (cheats_config != nullptr) {
        cheats_config->set_option_value(noclip_option, !cheat_enabled(noclip_option));
    }
}

void goemon64::toggle_invincible() {
    if (cheats_config != nullptr) {
        cheats_config->set_option_value(invincible_option, !cheat_enabled(invincible_option));
    }
}

static void add_sound_options(recomp::config::Config& config) {
    config.add_percent_number_option(bgm_volume_option, "Music Volume", "Controls background music volume.", 100.0);
    config.add_percent_number_option(se_volume_option, "Sound Effect Volume", "Controls sound effect volume.", 100.0);
}

static int sound_percent(const std::string& option_id) {
    double value = std::get<double>(recompui::config::get_sound_config().get_option_value(option_id));
    return std::clamp(static_cast<int>(value), 0, 100);
}

int goemon64::get_bgm_volume() {
    return sound_percent(bgm_volume_option);
}

int goemon64::get_se_volume() {
    return sound_percent(se_volume_option);
}

static void set_control_descriptions() {
    using recompinput::GameInput;
    using recompinput::set_game_input_description;

    set_game_input_description(GameInput::A, "Used to jump, interact with people/things, swim slowly while underwater, select options in menus and attack with right arm while controlling Impact.");
    set_game_input_description(GameInput::B, "Used to attack, pick up and throw objects, dash while underwater, go back in menus and attack with the left arm while controlling Impact.");
    set_game_input_description(GameInput::Z, "Used to crawl on the ground and to shoot bullets while controlling Impact.");
    set_game_input_description(GameInput::L, "Unused. Mods may use it for additional features.");
    set_game_input_description(GameInput::R, "Used to activate camera control. Hold and press any of the C-Buttons to control the camera.");
    set_game_input_description(GameInput::START, "Used to pause and start the game.");
    set_game_input_description(GameInput::C_UP, "Used for activating the current character's magic ability. When camera control is activated it will zoom in closer. In the closest zoom stage it will allow you to look around in 360 degrees.");
    set_game_input_description(GameInput::C_DOWN, "Used for changing the current character. When camera control is activated it will zoom out further.");
    set_game_input_description(GameInput::C_LEFT, "Used to change the character's current weapon/item. When camera control is activated it will rotate the camera left.");
    set_game_input_description(GameInput::C_RIGHT, "Used to activate the minimap. When camera control is activated it will rotate the right left.");
    set_game_input_description(GameInput::DPAD_UP, "Unused. Mods may use it for additional features.");
    set_game_input_description(GameInput::DPAD_DOWN, "Unused. Mods may use it for additional features.");
    set_game_input_description(GameInput::DPAD_LEFT, "Unused. Mods may use it for additional features.");
    set_game_input_description(GameInput::DPAD_RIGHT, "Unused. Mods may use it for additional features.");

    const std::string move_description = "Used to move and for steering while swimming. Used to control the cursor in the cockpit while controlling Impact.";
    set_game_input_description(GameInput::Y_AXIS_POS, move_description);
    set_game_input_description(GameInput::Y_AXIS_NEG, move_description);
    set_game_input_description(GameInput::X_AXIS_NEG, move_description);
    set_game_input_description(GameInput::X_AXIS_POS, move_description);
}

void goemon64::init_config() {
    std::filesystem::path recomp_dir = recompui::file::get_app_folder_path();

    if (!recomp_dir.empty()) {
        std::filesystem::create_directories(recomp_dir);
    }

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = false;
    general_options.has_mouse_sensitivity = true;

    recompui::config::create_general_tab(general_options);
    recompui::config::create_graphics_tab();

    set_control_descriptions();
    recompui::config::create_controls_tab();

    add_sound_options(recompui::config::create_sound_tab());

    add_cheat_options(recompui::config::create_config_tab("Cheats", cheats_tab_id, false));

    recompui::config::create_mods_tab();

    recompui::config::finalize();
}
