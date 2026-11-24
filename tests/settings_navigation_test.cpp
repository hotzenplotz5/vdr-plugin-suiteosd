#include "../settings_navigation.h"
#include <cassert>

int main() {
    using namespace suiteosd;
    SettingsNavigation nav;
    assert(nav.selected == SettingsNavigation::Vdr && !nav.detail);
    nav.Move(1, 0);
    assert(nav.selected == SettingsNavigation::Suite);
    nav.Move(0, 1);
    assert(nav.selected == SettingsNavigation::Television);
    nav.Move(0, 1);
    assert(nav.selected == SettingsNavigation::System);
    nav.Move(0, 1);
    assert(nav.selected == SettingsNavigation::System);
    nav.Move(-1, 0);
    assert(nav.selected == SettingsNavigation::Accounts);
    nav.Move(0, -1);
    assert(nav.selected == SettingsNavigation::Plugins);

    assert(nav.Select() == SettingsAction::None);
    assert(nav.detail);
    assert(nav.ActionCount() == 1);
    assert(nav.Select() == SettingsAction::OpenPluginSetup);
    assert(nav.Back());
    assert(!nav.detail);

    nav.Move(1, 0);
    assert(nav.selected == SettingsNavigation::Television);
    assert(nav.Select() == SettingsAction::None);
    assert(nav.Select() == SettingsAction::ToggleLivePreview);
    nav.Move(0, 1);
    assert(nav.action == 1);
    assert(nav.Select() == SettingsAction::OpenVdrSetup);
    nav.Move(0, 1);
    assert(nav.action == 1);
    assert(nav.Back());

    nav.Move(0, -1);
    assert(nav.selected == SettingsNavigation::Suite);
    assert(nav.Select() == SettingsAction::None);
    assert(nav.ActionCount() == 0);
    assert(nav.Select() == SettingsAction::None);
    assert(nav.Back());
    assert(!nav.Back());
    nav.Open();
    assert(nav.selected == SettingsNavigation::Vdr);
    assert(nav.Select() == SettingsAction::None);
    assert(nav.Select() == SettingsAction::OpenVdrSetup);
    return 0;
}
