#include "core_admin_client.h"

#include <curl/curl.h>
#include <json-c/json.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <strings.h>
#include <memory>
#include <mutex>
#include <map>
#include <utility>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>

namespace suiteosd {
namespace {

using Document = std::unique_ptr<json_object, decltype(&json_object_put)>;
void initializeCurl() { static std::once_flag once; std::call_once(once, []{curl_global_init(CURL_GLOBAL_DEFAULT);}); }

Document parse(const std::string &data) {
    if (data.empty()) return Document(nullptr, &json_object_put);
    json_tokener *tok = json_tokener_new();
    if (!tok) return Document(nullptr, &json_object_put);
    json_object *result = json_tokener_parse_ex(tok, data.data(), int(data.size()));
    const bool valid = json_tokener_get_error(tok) == json_tokener_success &&
                       json_object_is_type(result, json_type_object);
    json_tokener_free(tok);
    if (!valid) {
        if (result) json_object_put(result);
        result = nullptr;
    }
    return Document(result, &json_object_put);
}
json_object *field(json_object *obj, const char *name) {
    json_object *value = nullptr;
    return obj && json_object_object_get_ex(obj, name, &value) ? value : nullptr;
}
std::string stringOf(json_object *obj, const char *name) {
    json_object *value = field(obj, name);
    if (!value || !json_object_is_type(value, json_type_string)) return {};
    const char *text = json_object_get_string(value);
    return text ? text : "";
}
bool boolOf(json_object *obj, const char *name) {
    json_object *value = field(obj, name);
    return value && json_object_is_type(value, json_type_boolean) &&
           json_object_get_boolean(value);
}
std::int64_t integerOf(json_object *obj, const char *name, std::int64_t fallback = -1) {
    json_object *value = field(obj, name);
    return value && json_object_is_type(value, json_type_int)
           ? json_object_get_int64(value) : fallback;
}
void readPairingTiming(json_object *obj,DevicePairing &pairing) {
    const auto seconds=integerOf(obj,"pollIntervalSeconds",pairing.pollIntervalSeconds);
    if(seconds>0&&seconds<=std::numeric_limits<int>::max())pairing.pollIntervalSeconds=int(seconds);
    const auto expires=stringOf(obj,"expiresAt");
    if(!expires.empty()){
        std::tm utc{};
        const char *end=strptime(expires.c_str(),"%Y-%m-%dT%H:%M:%S",&utc);
        if(end&&(*end=='Z'||(*end=='.'&&expires.back()=='Z')))
            pairing.expiresAtEpoch=timegm(&utc);
    }
}
std::string jsonText(json_object *obj) {
    return obj ? json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN) : "{}";
}
bool safeId(const std::string &id) {
    return !id.empty() && id.size() <= 160 &&
           std::all_of(id.begin(), id.end(), [](unsigned char c) {
               return std::isalnum(c) != 0 || c == '.' || c == '_' ||
                      c == ':' || c == '-';
           });
}
bool safeOrigin(const std::string &origin) {
    if (origin.empty() || origin.size() > 250 || origin.back() == '/' ||
        origin.find('@') != std::string::npos ||
        origin.find('#') != std::string::npos ||
        origin.find('?') != std::string::npos ||
        origin.find('\\') != std::string::npos ||
        origin.find('\r') != std::string::npos ||
        origin.find('\n') != std::string::npos)
        return false;
    const bool tls = origin.compare(0, 8, "https://") == 0 &&
                     origin.size() > 8;
    const bool http = origin.compare(0, 7, "http://") == 0 &&
                      origin.size() > 7;
    // Use the operator-configured Core origin; do not add an independent
    // activation gate. The UI warns about unencrypted remote HTTP.
    return tls || http;
}
std::string headerTrim(std::string s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}
struct Transfer {
    std::string body;
    std::string etag;
    std::string cookie;
    bool tooLarge = false;
};
size_t receiveBody(char *bytes, size_t n, size_t size, void *ctx) {
    auto &x = *static_cast<Transfer *>(ctx);
    const size_t count = n * size;
    if (count > 1024U * 1024U || x.body.size() > 1024U * 1024U - count) {
        x.tooLarge = true;
        return 0;
    }
    x.body.append(bytes, count);
    return count;
}
size_t receiveHeader(char *bytes, size_t n, size_t size, void *ctx) {
    auto &x = *static_cast<Transfer *>(ctx);
    const size_t count = n * size;
    if (count > 8192) return count;
    const std::string line(bytes, count);
    const auto col = line.find(':');
    if (col == std::string::npos) return count;
    const std::string name = line.substr(0, col);
    if (name.size() == 4 && strncasecmp(name.c_str(), "ETag", 4) == 0) {
        const std::string value = headerTrim(line.substr(col + 1));
        if (value.size() <= 256 && value.find_first_of("\r\n") == std::string::npos)
            x.etag = value;
    } else if (name.size() == 10 &&
               strncasecmp(name.c_str(), "Set-Cookie", 10) == 0) {
        const std::string value = headerTrim(line.substr(col + 1));
        const auto semicolon = value.find(';');
        const std::string pair = value.substr(0, semicolon);
        if (pair.compare(0, 18, "vdr_suite_session=") == 0 &&
            pair.size() > 18 && pair.size() <= 1024 &&
            pair.find_first_of("\r\n\t ;") == std::string::npos)
            x.cookie = pair;
    }
    return count;
}

CoreStatus toStatus(long http, const std::string &error,
                    const std::string &body) {
    CoreStatus result;
    result.http = http;
    result.ok = http >= 200 && http < 300 && error.empty();
    if (result.ok) return result;
    if (!error.empty()) { result.message = error; return result; }
    const auto obj = parse(body);
    std::string code = stringOf(obj.get(), "code");
    if (code.empty()) {
        json_object *problem = field(obj.get(), "error");
        code = json_object_is_type(problem, json_type_object)
               ? stringOf(problem, "code") :
               json_object_is_type(problem, json_type_string)
               ? json_object_get_string(problem) : "";
    }
    result.message = code.empty()
        ? "Core-Antwort HTTP " + std::to_string(http) : code;
    return result;
}
bool parseItems(const std::string &body, std::vector<CoreItem> &items,
                const char *idName) {
    auto root = parse(body);
    json_object *array = field(root.get(), "items");
    if (!json_object_is_type(array, json_type_array)) return false;
    std::vector<CoreItem> next;
    const size_t n = json_object_array_length(array);
    if (n > 100U) return false;
    for (size_t i = 0; i < n; ++i) {
        json_object *entry = json_object_array_get_idx(array, i);
        CoreItem item;
        item.id = stringOf(entry, idName);
        item.name = stringOf(entry, "displayName");
        if (item.name.empty()) item.name = stringOf(entry, "name");
        item.active = boolOf(entry, "active");
        item.revoked = boolOf(entry, "revoked");
        if (!safeId(item.id)) return false;
        if (item.name.empty()) item.name = item.id;
        next.push_back(std::move(item));
    }
    items.swap(next); // failed reads preserve the previous successful listing
    return true;
}
std::string queryEncode(const std::string &value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            result.push_back(char(c));
        else {
            result.push_back('%');
            result.push_back(hex[c >> 4]);
            result.push_back(hex[c & 15]);
        }
    }
    return result;
}
std::string searchImage(json_object *entry, const char *key) {
    auto *image = field(entry, key);
    return boolOf(image, "available") ? stringOf(image, "url") : "";
}
CoreSearchRecording readSearchRecording(json_object *entry) {
    CoreSearchRecording row;
    row.id = stringOf(entry, "id");
    if (row.id.empty()) row.id = stringOf(entry,"recordingId");
    row.backendId = stringOf(entry, "backendId");
    row.nativeId = stringOf(entry, "backendNativeId");
    row.title = stringOf(entry, "title");
    row.subtitle = stringOf(entry, "subtitle");
    row.startTime = stringOf(entry, "startTime");
    row.person = stringOf(entry, "matchedPerson");
    row.role = stringOf(entry, "matchedRole");
    row.matchReason = stringOf(entry, "matchReason");
    row.personKey = stringOf(entry, "personKey");
    row.artworkUrl = searchImage(entry, "artwork");
    auto *metadata = field(entry, "metadata");
    auto *presentation = field(metadata, "presentation");
    const auto title = stringOf(presentation, "title");
    const auto subtitle = stringOf(presentation, "subtitle");
    if (!title.empty()) row.title = title;
    if (!subtitle.empty()) row.subtitle = subtitle;
    if (row.artworkUrl.empty())
        row.artworkUrl = stringOf(field(metadata, "artwork"), "preferredUrl");
    return row;
}
bool parsePersonSearch(const std::string &body, CorePersonSearch &output) {
    auto root = parse(body);
    auto *matches = field(root.get(), "matches");
    if (!json_object_is_type(matches, json_type_array)) return false;
    CorePersonSearch next;
    next.total = integerOf(root.get(), "totalCount");
    next.limit = int(integerOf(root.get(), "limit"));
    next.offset = int(integerOf(root.get(), "offset"));
    if (next.total < 0 || next.limit < 0 || next.offset < 0) return false;
    for (size_t i = 0; i < json_object_array_length(matches); ++i) {
        auto *entry = json_object_array_get_idx(matches, i);
        auto *recording = field(entry, "recording");
        if (!json_object_is_type(recording, json_type_object)) continue;
        auto row = readSearchRecording(recording);
        auto *person = field(entry, "person");
        row.person = stringOf(person, "originalName");
        row.role = stringOf(person, "role");
        row.character = stringOf(person, "characterName");
        next.recordings.push_back(std::move(row));
    }
    output = std::move(next);
    return true;
}
bool parseGlobalSearch(const std::string &body, CoreGlobalSearch &output) {
    auto root = parse(body);
    auto *recordings = field(root.get(), "recordings");
    auto *epg = field(root.get(), "epg");
    auto *people = field(root.get(), "people");
    if (!json_object_is_type(recordings, json_type_array) ||
        !json_object_is_type(epg, json_type_array) ||
        !json_object_is_type(people, json_type_array)) return false;
    CoreGlobalSearch next;
    next.query = stringOf(root.get(), "query");
    next.backendId = stringOf(root.get(), "backendId");
    next.status = stringOf(root.get(), "status");
    next.limit = int(integerOf(root.get(), "limit"));
    next.offset = int(integerOf(root.get(), "offset"));
    next.recordingTotal = integerOf(root.get(), "recordingTotal");
    next.epgTotal = integerOf(root.get(), "epgTotal");
    next.recordingHasMore = boolOf(root.get(), "recordingHasMore");
    next.epgHasMore = boolOf(root.get(), "epgHasMore");
    if (next.limit < 0 || next.offset < 0 || next.recordingTotal < 0 || next.epgTotal < 0)
        return false;
    for (size_t i = 0; i < json_object_array_length(recordings); ++i)
        next.recordings.push_back(readSearchRecording(json_object_array_get_idx(recordings, i)));
    for (size_t i = 0; i < json_object_array_length(epg); ++i) {
        auto *entry = json_object_array_get_idx(epg, i);
        CoreSearchEvent row;
        row.eventId = stringOf(entry, "eventId");
        row.backendId = stringOf(entry, "backendId");
        row.channelId = stringOf(entry, "channelId");
        row.channelName = stringOf(entry, "channelName");
        row.title = stringOf(entry, "title");
        row.subtitle = stringOf(entry, "subtitle");
        row.description = stringOf(entry, "description");
        row.startTime = stringOf(entry, "startTime");
        row.endTime = stringOf(entry, "endTime");
        row.person = stringOf(entry, "matchedPerson");
        row.role = stringOf(entry, "matchedRole");
        row.matchReason = stringOf(entry, "matchReason");
    row.personKey = stringOf(entry, "personKey");
        row.artworkUrl = searchImage(entry, "artwork");
        next.epg.push_back(std::move(row));
    }
    for (size_t i = 0; i < json_object_array_length(people); ++i) {
        auto *entry = json_object_array_get_idx(people, i);
        CoreSearchPerson row;
        row.name = stringOf(entry, "name");
        row.role = stringOf(entry, "role");
        row.imageUrl = searchImage(entry, "image");
        row.recordingCount = integerOf(entry, "recordingCount", 0);
        row.epgCount = integerOf(entry, "epgCount", 0);
        next.people.push_back(std::move(row));
    }
    output = std::move(next);
    return true;
}

