#ifndef __GOEMON_SUPPORT_H__
#define __GOEMON_SUPPORT_H__

namespace recompui {
    class LauncherMenu;
}

namespace goemon64 {
    void set_custom_theme();

    void launcher_animation_setup(recompui::LauncherMenu* menu);
    void launcher_animation_update(recompui::LauncherMenu* menu);
}

#endif
