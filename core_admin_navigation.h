#ifndef SUITEOSD_CORE_ADMIN_NAVIGATION_H
#define SUITEOSD_CORE_ADMIN_NAVIGATION_H
#include <algorithm>
#include <string>

namespace suiteosd {

enum class CorePage {
    None, Backends, BackendSettings, Accounts, AccountDetails,
    Devices, DeviceDetails, Rename
};
enum class CoreConfirmation { None, ToggleAddon, ToggleAccount, RevokeDevice };

struct CoreAdminNavigation {
    static constexpr int kKeyboardColumns = 10;
    CorePage page = CorePage::None;
    CoreConfirmation confirmation = CoreConfirmation::None;
    int selected = 0;
    int action = 0;
    int keyIndex = 0;
    bool uppercase = false;
    std::string editingName;

    static const std::string &keys() {
        static const std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789 @._-!?#%+";
        return chars;
    }
    void moveKey(int dx, int dy) {
        const int next = keyIndex + dx + dy * kKeyboardColumns;
        if (next >= 0 && next < int(keys().size())) keyIndex = next;
    }
    void typeKey() {
        const char c = keys()[keyIndex];
        const char v = uppercase && c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c;
        if (editingName.size() < 128U) editingName.push_back(v);
    }
    void digit(char c) {
        if (c < '0' || c > '9') return;
        if (editingName.size() < 128U) editingName.push_back(c);
    }
    void backspace() {
        if (!editingName.empty()) editingName.pop_back();
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
            return true;
        }
        return false;
    }
};

} // namespace suiteosd
#endif
