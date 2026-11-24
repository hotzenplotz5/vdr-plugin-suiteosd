#include "../core_admin_navigation.h"
#include <cassert>

int main() {
    using namespace suiteosd;
    CoreAdminNavigation n;
    assert(n.page == CorePage::None);
    n.openLogin(CorePage::Backends);
    assert(n.page == CorePage::Login);
    n.typeKey(); // a
    assert(n.username == "a");
    n.moveKey(1,0);
    n.uppercase = true;
    n.typeKey(); // B
    assert(n.username == "aB");
    n.passwordField = true;
    n.moveKey(1,0);
    n.typeKey(); // C, must not appear in username
    n.digit('9');
    assert(n.password == "C9");
    n.backspace();
    assert(n.password == "C");
    n.clearPassword();
    assert(n.password.empty() && n.username == "aB");
    n.passwordField = false;
    n.digit('7');
    assert(n.username == "aB7");
    n.moveKey(-1,0);
    n.moveKey(-1,0);
    assert(n.keyIndex == 0);

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
    n.page = CorePage::Login;
    n.password = "secret";
    assert(n.back() && n.page == CorePage::None);
    assert(n.password.empty());
}
