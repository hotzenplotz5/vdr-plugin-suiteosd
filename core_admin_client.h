#ifndef SUITEOSD_CORE_ADMIN_CLIENT_H
#define SUITEOSD_CORE_ADMIN_CLIENT_H
#include <cstdint>
#include <atomic>
#include <string>
#include <vector>
#include <tuple>

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
    std::string person, role, character, matchReason, artworkUrl, personKey;
};
struct CoreSearchEvent {
    std::string eventId, backendId, channelId, channelName, title, subtitle;
    std::string description, startTime, endTime, person, role, matchReason, artworkUrl, personKey;
    std::string nativeMatchProof;
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
    bool personProjection = false;
};

// Exact correlation only; no title/date/folder heuristics or second index.
std::string matchNativeRecording(const CoreSearchRecording &recording, const std::string &proof,
                                 const std::vector<std::string> &nativeNames);

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
// Compare the actual displayed Home projection, not the arrival of a new
// HTTP response. Unchanged readbacks need no rail replacement or redraw.
inline bool SameHomePage(const CoreHomePage &a,const CoreHomePage &b){
    if(a.total!=b.total||a.nextOffset!=b.nextOffset||a.hasMore!=b.hasMore||a.cards.size()!=b.cards.size())return false;
    for(size_t i=0;i<a.cards.size();++i){
        const auto &x=a.cards[i],&y=b.cards[i];
        auto projection=[](const CoreHomeCard &c){
            return std::tie(c.kind,c.key,c.title,c.subtitle,c.posterUrl,c.count,c.position,c.duration,c.completed,
                c.recording.id,c.recording.backendId,c.recording.nativeId,c.recording.title,c.recording.subtitle,
                c.recording.startTime,c.recording.artworkUrl);
        };
        if(projection(x)!=projection(y))return false;
    }
    return true;
}
struct DevicePairing {
    std::string origin, requestId, userCode, token, state;
    int pollIntervalSeconds = 5;
    std::int64_t expiresAtEpoch = 0;
};
struct DeviceCredential {
    std::string origin, deviceId, credentialId, secret;
};
// Local, private credential vault keyed by the exact configured server URL.
// Pairing bootstrap tokens and Human credentials are never persisted here.
bool loadDeviceProfile(const std::string &path, const std::string &origin,
                       DeviceCredential &credential);
bool saveDeviceProfile(const std::string &path, const DeviceCredential &credential);
bool removeDeviceProfile(const std::string &path, const std::string &origin);

class CoreAdminClient {
    std::string origin_;
    std::string localSocket_;
    std::string sessionCookie_;
    std::string csrf_;
    DeviceCredential device_;
    CoreStatus last_;
    void *requestHandle_ = nullptr;
    void resetTransport();
    struct Reply {
        long http = 0;
        std::string body;
        std::string etag;
        std::string cookie;
        std::string cacheControl;
        std::string error;
        bool ok() const { return http >= 200 && http < 300 && error.empty(); }
    };
    Reply request(const std::string &verb, const std::string &path,
                  const std::string &body = "", const std::string &etag = "",
                  const std::string &login = "", const std::string &password = "",
                  const std::string &pairingToken = "", bool anonymous = false, bool image = false);
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
    // One bootstrap GET, followed by at most one credential issue and durable storage.
    // A failed local write retains the issued credential for a storage-only retry.
    CoreStatus completePairing(DevicePairing &pairing, DeviceCredential &issued,
                               const std::string &profilePath);
    bool setDeviceCredential(const DeviceCredential &credential);
    CoreStatus testDevice();
    // One authorized SSE subscription. Only publish invalidation hints, never data.
    void recordingEvents(const std::string &backend, const std::atomic<bool> &stop,
                         std::atomic<bool> &changed, const std::string &domain = "recordings");
    void forgetDevice() { device_ = {}; }
    std::string artworkCredentialScope() const;
    bool hasDeviceCredential() const { return !device_.credentialId.empty(); }

    CoreStatus recordingPersons(const std::string &backendId, const std::string &name,
                                int limit, int offset, CorePersonSearch &results);
    CoreStatus globalSearch(const std::string &backendId, const std::string &query,
                            int limit, int offset, CoreGlobalSearch &results);
    CoreStatus publicSearch(const std::string &backendId, const std::string &query,
                            bool epg, int limit, int offset, CoreGlobalSearch &results);
    CoreStatus publicPeopleSearch(const std::string &backendId, const std::string &query,
                                  int limit, int offset, CoreGlobalSearch &results);
    CoreStatus publicRecordingDetail(const CoreSearchRecording &row, CoreSearchEvent &detail,
                                     bool nativeMatch = false);
    CoreStatus publicRecordingArtwork(const CoreSearchRecording &row, std::string &bytes,
        std::string *assetId=nullptr,const std::string &knownAsset={},int *privateMaxAge=nullptr);
    CoreStatus publicHomePage(const std::string &backend, HomeKind kind, const std::string &key,
                        int limit, int offset, int year, CoreHomePage &page);
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
