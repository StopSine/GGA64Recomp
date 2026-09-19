#include <chrono>
#include <deque>
#include <mutex>
#include <vector>

#include "recomp_ui.h"

#include "elements/ui_element.h"
#include "elements/ui_label.h"

// A passive feed of short messages in the corner of the screen, for telling the
// player something happened without interrupting them. The context does not
// capture input or the mouse, so it draws over gameplay and menus alike and is
// skipped by the focus and mouse handling in ui_state.cpp.

static constexpr size_t max_visible_messages = 6;
static constexpr auto message_lifetime = std::chrono::seconds(6);

namespace {
    struct GameMessage {
        std::string text;
        std::chrono::steady_clock::time_point expires_at;
    };

    struct {
        recompui::ContextId ui_context = recompui::ContextId::null();
        std::vector<recompui::Label*> slots;
        std::deque<GameMessage> messages;
        std::mutex mutex;
        bool dirty = false;
    } message_state;
}

void recompui::init_game_message_context() {
    ContextId context = create_context();

    std::lock_guard lock{ message_state.mutex };

    context.open();

    message_state.ui_context = context;

    Element* root = context.create_element<Element>(context.get_root_element());
    root->set_display(Display::Flex);
    root->set_position(Position::Absolute);
    root->set_top(0);
    root->set_right(0);
    root->set_bottom(0);
    root->set_left(0);
    root->set_flex_direction(FlexDirection::Column);
    root->set_align_items(AlignItems::FlexEnd);
    root->set_background_color({ 0, 0, 0, 0 });

    Element* column = context.create_element<Element>(root);
    column->set_display(Display::Flex);
    column->set_flex_direction(FlexDirection::Column);
    column->set_align_items(AlignItems::FlexEnd);
    column->set_margin_top(24, Unit::Dp);
    column->set_margin_right(24, Unit::Dp);
    column->set_background_color({ 0, 0, 0, 0 });

    message_state.slots.reserve(max_visible_messages);

    for (size_t i = 0; i < max_visible_messages; i++) {
        Label* label = context.create_element<Label>(column, "", LabelStyle::Small);
        label->set_display(Display::None);
        // LabelStyle::Small sets font metrics but no colour, so it would
        // otherwise inherit whatever the stylesheet default is.
        label->set_color(Color{ 255, 255, 255, 255 });
        label->set_margin_top(4, Unit::Dp);
        label->set_padding_top(6, Unit::Dp);
        label->set_padding_bottom(6, Unit::Dp);
        label->set_padding_left(12, Unit::Dp);
        label->set_padding_right(12, Unit::Dp);
        label->set_border_radius(8, Unit::Dp);
        label->set_background_color(Color{ 8, 7, 13, 204 });
        message_state.slots.push_back(label);
    }

    context.close();

    context.set_captures_input(false);
    context.set_captures_mouse(false);
}

void recompui::show_game_message(const std::string& text) {
    std::lock_guard lock{ message_state.mutex };

    message_state.messages.push_back(GameMessage{
        text,
        std::chrono::steady_clock::now() + message_lifetime,
    });

    // Oldest first, so the newest stays on screen when the feed is full.
    while (message_state.messages.size() > max_visible_messages) {
        message_state.messages.pop_front();
    }

    message_state.dirty = true;
}

void recompui::update_game_messages() {
    std::lock_guard lock{ message_state.mutex };

    if (message_state.ui_context == ContextId::null()) {
        return;
    }

    auto now = std::chrono::steady_clock::now();

    while (!message_state.messages.empty() && message_state.messages.front().expires_at <= now) {
        message_state.messages.pop_front();
        message_state.dirty = true;
    }

    if (!message_state.dirty) {
        return;
    }

    message_state.dirty = false;

    // Only shown while it has something to say. Leaving it shown permanently
    // would make is_any_context_shown() always true, and ui_state.cpp reads
    // that as "a menu is open" to decide whether to fall back to the launcher,
    // which then never appears.
    bool shown = is_context_shown(message_state.ui_context);

    if (message_state.messages.empty()) {
        if (shown) {
            hide_context(message_state.ui_context);
        }
        return;
    }

    if (!shown) {
        show_context(message_state.ui_context, "");
    }

    ContextId prev_context = try_close_current_context();
    message_state.ui_context.open();

    for (size_t i = 0; i < message_state.slots.size(); i++) {
        Label* label = message_state.slots[i];

        if (i < message_state.messages.size()) {
            label->set_text(message_state.messages[i].text);
            // Block, not Flex: a Label is a div holding text, and text in a
            // flex container does not render.
            label->set_display(Display::Block);
        }
        else {
            label->set_text("");
            label->set_display(Display::None);
        }
    }

    message_state.ui_context.close();

    if (prev_context != ContextId::null()) {
        prev_context.open();
    }
}
