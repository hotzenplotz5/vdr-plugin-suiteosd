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

// Read-only projections of the existing Web search contracts. Preserve the
// server's ordering, counts, identities and match reasons; no second index.
struct CoreSearchRecording {
    std::string id, backendId, nativeId, title, subtitle, startTime;
    std::string person, role, character, matchReason, artworkUrl;
};
struct CoreSearchEvent {
    std::string eventId, backendId, channelId, channelName, title, subtitle;
    std::string description, startTime, endTime, person, role, matchReason, artworkUrl;
};
struct CoreSearchPerson {
    std::string name, role, imageUrl;
    std::int64_t recordingCount = 0, epgCount = 0;
};
struct CorePersonSearch {
    std::vector<CoreSearchRecording> recordings;
    std::int64_t total = 0;
    int limit = 0, offset = 0;
};
struct CoreGlobalSearch {
    std::string query, backendId, status;
    std::vector<CoreSearchRecording> recordings;
    std::vector<CoreSearchEvent> epg;
    std::vector<CoreSearchPerson> people;
    std::int64_t recordingTotal = 0, epgTotal = 0;
    int limit = 0, offset = 0;
    bool recordingHasMore = false, epgHasMore = false;
};

enum class HomeKind { Continue, Recent, New, Genres, Genre, Folders, Root, Movies };
struct CoreHomeCard {
    enum class Kind { Recording, Genre, Folder, Root };
    Kind kind = Kind::Recording;
    CoreSearchRecording recording;
    std::string key, title, subtitle, posterUrl;
    std::int64_t count = 0, position = -1, duration = -1;
    bool completed = false;
};
struct CoreHomePage {
    std::vector<CoreHomeCard> cards;
    std::int64_t total = 0;
    int nextOffset = 0;
    bool hasMore = false;
};
struct DevicePairing {
    std::string origin, requestId, userCode, token, state;
};
struct DeviceCredential {
    std::string origin, deviceId, credentialId, secret;
};
class CoreAdminClient {
    std::string origin_;
    std::string localSocket_;
    std::string sessionCookie_;
    std::string csrf_;
    DeviceCredential device_;
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
                  const std::string &login = "", const std::string &password = "",
                  const std::string &pairingToken = "", bool anonymous = false);
    CoreStatus status(const Reply &reply);
    static std::string idPath(const std::string &id);
public:
    explicit CoreAdminClient(std::string origin = "http://127.0.0.1:18080");
    // Native TV data transport: no Browser cookie, no password, no TCP fallback.
    CoreAdminClient(std::string origin, std::string localSocket);
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
    CoreStatus beginPairing(const std::string &name, const std::string &version,
                            DevicePairing &pairing);
    CoreStatus readPairing(DevicePairing &pairing);
    CoreStatus issuePairing(DevicePairing &pairing, DeviceCredential &credential);
    bool setDeviceCredential(const DeviceCredential &credential);
    CoreStatus testDevice();
    void forgetDevice() { device_ = {}; }

    CoreStatus recordingPersons(const std::string &backendId, const std::string &name,
                                int limit, int offset, CorePersonSearch &results);
    CoreStatus globalSearch(const std::string &backendId, const std::string &query,
                            int limit, int offset, CoreGlobalSearch &results);
    CoreStatus homePage(const std::string &backend, HomeKind kind, const std::string &key,
                        int limit, int offset, int year, CoreHomePage &page);
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
