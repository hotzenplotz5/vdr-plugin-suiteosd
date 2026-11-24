#ifndef SUITEOSD_CORE_ADMIN_NAVIGATION_H
#define SUITEOSD_CORE_ADMIN_NAVIGATION_H
#include <algorithm>
#include <string>

namespace suiteosd {

enum class CorePage {
    None, Login, Backends, BackendSettings, Accounts, AccountDetails,
    Devices, DeviceDetails, Rename
};
enum class CoreConfirmation { None, ToggleAddon, ToggleAccount, RevokeDevice };

struct CoreAdminNavigation {
    static constexpr int kKeyboardColumns = 10;
    CorePage page = CorePage::None;
    CorePage afterLogin = CorePage::Backends;
    CoreConfirmation confirmation = CoreConfirmation::None;
    int selected = 0;
    int action = 0;
    int keyIndex = 0;
    bool passwordField = false;
    bool uppercase = false;
    std::string username;
    std::string password;
    std::string editingName;

    static const std::string &keys() {
        static const std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789 @._-!?#%+";
        return chars;
    }
    void openLogin(CorePage next) {
        page = CorePage::Login;
        afterLogin = next;
        selection = action = keyIndex = 0;
        passwordField = false;
        uppercase = false;
        clearPassword();
        username.clear();
        confirmation = CoreConfirmation::None;
    }
    void clearPassword() {
        std::fill(password.begin(), password.end(), '\0');
        password.clear();
    }
    void moveKey(int dx, int dy) {
        const int next = keyIndex + dx + dy * kKeyboardColumns;
        if (next >= 0 && next < int(keys().size())) keyIndex = next;
    }
    void typeKey() {
        const char c = keys()[keyIndex];
        const char v = uppercase && c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c;
        std::string &field = page == CorePage::Rename ? editingName :
                             passwordField ? password : username;
        const size_t max = page == CorePage::Rename || !passwordField ? 128U : 1024U;
        if (field.size() < max) field.push_back(v);
    }
    void digit(char c) {
        if (c < '0' || c > '9') return;
        std::string &field = page == CorePage::Rename ? editingName :
                             passwordField ? password : username;
        const size_t max = page == CorePage::Rename || !passwordField ? 128U : 1024U;
        if (field.size() < max) field.push_back(c);
    }
    void backspace() {
        std::string &field = page == CorePage::Rename ? editingName :
                             passwordField ? password : username;
        if (!field.empty()) field.pop_back();
    }
    bool back() {
        if (confirmation != CoreConfirmation::None) {
            confirmation = CoreConfirmation::None;
            return true;
        }
        if (page == CorePage::AccountDetails) {
            page = CorePage::Accounts; return true;
        }
        if (page == CorePage::DeviceDetails) {
            page = CorePage::Devices; return true;
        }
        if (page == CorePage::BackendSettings) {
            page = CorePage::Backends; return true;
        }
        if (page == CorePage::Rename) {
            page = CorePage::AccountDetails;
            editingName.clear();
            return true;
        }
        if (page != CorePage::None) {
            page = CorePage::None;
            clearPassword();
            return true;
        }
        return false;
    }
};

} // namespace suiteosd
#endif
