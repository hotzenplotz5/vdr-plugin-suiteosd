#ifndef VDR_SUITEOSD_SETTINGS_NAVIGATION_H
#define VDR_SUITEOSD_SETTINGS_NAVIGATION_H

#include <algorithm>

namespace suiteosd {

enum class SettingsAction {
    None, OpenVdrSetup, OpenPluginSetup, ToggleLivePreview
};

// Presentation-only navigation. VDR owns VDR settings, the installed plugins
// own their own setup pages, Suite Core/Agent owns Suite-account/addon data.
// This model must never duplicate any of their persistent configuration.
struct SettingsNavigation {
    static constexpr int kCategories = 6;
    static constexpr int kColumns = 2;
    enum Category {
        Vdr = 0, Suite = 1, Plugins = 2,
        Television = 3, Accounts = 4, System = 5
    };

    int selected = Vdr;
    int action = 0;
    bool detail = false;

    int ActionCount() const {
        switch (selected) {
        case Vdr: return 1;
        case Plugins: return 1;
        case Television: return 2;
        default: return 0; // read-only sections remain navigable
        }
    }

    void Open() {
        selected = Vdr;
        action = 0;
        detail = false;
    }

    void Move(int dx, int dy) {
        if (detail) {
            const int n = ActionCount();
            if (n > 0) action = std::clamp(action + dy + dx, 0, n - 1);
            return;
        }
        const int next = selected + dx + dy * kColumns;
        if (next >= 0 && next < kCategories) selected = next;
    }

    // true = return to the overview, false = leave Settings for Suite Home.
    bool Back() {
        if (!detail) return false;
        detail = false;
        action = 0;
        return true;
    }

    SettingsAction Select() {
        if (!detail) {
            detail = true;
            action = 0;
            return SettingsAction::None;
        }
        switch (selected) {
        case Vdr: return SettingsAction::OpenVdrSetup;
        case Plugins: return SettingsAction::OpenPluginSetup;
        case Television:
            return action == 0 ? SettingsAction::ToggleLivePreview :
                                 SettingsAction::OpenVdrSetup;
        default: return SettingsAction::None;
        }
    }
};

} // namespace suiteosd
#endif
