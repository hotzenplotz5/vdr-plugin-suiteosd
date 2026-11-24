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
#include <utility>

namespace suiteosd {
namespace {

using Document = std::unique_ptr<json_object, decltype(&json_object_put)>;

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
CoreAdminClient::~CoreAdminClient() { logout(); }

void CoreAdminClient::setOrigin(const std::string &value) {
    if (origin_ != value) logout();
    origin_ = safeOrigin(value) ? value : "";
    if (origin_.empty()) {
        last_.ok = false;
        last_.message = "Core-URL ungültig (HTTP/HTTPS erwartet)";
    }
}

CoreAdminClient::Reply CoreAdminClient::request(
    const std::string &verb, const std::string &path, const std::string &body,
    const std::string &etag, const std::string &username, const std::string &password)
{
    Reply reply;
    if (origin_.empty() || path.empty() || path[0] != '/' ||
        path.find('\r') != std::string::npos ||
        path.find('\n') != std::string::npos) {
        reply.error = "Ungültige Core-URL";
        return reply;
    }
    static std::once_flag curlOnce;
    std::call_once(curlOnce, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    CURL *curl = curl_easy_init();
    if (!curl) { reply.error = "HTTP-Client nicht verfügbar"; return reply; }
    struct curl_slist *headers = nullptr;
    Transfer receive;
    const std::string url = origin_ + path;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Cache-Control: no-store");
    if (!username.empty()) {
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
            headers = curl_slist_append(headers, "Content-Type: application/json");
            if (!csrf_.empty()) {
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