json_object *homeArray(json_object *root,const char *name) {
    auto *array=field(root,name);
    if(!json_object_is_type(array,json_type_array)) array=field(root,"items");
    return json_object_is_type(array,json_type_array)?array:nullptr;
}
CoreHomeCard homeRecording(json_object *rec) {
    CoreHomeCard card;
    card.recording=readSearchRecording(rec);
    card.key=card.recording.id;
    card.title=card.recording.title;
    card.subtitle=card.recording.subtitle;
    card.posterUrl=stringOf(field(field(rec,"metadata"),"presentation"),"posterUrl");
    return card;
}
bool parseHomePage(const std::string &body,const std::string &backend,HomeKind kind,
                   const std::string &key,int offset,CoreHomePage &output) {
    auto root=parse(body); if(!root)return false;
    CoreHomePage page;
    auto append=[&](CoreHomeCard card){
        if(card.recording.backendId.empty())card.recording.backendId=backend;
        if(card.recording.backendId!=backend || card.key.empty())return;
        if(std::none_of(page.cards.begin(),page.cards.end(),[&](const CoreHomeCard &c){
            return c.kind==card.kind&&c.key==card.key;}))page.cards.push_back(std::move(card));
    };
    if(kind==HomeKind::Continue||kind==HomeKind::Recent) {
        auto *items=homeArray(root.get(),"items");if(!items)return false;
        for(size_t i=0;i<json_object_array_length(items);++i){
            auto *item=json_object_array_get_idx(items,i);
            auto card=homeRecording(field(item,"recording"));
            if(card.recording.id.empty()) card.recording.id=stringOf(item,"recordingId");
            if(card.recording.nativeId.empty())card.recording.nativeId=stringOf(item,"backendNativeId");
            if(card.recording.backendId.empty())card.recording.backendId=stringOf(item,"backendId");
            const auto title=stringOf(item,"title"); if(!title.empty())card.title=title;
            const auto subtitle=stringOf(item,"subtitle");if(!subtitle.empty())card.subtitle=subtitle;
            const auto poster=stringOf(item,"posterUrl");if(!poster.empty())card.posterUrl=poster;
            card.key=card.recording.id;
            if(kind==HomeKind::Continue) {
                card.position=integerOf(item,"resumePositionSeconds",0);
                if(boolOf(item,"durationKnown"))card.duration=integerOf(item,"durationSeconds",0);
                if(card.position<=0||(card.duration>0&&card.position>=card.duration))continue;
            }else{
                if(boolOf(item,"positionKnown"))card.position=integerOf(item,"positionSeconds",0);
                card.completed=boolOf(item,"completionKnown")&&boolOf(item,"completed");
            }
            append(std::move(card));
        }
        page.total=page.cards.size();
    }else if(kind==HomeKind::Genres){
        auto *items=homeArray(root.get(),"genres");if(!items)return false;
        for(size_t i=0;i<json_object_array_length(items);++i){
            auto *item=json_object_array_get_idx(items,i);
            CoreHomeCard card;card.kind=CoreHomeCard::Kind::Genre;
            card.key=stringOf(item,"id");card.title=stringOf(item,"label");
            if(card.title.empty())card.title=stringOf(item,"labelDe");
            if(card.title.empty())card.title=card.key;
            card.count=integerOf(item,"count",0);
            if(!card.key.empty()&&card.count>0){card.subtitle=std::to_string(card.count)+" Aufnahmen";page.cards.push_back(std::move(card));}
        }
        page.total=page.cards.size();
    }else if(kind==HomeKind::Folders||kind==HomeKind::Root){
        auto *folders=field(root.get(),"folders"), *records=field(root.get(),"recordings");
        if(!json_object_is_type(folders,json_type_array)||!json_object_is_type(records,json_type_array))return false;
        const auto n=json_object_array_length(records);
        const auto total=integerOf(root.get(),"recordingCount",n);
        if(offset==0){
            std::vector<CoreHomeCard> leaves;
            for(size_t i=0;i<json_object_array_length(folders);++i){
                auto *folder=json_object_array_get_idx(folders,i);
                if(boolOf(folder,"singleRecordingLeaf")){
                    auto card=homeRecording(field(folder,"singleRecording"));
                    if(!card.key.empty()&&std::none_of(leaves.begin(),leaves.end(),[&](const CoreHomeCard &c){return c.key==card.key;}))leaves.push_back(std::move(card));
                    continue;
                }
                if(kind==HomeKind::Root)continue;
                CoreHomeCard card;card.kind=CoreHomeCard::Kind::Folder;
                card.key=stringOf(folder,"path");card.title=stringOf(folder,"name");
                if(card.title.empty())card.title=card.key;
                card.count=integerOf(folder,"recordingCount",0);
                card.subtitle=std::to_string(card.count)+" Aufnahmen";
                if(!card.key.empty())page.cards.push_back(std::move(card));
            }
            if(kind==HomeKind::Folders&&key.empty()){
                // The Web root rail groups direct recordings and embedded leaves.
                // Count duplicate leaves only once within the returned page.
                const auto additional=std::count_if(leaves.begin(),leaves.end(),[&](const CoreHomeCard &leaf){
                    for(size_t i=0;i<n;++i)
                        if(homeRecording(json_object_array_get_idx(records,i)).key==leaf.key)return false;
                    return true;
                });
                if(total+additional>0){
                    CoreHomeCard card;card.kind=CoreHomeCard::Kind::Root;card.key="root";
                    card.title="Hauptverzeichnis";card.count=total+additional;
                    card.subtitle=std::to_string(card.count)+" Aufnahmen";
                    page.cards.insert(page.cards.begin(),std::move(card));
                }
                page.total=page.cards.size();output=std::move(page);return true;
            }
            for(auto &card:leaves)append(std::move(card));
        }
        for(size_t i=0;i<n;++i)append(homeRecording(json_object_array_get_idx(records,i)));
        page.total=total;
        page.nextOffset=offset+int(n);page.hasMore=n>0&&page.nextOffset<total;
    }else{
        auto *items=homeArray(root.get(),"recordings");if(!items)return false;
        const auto n=json_object_array_length(items);
        for(size_t i=0;i<n;++i) {
            auto *item=json_object_array_get_idx(items,i);auto card=homeRecording(item);
            if(kind==HomeKind::Movies){
                auto *provider=field(field(item,"metadata"),"provider");
                auto date=stringOf(provider,"releaseDate");
                if(stringOf(provider,"contentKind")!="movie"||date.size()<4)continue;
                card.subtitle=date.substr(0,4);
            }
            append(std::move(card));
        }
        page.nextOffset=offset+int(n);
        page.total=integerOf(root.get(),"total",integerOf(root.get(),"totalCount",page.nextOffset));
        page.hasMore=n>0&&(boolOf(root.get(),"hasMore")||page.nextOffset<page.total);
    }
    output=std::move(page);return true;
}

