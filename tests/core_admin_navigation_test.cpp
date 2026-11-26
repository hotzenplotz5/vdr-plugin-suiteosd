#include "../core_admin_navigation.h"
#include <cassert>

int main() {
    using namespace suiteosd;
    CoreAdminNavigation n;
    assert(n.page == CorePage::None);
    n.page = CorePage::Rename;
    n.typeKey(); // a
    n.moveKey(1,0);
    n.uppercase = true;
    n.typeKey(); // B
    n.digit('9');
    assert(n.editingName == "aB9");
    n.backspace();
    assert(n.editingName == "aB");
    assert(n.back() && n.page == CorePage::AccountDetails && n.editingName.empty());

    n.page = CorePage::Accounts;
    n.confirmation = CoreConfirmation::ToggleAccount;
    assert(n.back() && n.page == CorePage::Accounts &&
           n.confirmation == CoreConfirmation::None);
    n.page = CorePage::AccountDetails;
    assert(n.back() && n.page == CorePage::Accounts);
    n.page = CorePage::Devices;
    n.selected = 3;
    n.page = CorePage::DeviceDetails;
    assert(n.back() && n.page == CorePage::Devices && n.selected == 3);
    n.page = CorePage::BackendSettings;
    assert(n.back() && n.page == CorePage::Backends);
    n.page = CorePage::Rename;
    n.editingName = "Test";
    n.digit('3');
    assert(n.editingName == "Test3");
    assert(n.back() && n.page == CorePage::AccountDetails);
    assert(n.editingName.empty());
    n.page = CorePage::Backends;
    assert(n.back() && n.page == CorePage::None);
}
