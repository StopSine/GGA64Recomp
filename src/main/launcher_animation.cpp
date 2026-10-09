// Launcher background, reproducing what the old recomp.rcss did with two
// keyframe animations on goemon-clipped.svg: fade 0 -> 0.1 over 2.5s
// (cubic-in-out) and drift 100dp -> 0dp over 25s (cubic-out).

#include <chrono>

#include "recompui/recompui.h"

#include "goemon_support.h"

namespace {

constexpr float fade_duration = 2.5f;
constexpr float fade_target_opacity = 0.1f;

constexpr float slide_duration = 25.0f;
// The old animation drifted 100dp, a small nudge rather than a sweep.
constexpr float slide_start_x = 100.0f;

// The old wrapper spanned top: -55vw to bottom: -50vw, so the image was
// 100vh + 105vw tall and hung further above the viewport than below it. At 16:9
// that works out to these percentages of viewport height; the asymmetry is what
// keeps Goemon's lower half on screen.
constexpr float background_height = 287.0f;
constexpr float background_top = -98.0f;

recompui::Element* background_svg = nullptr;
std::chrono::steady_clock::time_point start_time;
bool started = false;

float cubic_in_out(float t) {
    return t < 0.5f ? 4.0f * t * t * t
                    : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

float cubic_out(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

float progress(float elapsed, float duration) {
    if (elapsed >= duration) {
        return 1.0f;
    }
    return elapsed / duration;
}

}

void goemon64::launcher_animation_setup(recompui::LauncherMenu* menu) {
    background_svg = menu->set_launcher_background_svg("goemon-clipped.svg");

    // The old layout sized by height and let the width follow, which keeps
    // Goemon's proportions when the window is wider than tall.
    background_svg->set_top(background_top, recompui::Unit::Percent);
    background_svg->set_left(0);
    background_svg->set_height(background_height, recompui::Unit::Percent);
    background_svg->set_width_auto();
    background_svg->set_opacity(0.0f);
    background_svg->set_translate_2D(slide_start_x, 0.0f, recompui::Unit::Dp);

    started = false;
}

void goemon64::launcher_animation_update(recompui::LauncherMenu* menu) {
    (void)menu;

    if (background_svg == nullptr) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();

    if (!started) {
        start_time = now;
        started = true;
    }

    float elapsed = std::chrono::duration<float>(now - start_time).count();

    background_svg->set_opacity(cubic_in_out(progress(elapsed, fade_duration)) * fade_target_opacity);

    float slide = 1.0f - cubic_out(progress(elapsed, slide_duration));
    background_svg->set_translate_2D(slide_start_x * slide, 0.0f, recompui::Unit::Dp);
}