bool parseAddon(const std::string &body, CoreAddon &state) {
    auto root = parse(body);
    if (!root || !field(root.get(), "desiredEnabled") ||
        !field(root.get(), "revision")) return false;
    CoreAddon result;
    result.backendId = stringOf(root.get(), "backendId");
    result.enabled = boolOf(root.get(), "desiredEnabled");
    result.revision = integerOf(root.get(), "revision");
    if (!safeId(result.backendId) || result.revision < 0) return false;
    state = std::move(result);
    return true;
}
bool parseEncoding(const std::string &body, CoreEncoding &state) {
    auto root = parse(body);
    if (!root) return false;
    CoreEncoding result;
    result.backendId = stringOf(root.get(), "backendId");
    result.revision = stringOf(root.get(), "revision");
    result.importCrf = int(integerOf(root.get(), "importCrf"));
    result.shrinkCrf = int(integerOf(root.get(), "shrinkCrf"));
    result.importPreset = stringOf(root.get(), "importPreset");
    result.shrinkPreset = stringOf(root.get(), "shrinkPreset");
    if (!safeId(result.backendId) || result.revision.size() != 16 ||
        result.revision.find_first_not_of("0123456789abcdef") != std::string::npos ||
        result.importCrf < 0 || result.importCrf > 51 ||
        result.shrinkCrf < 0 || result.shrinkCrf > 51 ||
        result.importPreset.empty() || result.shrinkPreset.empty()) return false;
    state = std::move(result);
    return true;
}
std::string encodeBody(const std::vector<std::pair<std::string, std::string>> &strings,
                       const std::vector<std::pair<std::string, bool>> &flags,
                       const std::vector<std::pair<std::string, std::int64_t>> &numbers = {}) {
    json_object *root = json_object_new_object();
    if (!root) return {};
    for (const auto &entry : strings)
        json_object_object_add(root, entry.first.c_str(),
                               json_object_new_string(entry.second.c_str()));
    for (const auto &entry : flags)
        json_object_object_add(root, entry.first.c_str(),
                               json_object_new_boolean(entry.second));
    for (const auto &entry : numbers)
        json_object_object_add(root, entry.first.c_str(),
                               json_object_new_int64(entry.second));
    std::string output = jsonText(root);
    json_object_put(root);
    return output;
}

} // namespace

