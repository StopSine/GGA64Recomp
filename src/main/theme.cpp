#include "elements/ui_theme.h"

#include "goemon_support.h"

void goemon64::set_custom_theme() {
    using namespace recompui;

    theme::set_theme_color(theme::color::Background1, Color{13, 10, 7, 255});
    theme::set_theme_color(theme::color::Background2, Color{24, 20, 16, 255});
    theme::set_theme_color(theme::color::Background3, Color{34, 28, 22, 255});
    theme::set_theme_color(theme::color::BGOverlay, Color{219, 190, 184, 26});
    theme::set_theme_color(theme::color::ModalOverlay, Color{13, 10, 7, 242});
    theme::set_theme_color(theme::color::BGShadow, Color{0, 0, 0, 89});
    theme::set_theme_color(theme::color::BGShadow2, Color{13, 10, 7, 184});

    theme::set_theme_color(theme::color::Text, Color{242, 242, 242, 255});
    theme::set_theme_color(theme::color::TextActive, Color{245, 245, 245, 255});
    theme::set_theme_color(theme::color::TextDim, Color{204, 204, 204, 255});
    theme::set_theme_color(theme::color::TextInactive, Color{255, 255, 255, 153});
    theme::set_theme_color(theme::color::TextA5, Color{242, 242, 242, 13});
    theme::set_theme_color(theme::color::TextA20, Color{242, 242, 242, 51});
    theme::set_theme_color(theme::color::TextA30, Color{242, 242, 242, 77});
    theme::set_theme_color(theme::color::TextA50, Color{242, 242, 242, 128});
    theme::set_theme_color(theme::color::TextA80, Color{242, 242, 242, 204});

    theme::set_theme_color(theme::color::Primary, Color{242, 76, 61, 255});
    theme::set_theme_color(theme::color::PrimaryL, Color{247, 181, 160, 255});
    theme::set_theme_color(theme::color::PrimaryD, Color{198, 58, 42, 255});
    theme::set_theme_color(theme::color::PrimaryA5, Color{242, 76, 61, 13});
    theme::set_theme_color(theme::color::PrimaryA20, Color{242, 76, 61, 51});
    theme::set_theme_color(theme::color::PrimaryA30, Color{242, 76, 61, 77});
    theme::set_theme_color(theme::color::PrimaryA50, Color{242, 76, 61, 128});
    theme::set_theme_color(theme::color::PrimaryA80, Color{242, 76, 61, 204});

    // The launcher's selected option uses SecondaryA80, so this carries GGA's
    // red rather than the blue the old sheet used for secondary accents.
    theme::set_theme_color(theme::color::Secondary, Color{242, 76, 61, 255});
    theme::set_theme_color(theme::color::SecondaryL, Color{247, 181, 160, 255});
    theme::set_theme_color(theme::color::SecondaryD, Color{198, 58, 42, 255});
    theme::set_theme_color(theme::color::SecondaryA5, Color{242, 76, 61, 13});
    theme::set_theme_color(theme::color::SecondaryA20, Color{242, 76, 61, 51});
    theme::set_theme_color(theme::color::SecondaryA30, Color{242, 76, 61, 77});
    theme::set_theme_color(theme::color::SecondaryA50, Color{242, 76, 61, 128});
    theme::set_theme_color(theme::color::SecondaryA80, Color{242, 76, 61, 204});

    theme::set_theme_color(theme::color::Warning, Color{233, 205, 53, 255});
    theme::set_theme_color(theme::color::WarningL, Color{249, 229, 124, 255});
    theme::set_theme_color(theme::color::WarningD, Color{197, 170, 22, 255});
    theme::set_theme_color(theme::color::WarningA5, Color{233, 205, 53, 13});
    theme::set_theme_color(theme::color::WarningA20, Color{233, 205, 53, 51});
    theme::set_theme_color(theme::color::WarningA30, Color{233, 205, 53, 77});
    theme::set_theme_color(theme::color::WarningA50, Color{233, 205, 53, 128});
    theme::set_theme_color(theme::color::WarningA80, Color{233, 205, 53, 204});

    theme::set_theme_color(theme::color::Danger, Color{248, 96, 57, 255});
    theme::set_theme_color(theme::color::DangerL, Color{254, 134, 103, 255});
    theme::set_theme_color(theme::color::DangerD, Color{178, 57, 25, 255});
    theme::set_theme_color(theme::color::DangerA5, Color{248, 96, 57, 13});
    theme::set_theme_color(theme::color::DangerA20, Color{248, 96, 57, 51});
    theme::set_theme_color(theme::color::DangerA30, Color{248, 96, 57, 77});
    theme::set_theme_color(theme::color::DangerA50, Color{248, 96, 57, 128});
    theme::set_theme_color(theme::color::DangerA80, Color{248, 96, 57, 204});

    theme::set_theme_color(theme::color::Success, Color{69, 208, 67, 255});
    theme::set_theme_color(theme::color::SuccessL, Color{170, 234, 169, 255});
    theme::set_theme_color(theme::color::SuccessD, Color{44, 167, 42, 255});
    theme::set_theme_color(theme::color::SuccessA5, Color{69, 208, 67, 13});
    theme::set_theme_color(theme::color::SuccessA20, Color{69, 208, 67, 51});
    theme::set_theme_color(theme::color::SuccessA30, Color{69, 208, 67, 77});
    theme::set_theme_color(theme::color::SuccessA50, Color{69, 208, 67, 128});
    theme::set_theme_color(theme::color::SuccessA80, Color{69, 208, 67, 204});
}
