#ifndef SUITEOSD_CORE_ADMIN_CLIENT_H
#define SUITEOSD_CORE_ADMIN_CLIENT_H
#include <cstdint>
#include <string>
#include <vector>

namespace suiteosd {

// All Suite-owned mutations use the existing Human Browser Session + CSRF,
// server permission grants and owner revision contracts. No VDR-root privilege,
// Device credential or second settings store authorizes these operations.
struct CoreStatus {
    bool ok = false;
    long http = 0;
    std::string message;
};
struct CoreItem {
    std::string id;
    std::string name;
    bool active = false;
    bool revoked = false;
};
struct CoreAddon {
    std::string backendId;
    bool enabled = false;
    std::int64_t revision = -1;
};
struct CoreEncoding {
    std::string backendId;
    std::string revision;
    int importCrf = 23;
    int shrinkCrf = 25;
    std::string importPreset;
    std::string shrinkPreset;
};

class CoreAdminClient {
    std::string origin_;
    std::string sessionCookie_;
    std::string csrf_;
    CoreStatus last_;
    struct Reply {
        long http = 0;
        std::string body;
        std::string etag;
        std::string cookie;
        std::string error;
        bool ok() const { return http >= 200 && http < 300 && error.empty(); }
    };
    Reply request(const std::string &verb, const std::string &path,
                  const std::string &body = "", const std::string &etag = "",
                  const std::string &login = "", const std::string &password = "");
    CoreStatus status(const Reply &reply);
    static std::string idPath(const std::string &id);
public:
    explicit CoreAdminClient(std::string origin = "http://127.0.0.1:18080");
    ~CoreAdminClient();
    CoreAdminClient(const CoreAdminClient &) = delete;
    CoreAdminClient &operator=(const CoreAdminClient &) = delete;

    bool configured() const { return !origin_.empty(); }
    bool authenticated() const { return !sessionCookie_.empty() && !csrf_.empty(); }
    const CoreStatus &last() const { return last_; }
    void setOrigin(const std::string &origin);
    const std::string &origin() const { return origin_; }
    CoreStatus login(const std::string &username, const std::string &password);
    void logout();

    CoreStatus backends(std::vector<CoreItem> &items);
    CoreStatus addon(const std::string &backendId, CoreAddon &value);
    CoreStatus setAddon(const CoreAddon &previous, bool enabled, CoreAddon &updated);
    CoreStatus encoding(const std::string &backendId, CoreEncoding &value);
    CoreStatus setEncoding(const CoreEncoding &previous, const CoreEncoding &desired,
                           CoreEncoding &updated);
    CoreStatus accounts(std::vector<CoreItem> &items);
    CoreStatus changeAccount(const CoreItem &selected, const std::string &displayName,
                              bool changeActive, bool active);
    CoreStatus devices(std::vector<CoreItem> &items);
    CoreStatus revokeDevice(const CoreItem &selected);
};

} // namespace suiteosd
#endif