CoreAdminClient::CoreAdminClient(std::string origin) { setOrigin(origin); }
CoreAdminClient::CoreAdminClient(std::string origin,std::string localSocket)
    : localSocket_(std::move(localSocket)) {
    setOrigin(origin);
}
CoreAdminClient::~CoreAdminClient() { logout(); }

void CoreAdminClient::setOrigin(const std::string &value) {
    if (!localSocket_.empty()) {
        origin_ = "http://localhost";
        return;
    }
    if (origin_ != value) { logout(); forgetDevice(); }
    origin_ = safeOrigin(value) ? value : "";
    if (origin_.empty()) {
        last_.ok = false;
        last_.message = "Core-URL ungültig (HTTP/HTTPS erwartet)";
    }
}

CoreAdminClient::Reply CoreAdminClient::request(
    const std::string &verb, const std::string &path, const std::string &body,
    const std::string &etag, const std::string &username, const std::string &password,
    const std::string &pairingToken, bool anonymous)
{
    Reply reply;
    if (!localSocket_.empty() && (!username.empty() || !sessionCookie_.empty())) {
        reply.error = "Lokaler Core-Transport verwendet keine Browser-Anmeldung";
        return reply;
    }
    if (origin_.empty() || path.empty() || path[0] != '/' ||
        path.find('\r') != std::string::npos ||
        path.find('\n') != std::string::npos) {
        reply.error = "Ungültige Core-URL";
        return reply;
    }
    initializeCurl();
    CURL *curl = curl_easy_init();
    if (!curl) { reply.error = "HTTP-Client nicht verfügbar"; return reply; }
    struct curl_slist *headers = nullptr;
    Transfer receive;
    const std::string url = origin_ + path;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Cache-Control: no-store");
    if (!localSocket_.empty()) {
        // The local Core transport uses kernel peer credentials, never a
        // client-supplied identity header or the browser's credentials.
        curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, localSocket_.c_str());
        curl_easy_setopt(curl, CURLOPT_PROXY, "");
    } else if (anonymous) {
        if (!pairingToken.empty()) {
            const std::string header = "X-VDR-Suite-Pairing-Token: " + pairingToken;
            headers = curl_slist_append(headers, header.c_str());
        }
    } else if (!device_.credentialId.empty() &&
               (path == "/api/v1" || path.compare(0, 8, "/api/v1/") == 0)) {
        const std::string header = "Authorization: VDR-Suite-Device " +
            device_.credentialId + "." + device_.secret;
        headers = curl_slist_append(headers, header.c_str());
    } else if (!username.empty()) {
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
    } else if (!sessionCookie_.empty()) {
        const std::string header = "Cookie: " + sessionCookie_;
        headers = curl_slist_append(headers, header.c_str());
    }
    if (verb == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, long(body.size()));
        if (!username.empty()) {
            // Login is a bodyless POST authenticated by Human Basic.
        } else {
            if (!anonymous || !body.empty())
                headers = curl_slist_append(headers, "Content-Type: application/json");
            else
                headers = curl_slist_append(headers, "Content-Type:");
            if (!anonymous && !csrf_.empty()) {
                const std::string csrf = "X-CSRF-Token: " + csrf_;
                headers = curl_slist_append(headers, csrf.c_str());
            }
            if (!etag.empty()) {
                const std::string match = "If-Match: " + etag;
                headers = curl_slist_append(headers, match.c_str());
            }
        }
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &receive);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, receiveHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &receive);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1200L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, long(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    const CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK)
        reply.error = receive.tooLarge ? "Core-Antwort zu groß" :
                        "Core-HTTP-Verbindung fehlgeschlagen";
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply.http);
    reply.body = std::move(receive.body);
    reply.etag = std::move(receive.etag);
    reply.cookie = std::move(receive.cookie);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return reply;
}

CoreStatus CoreAdminClient::status(const Reply &reply) {
    last_ = toStatus(reply.http, reply.error, reply.body);
    if (reply.http == 401) {
        sessionCookie_.clear();
        csrf_.clear();
    }
    return last_;
}
std::string CoreAdminClient::idPath(const std::string &id) {
    return safeId(id) ? id : "";
}

CoreStatus CoreAdminClient::login(const std::string &username,
                                  const std::string &password) {
    forgetDevice();
    sessionCookie_.clear();
    csrf_.clear();
    if (username.empty() || username.size() > 128 ||
        username.find(':') != std::string::npos || password.empty() ||
        password.size() > 1024) {
        last_ = {false, 0, "Benutzername oder Passwort ungültig"};
        return last_;
    }
    const Reply r = request("POST", "/api/security/browser-sessions", "", "",
                            username, password);
    const CoreStatus result = status(r);
    if (!result.ok) return result;
    const auto root = parse(r.body);
    const std::string token = stringOf(root.get(), "csrfToken");
    if (r.cookie.empty() || token.empty() || token.size() > 1024 ||
        token.find_first_of("\r\n") != std::string::npos) {
        last_ = {false, r.http, "Browser-Sitzung unvollständig"};
        return last_;
    }
    sessionCookie_ = r.cookie;
    csrf_ = token;
    return last_;
}

void CoreAdminClient::logout() {
    if (!sessionCookie_.empty() && !csrf_.empty())
        (void)request("POST", "/api/security/browser-sessions/logout", "{}");
    csrf_.clear();
    sessionCookie_.clear();
}

namespace {
bool wireSecret(const std::string &value) {
    return !value.empty() && value.size() <= 1024 &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return c > 32 && c < 127;
        });
}
}
namespace {
constexpr size_t kProfileFileLimit = 256U * 1024U;
Document readProfiles(const std::string &path) {
    if (path.empty()) return Document(nullptr,&json_object_put);
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        if (errno == ENOENT)
            return Document(json_object_new_object(),&json_object_put);
        return Document(nullptr,&json_object_put);
    }
    struct stat st{};
    if (fstat(fd,&st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_uid != geteuid() || (st.st_mode & 077) != 0 ||
        st.st_size < 0 || size_t(st.st_size) > kProfileFileLimit) {
        close(fd);
        return Document(nullptr,&json_object_put);
    }
    std::string bytes;
    char buffer[4096];
    bool ok = true;
    for (;;) {
        const auto n = read(fd,buffer,sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 || (n > 0 && bytes.size()+size_t(n)>kProfileFileLimit)) {
            ok = false; break;
        }
        if (!n) break;
        bytes.append(buffer,size_t(n));
    }
    close(fd);
    return ok ? parse(bytes) : Document(nullptr,&json_object_put);
}
bool writeProfiles(const std::string &path,json_object *root) {
    const std::string bytes = jsonText(root);
    if (bytes.size() > kProfileFileLimit) return false;
    std::string temp = path + ".XXXXXX";
    const int fd = mkstemp(&temp[0]); // mode 0600; same directory for atomic rename
    if (fd < 0) return false;
    size_t offset = 0;
    while (offset < bytes.size()) {
        const auto n = write(fd,bytes.data()+offset,bytes.size()-offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        offset += size_t(n);
    }
    bool ok = offset == bytes.size() && fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (ok) ok = rename(temp.c_str(),path.c_str()) == 0;
    if (!ok) unlink(temp.c_str());
    return ok;
}
bool validDevice(const DeviceCredential &c) {
    return safeOrigin(c.origin) && safeId(c.deviceId) &&
           safeId(c.credentialId) && wireSecret(c.secret);
}
}
bool loadDeviceProfile(const std::string &path,const std::string &origin,
                       DeviceCredential &credential) {
    auto root = readProfiles(path);
    auto *item = field(root.get(),origin.c_str());
    DeviceCredential next{origin,stringOf(item,"deviceId"),
        stringOf(item,"credentialId"),stringOf(item,"secret")};
    if (!validDevice(next)) return false;
    credential = std::move(next);
    return true;
}
bool saveDeviceProfile(const std::string &path,const DeviceCredential &credential) {
    if (!validDevice(credential)) return false;
    auto root = readProfiles(path);
    if (!root) return false;
    auto *item = json_object_new_object();
    if (!item) return false;
    json_object_object_add(item,"deviceId",json_object_new_string(credential.deviceId.c_str()));
    json_object_object_add(item,"credentialId",json_object_new_string(credential.credentialId.c_str()));
    json_object_object_add(item,"secret",json_object_new_string(credential.secret.c_str()));
    json_object_object_add(root.get(),credential.origin.c_str(),item);
    return writeProfiles(path,root.get());
}
bool removeDeviceProfile(const std::string &path,const std::string &origin) {
    auto root = readProfiles(path);
    if (!root) return false;
    if (!field(root.get(),origin.c_str())) return true;
    json_object_object_del(root.get(),origin.c_str());
    return writeProfiles(path,root.get());
}

CoreStatus CoreAdminClient::beginPairing(const std::string &name,
        const std::string &version, DevicePairing &pairing) {
    const auto r = request("POST", "/api/v1/device-pairings",
        encodeBody({{"displayName", name}, {"clientKind", "suiteosd"},
                    {"appVersion", version}}, {}), "", "", "", "", true);
    if (!status(r).ok) return last_;
    const auto root = parse(r.body);
    DevicePairing next{origin_, stringOf(root.get(),"pairingRequestId"),
        stringOf(root.get(),"userCode"), stringOf(root.get(),"pairingToken"),
        stringOf(root.get(),"status")};
    if (r.http != 201 || !safeId(next.requestId) || next.userCode.empty() ||
        !wireSecret(next.token) || next.state != "pending")
        return (last_ = {false,r.http,"Kopplungsantwort unvollständig"});
    readPairingTiming(root.get(),next);
    pairing = std::move(next);
    return last_;
}
CoreStatus CoreAdminClient::readPairing(DevicePairing &pairing) {
    if (pairing.origin != origin_ || !safeId(pairing.requestId) ||
        !wireSecret(pairing.token))
        return (last_ = {false,0,"Keine gültige Kopplungsanfrage für diesen Server"});
    const auto r = request("GET", "/api/v1/device-pairings/" + pairing.requestId,
        "", "", "", "", pairing.token, true);
    if (!status(r).ok) return last_;
    const auto root = parse(r.body);
    const auto state = stringOf(root.get(),"status");
    if (state != "pending" && state != "approved" && state != "rejected")
        return (last_ = {false,r.http,"Ungültiger Kopplungsstatus"});
    pairing.state = state;
    readPairingTiming(root.get(),pairing);
    return last_;
}
CoreStatus CoreAdminClient::issuePairing(DevicePairing &pairing,
        DeviceCredential &credential) {
    if (pairing.origin != origin_ || pairing.state != "approved" ||
        !safeId(pairing.requestId) || !wireSecret(pairing.token))
        return (last_ = {false,0,"Kopplung noch nicht bestätigt"});
    // Issuance is a one-shot mutation; never automatically retry even after
    // transport failure. A lost response requires a new pairing request.
    const auto approvedId = pairing.requestId;
    const auto token = std::move(pairing.token);
    pairing = {};
    const auto r = request("POST", "/api/v1/device-pairings/" +
        approvedId + "/credential", "", "", "", "", token, true);
    if (!status(r).ok) return last_;
    const auto root = parse(r.body);
    DeviceCredential next{origin_, stringOf(root.get(),"deviceId"),
        stringOf(root.get(),"credentialId"), stringOf(root.get(),"credentialSecret")};
    if (r.http != 201 || !safeId(next.deviceId) || !safeId(next.credentialId) ||
        !wireSecret(next.secret))
        return (last_ = {false,r.http,"Geräteberechtigung unvollständig"});
    credential = std::move(next);
    return last_;
}
CoreStatus CoreAdminClient::completePairing(DevicePairing &pairing,DeviceCredential &issued,
                                           const std::string &profilePath) {
    // A consumed bootstrap request is not an unpaired device. Reuse the
    // saved exact-origin credential for all subsequent status checks.
    if(issued.secret.empty() && pairing.requestId.empty() && pairing.token.empty()){
        DeviceCredential saved;
        if(!loadDeviceProfile(profilePath,origin_,saved)||!setDeviceCredential(saved))
            return {false,0,"Keine gespeicherte Kopplung für diesen Server"};
        const auto verified=testDevice();
        return verified.ok?CoreStatus{true,200,"Erfolgreich gekoppelt"}:verified;
    }
    if(issued.secret.empty()){
        const auto result=readPairing(pairing);
        if(!result.ok){
            // Terminal bootstrap outcomes must not start another issuance or continue polling.
            if(result.http==401||result.http==404||result.http==409||result.http==410)pairing={};
            return result;
        }
        if(pairing.state=="pending")return {true,200,"Warte auf Freigabe im Webfrontend"};
        if(pairing.state=="rejected"){
            pairing={};
            return {false,200,"Kopplung abgelehnt"};
        }
        const auto resultIssue=issuePairing(pairing,issued);
        if(!resultIssue.ok)return resultIssue; // no repeat of one-shot POST
    }
    if(issued.origin!=origin_)return {false,0,"Berechtigung gehört zu einem anderen Server"};
    if(!saveDeviceProfile(profilePath,issued))
        return {false,0,"Speichern fehlgeschlagen – Kopplung abschließen erneut wählen"};
    setDeviceCredential(issued);
    issued={};
    const auto verified=testDevice();
    return verified.ok?CoreStatus{true,201,"Erfolgreich gekoppelt"}:
        CoreStatus{true,201,"Kopplung gespeichert – Verbindungstest: "+verified.message};
}
bool CoreAdminClient::setDeviceCredential(const DeviceCredential &credential) {
    if (!localSocket_.empty() || credential.origin != origin_ ||
        !safeId(credential.deviceId) || !safeId(credential.credentialId) ||
        !wireSecret(credential.secret)) return false;
    logout();
    device_ = credential;
    return true;
}
CoreStatus CoreAdminClient::testDevice() {
    if (device_.credentialId.empty())
        return (last_ = {false,0,"Gerät nicht gekoppelt"});
    const auto r = request("GET","/api/v1");
    if (!status(r).ok) return last_;
    const auto root = parse(r.body);
    if (!boolOf(field(root.get(),"authentication"),"authenticated"))
        return (last_ = {false,r.http,"Geräteanmeldung nicht bestätigt"});
    return last_;
}

void CoreAdminClient::recordingEvents(const std::string &backend,const std::atomic<bool> &stop,
                                      std::atomic<bool> &changed) {
    if(stop.load()||device_.credentialId.empty()||!localSocket_.empty())return;
    initializeCurl();
    CURL *curl=curl_easy_init();if(!curl)return;
    struct Stream {
        const std::string &backend;
        const std::atomic<bool> &stop;
        std::atomic<bool> &changed;
        std::string buffer,event,data,epoch;
        bool admitted=false;
        std::int64_t sequence=0;
        void frame(){
            if(event=="resync"&&admitted)changed.store(true);
            else if(event=="hello"||event=="update"){
                const auto root=parse(data);
                if(stringOf(root.get(),"backendId")==backend){
                    const bool hello=event=="hello";
                    const auto incomingEpoch=stringOf(root.get(),"daemonEpoch");
                    const auto incomingSequence=integerOf(root.get(),"subscriptionSequence");
                    if(hello){admitted=false;epoch=incomingEpoch;sequence=0;}
                    else if(incomingEpoch!=epoch||incomingSequence<=sequence){
                        event.clear();data.clear();return;
                    }
                    auto *domains=field(root.get(),hello?"authorizedDomains":"changedDomains");
                    if(json_object_is_type(domains,json_type_array))
                        for(size_t i=0;i<json_object_array_length(domains);++i){
                            auto *d=json_object_array_get_idx(domains,i);
                            if(json_object_is_type(d,json_type_string)&&
                               std::string(json_object_get_string(d))=="recordings"){
                                if(hello)admitted=!epoch.empty()&&incomingSequence==0;
                                if(admitted){sequence=incomingSequence;changed.store(true);}
                            }
                        }
                }
            }
            event.clear();data.clear();
        }
        size_t feed(const char *bytes,size_t count){
            if(stop.load()||count>65536||buffer.size()+count>65536)return 0;
            buffer.append(bytes,count);
            size_t pos;
            while((pos=buffer.find('\n'))!=std::string::npos){
                auto line=buffer.substr(0,pos);buffer.erase(0,pos+1);
                if(!line.empty()&&line.back()=='\r')line.pop_back();
                if(line.empty())frame();
                else if(line.compare(0,6,"event:")==0)event=headerTrim(line.substr(6));
                else if(line.compare(0,5,"data:")==0){
                    if(data.size()+line.size()>65536)return 0;
                    if(!data.empty())data+='\n';
                    data+=headerTrim(line.substr(5));
                }
            }
            return count;
        }
    } stream{backend,stop,changed,{},{},{},{},false,0};
    const auto url=origin_+"/api/v1/events?backendId="+queryEncode(backend)+"&domain=recordings";
    const auto auth="Authorization: VDR-Suite-Device "+device_.credentialId+"."+device_.secret;
    curl_slist *headers=nullptr;
    headers=curl_slist_append(headers,"Accept: text/event-stream");
    headers=curl_slist_append(headers,auth.c_str());
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,+[](char *p,size_t n,size_t s,void *u)->size_t{
        return static_cast<Stream*>(u)->feed(p,n*s);
    });
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&stream);
    curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,+[](void *u,curl_off_t,curl_off_t,curl_off_t,curl_off_t)->int{
        return static_cast<Stream*>(u)->stop.load()?1:0;
    });
    curl_easy_setopt(curl,CURLOPT_XFERINFODATA,&stream);
    curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,1200L);
    curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1L);
    curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,35L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,150000L);
    curl_easy_setopt(curl,CURLOPT_FAILONERROR,1L);
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
    curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS,long(CURLPROTO_HTTP|CURLPROTO_HTTPS));
    (void)curl_easy_perform(curl);
    curl_slist_free_all(headers);curl_easy_cleanup(curl);
}

CoreStatus CoreAdminClient::recordingPersons(const std::string &backendId,
        const std::string &name, int limit, int offset, CorePersonSearch &results) {
    // Ask the existing read owner. It decides whether this request requires
    // the existing Human session; no additional client-side permission gate.
    const Reply r = request("GET", "/api/vdr/recordings/persons/search?backend=" +
        queryEncode(backendId) + "&name=" + queryEncode(name) +
        "&limit=" + std::to_string(limit) + "&offset=" + std::to_string(offset));
    if (!status(r).ok) return last_;
    if (!parsePersonSearch(r.body, results))
        last_ = {false, r.http, "Ungültige Personensuchantwort"};
    return last_;
}
CoreStatus CoreAdminClient::globalSearch(const std::string &backendId,
        const std::string &query, int limit, int offset, CoreGlobalSearch &results) {
    const Reply r = request("GET", "/api/search?backend=" + queryEncode(backendId) +
        "&query=" + queryEncode(query) + "&limit=" + std::to_string(limit) +
        "&offset=" + std::to_string(offset));
    if (!status(r).ok) return last_;
    if (!parseGlobalSearch(r.body, results))
        last_ = {false, r.http, "Ungültige Suchantwort"};
    return last_;
}

CoreStatus CoreAdminClient::publicSearch(const std::string &backendId,const std::string &query,
        bool epg,int limit,int offset,CoreGlobalSearch &results) {
    const auto r=request("GET",std::string("/api/v1/search/")+(epg?"epg":"recordings")+
        "?backendId="+queryEncode(backendId)+"&query="+queryEncode(query)+
        "&limit="+std::to_string(limit)+"&offset="+std::to_string(offset));
    if(!status(r).ok)return last_;
    const auto root=parse(r.body);
    auto *items=field(root.get(),"items"),*page=field(root.get(),"page");
    const auto invalid=[&](){return (last_={false,r.http,"Ungültige Public-v1-Suchantwort"});};
    if(!json_object_is_type(items,json_type_array)||!json_object_is_type(page,json_type_object))
        return invalid();
    CoreGlobalSearch next;
    next.query=query;next.backendId=backendId;next.status="ready";
    next.personProjection=integerOf(root.get(),"personProjectionVersion",0)==1;
    const auto total=integerOf(page,"totalCount");
    next.limit=int(integerOf(page,"limit"));next.offset=int(integerOf(page,"offset"));
    if(total<0||next.limit!=limit||next.offset!=offset||
       json_object_array_length(items)>size_t(limit)||
       !json_object_is_type(field(page,"hasMore"),json_type_boolean))return invalid();
    for(size_t i=0;i<json_object_array_length(items);++i){
        auto *item=json_object_array_get_idx(items,i);
        if(stringOf(item,"backendId")!=backendId||stringOf(item,"kind")!=(epg?"epg":"recording"))
            return invalid();
        if(epg){
            CoreSearchEvent row;
            row.backendId=backendId;row.eventId=stringOf(item,"eventId");
            row.channelId=stringOf(item,"channelId");row.title=stringOf(item,"title");
            row.subtitle=stringOf(item,"subtitle");row.startTime=stringOf(item,"startTime");
            row.endTime=stringOf(item,"endTime");
            row.channelName=stringOf(item,"channelName");
            row.description=stringOf(item,"description");
            row.person=stringOf(item,"matchedPerson");
            row.role=stringOf(item,"matchedRole");
            row.matchReason=stringOf(item,"matchReason");
            row.personKey=stringOf(item,"personKey");
            if(row.eventId.empty()||row.channelId.empty()||row.title.empty()||
               row.startTime.empty()||row.endTime.empty())return invalid();
            next.epg.push_back(std::move(row));
        }else{
            auto row=readSearchRecording(item);
            if(row.id.empty()||!safeId(row.id)||row.title.empty())return invalid();
            row.nativeId.clear(); // Public IDs are not native VDR file names.
            next.recordings.push_back(std::move(row));
        }
    }
    if(epg){next.epgTotal=total;next.epgHasMore=boolOf(page,"hasMore");}
    else{next.recordingTotal=total;next.recordingHasMore=boolOf(page,"hasMore");}
    results=std::move(next);return last_;
}
CoreStatus CoreAdminClient::publicPeopleSearch(const std::string &backendId,const std::string &query,
        int limit,int offset,CoreGlobalSearch &results) {
    CoreGlobalSearch recordings,epg;
    const auto recStatus=publicSearch(backendId,query,false,limit,offset,recordings);
    const auto epgStatus=publicSearch(backendId,query,true,limit,offset,epg);
    const bool recOk=recStatus.ok&&recordings.personProjection;
    const bool epgOk=epgStatus.ok&&epg.personProjection;
    if(!recOk&&!epgOk){
        if(recStatus.ok||epgStatus.ok)
            return (last_={false,0,"Personenprojektion im Core nicht verfügbar"});
        return (last_=recStatus);
    }
    CoreGlobalSearch next;
    next.query=query;next.backendId=backendId;next.limit=limit;next.offset=offset;
    next.status=recOk&&epgOk?"ready":"partial";next.personProjection=true;
    std::map<std::string,CoreSearchPerson> people;
    auto add=[&](const auto &row,bool recording){
        if(row.personKey.empty()||row.person.empty())return;
        auto &person=people[row.personKey];
        person.name=row.person;person.role=row.role;
        if(recording)++person.recordingCount;else ++person.epgCount;
    };
    if(recOk){
        for(const auto &row:recordings.recordings)add(row,true);
        next.recordingTotal=recordings.recordingTotal;
        next.recordingHasMore=recordings.recordingHasMore;
        next.recordings=std::move(recordings.recordings);
    }
    if(epgOk){
        for(const auto &row:epg.epg)add(row,false);
        next.epgTotal=epg.epgTotal;next.epgHasMore=epg.epgHasMore;
        next.epg=std::move(epg.epg);
    }
    for(auto &entry:people)next.people.push_back(std::move(entry.second));
    results=std::move(next);
    if(recOk&&epgOk)return (last_={true,200,""});
    const auto &missing=recOk?epgStatus:recStatus;
    return (last_={true,200,std::string(recOk?"EPG":"Aufnahmen")+" nicht vollständig: "+
        (missing.ok?"Personenprojektion fehlt":"HTTP "+std::to_string(missing.http)+" · "+missing.message)});
}
CoreStatus CoreAdminClient::publicRecordingDetail(const CoreSearchRecording &row,CoreSearchEvent &detail) {
    if(!safeId(row.id)||row.id.compare(0,4,"rec_")!=0||row.backendId.empty())
        return (last_={false,0,"Ungültige Aufnahme-ID"});
    const auto r=request("GET","/api/v1/recordings/"+row.id+"?backendId="+queryEncode(row.backendId));
    if(!status(r).ok)return last_;
    const auto root=parse(r.body);
    if(stringOf(root.get(),"recordingId")!=row.id||stringOf(root.get(),"backendId")!=row.backendId)
        return (last_={false,r.http,"Ungültige Aufnahmedetails"});
    CoreSearchEvent next;
    next.title=row.title;next.backendId=row.backendId;
    auto *meta=field(root.get(),"metadata");
    next.subtitle=stringOf(meta,"episodeTitle");
    if(next.subtitle.empty())next.subtitle=stringOf(meta,"subtitle");
    next.description=stringOf(meta,"overview");
    if(next.description.empty())next.description=stringOf(root.get(),"description");
    next.startTime=stringOf(root.get(),"recordedAt");
    detail=std::move(next);return last_;
}
CoreStatus CoreAdminClient::homePage(const std::string &backend,HomeKind kind,
    const std::string &key,int limit,int offset,int year,CoreHomePage &page) {
    std::string route,method="GET",body;
    const auto b=queryEncode(backend);
    const auto pagination="&limit="+std::to_string(limit)+"&offset="+std::to_string(offset);
    switch(kind){
    case HomeKind::Continue: case HomeKind::Recent:
        method="POST";route=kind==HomeKind::Continue?"/api/media/continue-watching":"/api/media/recently-watched";
        body=encodeBody({{"operation","list"},{"backendId",backend}},{});
        break;
    case HomeKind::Genres:route="/api/metadata/genres?backend="+b+"&scope=recordings&locale=de";break;
    case HomeKind::Genre:route="/api/metadata/genres/recordings?backend="+b+"&genre="+queryEncode(key)+pagination;break;
    case HomeKind::Folders: case HomeKind::Root:
        route="/api/vdr/recordings/folder?backend="+b+"&path="+queryEncode(key)+pagination;break;
    case HomeKind::New:route="/api/vdr/recordings/query?backend="+b+"&sort=startTime&order=desc"+pagination;break;
    case HomeKind::Movies:route="/api/vdr/recordings/query?backend="+b+"&sort=movieReleaseDate&order=desc&movieReleaseYearFrom="+
        std::to_string(year-4)+"&movieReleaseYearTo="+std::to_string(year)+pagination;break;
    }
    const auto r=request(method,route,body);
    if(!status(r).ok)return last_;
    if(!parseHomePage(r.body,backend,kind,key,offset,page))last_={false,r.http,"Ungültige Home-Antwort"};
    return last_;
}

CoreStatus CoreAdminClient::backends(std::vector<CoreItem> &items) {
    const Reply r = request("GET", "/api/v1/backends?limit=100&sort=backendId&order=asc");
    if (!status(r).ok) return last_;
    if (!parseItems(r.body, items, "backendId")) {
        last_ = {false,r.http,"Ungültige Backend-Liste"}; return last_;
    }
    return last_;
}
CoreStatus CoreAdminClient::addon(const std::string &id, CoreAddon &state) {
    if (idPath(id).empty()) return {false,0,"Ungültiges Backend"};
    const Reply r = request("GET", "/api/v1/addons/rectools/activation?backendId=" + id);
    if (!status(r).ok) return last_;
    if (!parseAddon(r.body, state) || state.backendId != id) {
        last_ = {false,r.http,"Ungültiger Addon-Status"};
    }
    return last_;
}
CoreStatus CoreAdminClient::setAddon(const CoreAddon &prev, bool enabled,
                                     CoreAddon &updated) {
    if (!safeId(prev.backendId) || prev.revision < 0)
        return {false,0,"Addon-Revision nicht verfügbar"};
    const std::string body = encodeBody({{"backendId",prev.backendId}},
        {{"desiredEnabled",enabled}},{{"expectedRevision",prev.revision}});
    const Reply r = request("POST", "/api/v1/addons/rectools/activation", body);
    if (!status(r).ok) return last_;
    if (!parseAddon(r.body, updated) || updated.backendId != prev.backendId ||
        updated.enabled != enabled) {
        last_ = {false,r.http,"Addon-Readback nicht bestätigt"};
    }
    return last_;
}
CoreStatus CoreAdminClient::encoding(const std::string &id, CoreEncoding &state) {
    if (!safeId(id)) return {false,0,"Ungültiges Backend"};
    const Reply r = request("GET", "/api/v1/addons/media-tools/encoding?backendId=" + id);
    if (!status(r).ok) return last_;
    if (!parseEncoding(r.body, state) || state.backendId != id)
        last_ = {false,r.http,"Ungültiger Encoding-Status"};
    return last_;
}
CoreStatus CoreAdminClient::setEncoding(const CoreEncoding &prev,
        const CoreEncoding &desired, CoreEncoding &updated) {
    if (!safeId(prev.backendId) || prev.backendId != desired.backendId ||
        prev.revision.size() != 16 || desired.importCrf < 0 || desired.importCrf > 51 ||
        desired.shrinkCrf < 0 || desired.shrinkCrf > 51)
        return {false,0,"Encoding-Eingaben ungültig"};
    const std::string body = encodeBody(
        {{"backendId",prev.backendId},{"revision",prev.revision},
         {"importPreset",desired.importPreset},{"shrinkPreset",desired.shrinkPreset}},
        {},{{"importCrf",desired.importCrf},{"shrinkCrf",desired.shrinkCrf}});
    const Reply r = request("POST", "/api/v1/addons/media-tools/encoding", body);
    if (!status(r).ok) return last_;
    if (!parseEncoding(r.body, updated) || updated.backendId != prev.backendId)
        last_ = {false,r.http,"Encoding-Readback nicht bestätigt"};
    return last_;
}
CoreStatus CoreAdminClient::accounts(std::vector<CoreItem> &items) {
    const Reply r = request("GET", "/api/v1/accounts?limit=100&sort=accountId&order=asc");
    if (!status(r).ok) return last_;
    if (!parseItems(r.body, items, "accountId"))
        last_ = {false,r.http,"Ungültige Kontenliste"};
    return last_;
}
CoreStatus CoreAdminClient::changeAccount(const CoreItem &item,
        const std::string &displayName, bool changeActive, bool active) {
    const std::string id = idPath(item.id);
    if (id.empty() || (!changeActive &&
        (displayName.empty() || displayName.size() > 128)))
        return {false,0,"Ungültiges Konto oder Anzeigename"};
    const std::string path = "/api/v1/accounts/" + id;
    const Reply fresh = request("GET", path);
    if (!status(fresh).ok) return last_;
    const auto root = parse(fresh.body);
    if (fresh.etag.empty() || stringOf(root.get(),"accountId") != id)
        return (last_ = {false,fresh.http,"Konto-Revision nicht verfügbar"});
    // One deliberate mutation, no retry on uncertainty, conflicts or loss.
    const std::string body = changeActive ?
        encodeBody({},{{"active",active}}) :
        encodeBody({{"displayName",displayName}},{});
    const Reply changed = request("POST", path, body, fresh.etag);
    if (!status(changed).ok) return last_;
    const auto after = parse(changed.body);
    if (stringOf(after.get(),"accountId") != id ||
        (changeActive && boolOf(after.get(),"active") != active) ||
        (!changeActive && stringOf(after.get(),"displayName") != displayName))
        return (last_ = {false,changed.http,"Kontenänderung nicht bestätigt"});
    return last_;
}
CoreStatus CoreAdminClient::devices(std::vector<CoreItem> &items) {
    const Reply r = request("GET", "/api/v1/devices?limit=100");
    if (!status(r).ok) return last_;
    if (!parseItems(r.body, items, "deviceId"))
        last_ = {false,r.http,"Ungültige Geräteliste"};
    return last_;
}
CoreStatus CoreAdminClient::revokeDevice(const CoreItem &item) {
    const std::string id = idPath(item.id);
    if (id.empty() || item.revoked)
        return {false,0,"Geräteauswahl ungültig oder bereits widerrufen"};
    const std::string path = "/api/v1/devices/" + id + "/lifecycle";
    const Reply fresh = request("GET", path);
    if (!status(fresh).ok) return last_;
    const auto root = parse(fresh.body);
    if (fresh.etag.empty() || stringOf(root.get(),"deviceId") != id)
        return (last_ = {false,fresh.http,"Geräte-Revision nicht verfügbar"});
    if (boolOf(root.get(),"revoked"))
        return (last_ = {false,fresh.http,"Gerät bereits widerrufen"});
    const Reply changed = request("POST", path, "{}", fresh.etag);
    if (!status(changed).ok) return last_;
    const auto after = parse(changed.body);
    if (stringOf(after.get(),"deviceId") != id ||
        !boolOf(after.get(),"revoked"))
        return (last_ = {false,changed.http,"Geräte-Widerruf nicht bestätigt"});
    return last_;
}

} // namespace suiteosd
