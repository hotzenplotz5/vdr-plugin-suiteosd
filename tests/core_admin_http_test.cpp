#include "../core_admin_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace {
struct Request {
    std::string method, path, body;
    std::map<std::string, std::string> headers;
};
std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return char(std::tolower(c));
    });
    return text;
}
Request readRequest(int fd) {
    std::string bytes;
    char buffer[4096];
    size_t bodyStart = std::string::npos;
    while ((bodyStart = bytes.find("\r\n\r\n")) == std::string::npos) {
        const auto n = recv(fd, buffer, sizeof(buffer), 0);
        assert(n > 0);
        bytes.append(buffer, size_t(n));
        assert(bytes.size() < 65536);
    }
    const auto lineEnd = bytes.find("\r\n");
    assert(lineEnd != std::string::npos);
    const std::string first = bytes.substr(0, lineEnd);
    const auto one = first.find(' ');
    const auto two = first.find(' ', one + 1);
    assert(one != std::string::npos && two != std::string::npos);
    Request r;
    r.method = first.substr(0, one);
    r.path = first.substr(one + 1, two - one - 1);
    size_t offset = lineEnd + 2;
    while (offset < bodyStart) {
        const auto end = bytes.find("\r\n", offset);
        assert(end != std::string::npos);
        const auto colon = bytes.find(':', offset);
        if (colon != std::string::npos && colon < end) {
            std::string key = lower(bytes.substr(offset, colon - offset));
            size_t value = colon + 1;
            while (value < end && bytes[value] == ' ') ++value;
            r.headers[key] = bytes.substr(value, end - value);
        }
        offset = end + 2;
    }
    const size_t size = r.headers.count("content-length")
        ? size_t(std::stoul(r.headers["content-length"])) : 0;
    r.body = bytes.substr(bodyStart + 4);
    while (r.body.size() < size) {
        const auto n = recv(fd, buffer, sizeof(buffer), 0);
        assert(n > 0);
        r.body.append(buffer, size_t(n));
    }
    r.body.resize(size);
    return r;
}
void answer(int fd, const std::string &body, const std::string &extra = "",
            int http = 200) {
    const std::string response =
        "HTTP/1.1 " + std::to_string(http) + (http == 204 ? " No Content" : " OK") +
        "\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n" +
        extra + "Content-Length: " + std::to_string(body.size()) +
        "\r\nConnection: close\r\n\r\n" + body;
    const auto sent = send(fd, response.c_str(), response.size(), MSG_NOSIGNAL);
    assert(sent == ssize_t(response.size()));
}
} // namespace

int main() {
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenFd >= 0);
    sockaddr_in bindTo{};
    bindTo.sin_family = AF_INET;
    bindTo.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bindTo.sin_port = 0;
    assert(bind(listenFd, reinterpret_cast<sockaddr *>(&bindTo), sizeof(bindTo)) == 0);
    assert(listen(listenFd, 2) == 0);
    socklen_t len = sizeof(bindTo);
    assert(getsockname(listenFd, reinterpret_cast<sockaddr *>(&bindTo), &len) == 0);
    const int port = ntohs(bindTo.sin_port);

    std::thread mock([&] {
        const std::vector<std::string> expected = {
            "POST /api/security/browser-sessions",
            "GET /api/v1/backends?limit=100&sort=backendId&order=asc",
            "GET /api/v1/addons/rectools/activation?backendId=default",
            "POST /api/v1/addons/rectools/activation",
            "GET /api/v1/addons/media-tools/encoding?backendId=default",
            "POST /api/v1/addons/media-tools/encoding",
            "GET /api/v1/accounts?limit=100&sort=accountId&order=asc",
            "GET /api/v1/accounts/acct-a",
            "POST /api/v1/accounts/acct-a",
            "GET /api/v1/devices?limit=100",
            "GET /api/v1/devices/dev-a/lifecycle",
            "POST /api/v1/devices/dev-a/lifecycle",
            "GET /api/vdr/recordings/persons/search?backend=default&name=Will%20Smith&limit=3&offset=0",
            "GET /api/search?backend=default&query=J%C3%BCrgen%20%26%20Co&limit=24&offset=24",
            "GET /api/search?backend=default&query=Will%20Smith&limit=3&offset=0",
            "POST /api/security/browser-sessions/logout"
        };
        const std::vector<std::string> bodies = {
            R"({"csrfToken":"csrf-abc","expiresAt":"2099-01-01T00:00:00Z"})",
            R"({"items":[{"backendId":"default","name":"Main","active":true}]})",
            R"({"backendId":"default","revision":5,"desiredEnabled":false})",
            R"({"backendId":"default","revision":6,"desiredEnabled":true})",
            R"({"backendId":"default","revision":"0123456789abcdef","importCrf":23,"shrinkCrf":25,"importPreset":"medium","shrinkPreset":"fast"})",
            R"({"backendId":"default","revision":"fedcba9876543210","importCrf":24,"shrinkCrf":25,"importPreset":"medium","shrinkPreset":"fast"})",
            R"({"items":[{"accountId":"acct-a","displayName":"Admin","active":true}]})",
            R"({"accountId":"acct-a","displayName":"Admin","active":true})",
            R"({"accountId":"acct-a","displayName":"Admin","active":false})",
            R"({"items":[{"deviceId":"dev-a","displayName":"Samsung TV","active":true,"revoked":false}]})",
            R"({"deviceId":"dev-a","active":true,"revoked":false})",
            R"({"deviceId":"dev-a","active":false,"revoked":true})",
            R"({"totalCount":10,"returnedCount":3,"limit":3,"offset":0,"matches":[{"recording":{"id":"33","backendId":"default","backendNativeId":"/video/SciFi/Men in Black 3/2026-01-01.20.15.1-0.rec","title":"SciFi/Men in Black 3","startTime":"1781935980","metadata":{"presentation":{"title":"Men in Black 3","subtitle":"Filmbeschreibung","posterUrl":"/recording-artwork/default/test"},"artwork":{"preferredUrl":"/recording-artwork/default/test"}}},"person":{"source":"tvscraper","originalName":"Will Smith","normalizedName":"will-smith","role":"actor","characterName":"Agent J","providerReference":""}},{"recording":{"id":"17","backendId":"default","backendNativeId":"/video/SciFi/Men in Black/2026-01-01.20.15.1-0.rec","title":"SciFi/Men in Black","startTime":"1781935980","metadata":{"presentation":{"title":"Men in Black","subtitle":"Filmbeschreibung","posterUrl":"/recording-artwork/default/test"},"artwork":{"preferredUrl":"/recording-artwork/default/test"}}},"person":{"source":"tvscraper","originalName":"Will Smith","normalizedName":"will-smith","role":"actor","characterName":"Jay","providerReference":""}},{"recording":{"id":"83","backendId":"default","backendNativeId":"/video/SciFi/Independence Day/2026-01-01.20.15.1-0.rec","title":"SciFi/Independence Day","startTime":"1781935980","metadata":{"presentation":{"title":"Independence Day","subtitle":"Filmbeschreibung","posterUrl":"/recording-artwork/default/test"},"artwork":{"preferredUrl":"/recording-artwork/default/test"}}},"person":{"source":"tvscraper","originalName":"Will Smith","normalizedName":"will-smith","role":"actor","characterName":"Capt. Steven Hiller","providerReference":""}}]})",
            R"({"query":"Jürgen & Co","backendId":"default","status":"ok","limit":24,"offset":24,"recordings":[{"id":"r2","backendId":"default","backendNativeId":"/video/a.rec","title":"Film","matchedPerson":"Jürgen","matchReason":"person"}],"recordingTotal":25,"recordingHasMore":false,"epg":[{"eventId":"7","backendId":"default","channelId":"C-1-2-3","title":"Sendung","matchedPerson":"Jürgen","artwork":{"available":true,"url":"/api/epg/cache/artwork?eventId=7"}}],"epgTotal":30,"epgHasMore":true,"people":[{"name":"Jürgen","role":"actor","recordingCount":25,"epgCount":30}]})",
            R"({"query":"Will Smith","backendId":"default","status":"ready","minimumQueryLength":2,"limit":3,"offset":0,"recordings":[{"id":"191","backendId":"default","backendNativeId":"/video/Drama/Das Streben nach Glück/2026-01-01.20.15.1-0.rec","title":"Das Streben nach Glück","matchedPerson":"Will Smith","matchedRole":"actor","matchReason":"person","artwork":{"available":true,"url":"/api/recordings/metadata/image?kind=preferred"}},{"id":"83","backendId":"default","backendNativeId":"/video/SciFi/Independence Day/2026-01-01.20.15.1-0.rec","title":"SciFi/Independence Day","matchedPerson":"Will Smith","matchedRole":"actor","matchReason":"person"},{"id":"239","backendId":"default","backendNativeId":"/video/Action/Gemini_Man/2026-01-01.20.15.1-0.rec","title":"Gemini Man","matchedPerson":"Will Smith","matchedRole":"actor","matchReason":"person"}],"recordingTotal":9,"recordingHasMore":true,"epg":[{"id":"62892","eventId":"62892","backendId":"default","channelId":"C-1-2-3","channelName":"Warner TV Film HD","title":"Der Staatsfeind Nr. 1","subtitle":"Mit Will Smith","startTime":"1795705200","endTime":"1795713300","matchedPerson":"","matchedRole":"","matchReason":"title","artwork":{"available":false,"url":""}}],"epgTotal":1,"epgHasMore":false,"people":[{"name":"Will Smith","role":"actor","recordingCount":3,"epgCount":0,"image":{"available":true,"url":"/api/recordings/metadata/image?kind=person&index=0"}}],"hasMore":true})",
            ""
        };
        for (size_t i = 0; i < expected.size(); ++i) {
            int fd = accept(listenFd, nullptr, nullptr);
            assert(fd >= 0);
            const Request request = readRequest(fd);
            assert(request.method + " " + request.path == expected[i]);
            if (i == 0) {
                assert(request.headers.count("authorization"));
                assert(request.headers.at("authorization").find("Basic ") == 0);
                assert(!request.headers.count("cookie"));
            } else {
                assert(request.headers.count("cookie"));
                assert(request.headers.at("cookie") == "vdr_suite_session=sid-abc");
                assert(!request.headers.count("authorization"));
                if (request.method == "POST") {
                    assert(request.headers.count("x-csrf-token"));
                    assert(request.headers.at("x-csrf-token") == "csrf-abc");
                }
            }
            if (i == 3) {
                assert(request.body.find("\"desiredEnabled\":true") != std::string::npos);
                assert(request.body.find("\"expectedRevision\":5") != std::string::npos);
            }
            if (i == 5) {
                assert(request.body.find("\"revision\":\"0123456789abcdef\"") != std::string::npos);
                assert(request.body.find("\"importCrf\":24") != std::string::npos);
            }
            if (i == 8 || i == 11)
                assert(request.headers.at("if-match") ==
                       (i == 8 ? "\"account-etag\"" : "\"device-etag\""));
            if (i == 8)
                assert(request.body.find("\"active\":false") != std::string::npos);
            if (i == 11) assert(request.body == "{}");
            std::string header;
            if (i == 0)
                header = "Set-Cookie: vdr_suite_session=sid-abc; Path=/; HttpOnly\r\n";
            if (i == 7) header = "ETag: \"account-etag\"\r\n";
            if (i == 10) header = "ETag: \"device-etag\"\r\n";
            answer(fd, bodies[i], header, i == expected.size() - 1 ? 204 : 200);
            close(fd);
        }
        close(listenFd);
    });

    suiteosd::CoreAdminClient client(
        "http://127.0.0.1:" + std::to_string(port));
    assert(!client.authenticated());
    assert(client.login("admin", "test-password").ok);
    assert(client.authenticated());

    std::vector<suiteosd::CoreItem> backends;
    assert(client.backends(backends).ok);
    assert(backends.size() == 1 && backends[0].id == "default");
    suiteosd::CoreAddon addon;
    assert(client.addon("default", addon).ok && !addon.enabled && addon.revision == 5);
    suiteosd::CoreAddon activated;
    assert(client.setAddon(addon, true, activated).ok);
    assert(activated.enabled && activated.revision == 6);

    suiteosd::CoreEncoding encoding;
    assert(client.encoding("default", encoding).ok && encoding.importCrf == 23);
    auto desired = encoding;
    desired.importCrf = 24;
    suiteosd::CoreEncoding saved;
    assert(client.setEncoding(encoding, desired, saved).ok);
    assert(saved.revision == "fedcba9876543210" && saved.importCrf == 24);

    std::vector<suiteosd::CoreItem> accounts;
    assert(client.accounts(accounts).ok && accounts.size() == 1);
    assert(client.changeAccount(accounts[0], "", true, false).ok);

    std::vector<suiteosd::CoreItem> devices;
    assert(client.devices(devices).ok && devices.size() == 1);
    assert(client.revokeDevice(devices[0]).ok);
    suiteosd::CorePersonSearch persons;
    assert(client.recordingPersons("default", "Will Smith", 3, 0, persons).ok);
    assert(persons.total == 10 && persons.recordings.size() == 3);
    assert(persons.limit == 3 && persons.offset == 0);
    assert(persons.recordings[0].title == "Men in Black 3");
    assert(persons.recordings[0].nativeId == "/video/SciFi/Men in Black 3/2026-01-01.20.15.1-0.rec");
    assert(persons.recordings[0].character == "Agent J");
    assert(persons.recordings[0].role == "actor");
    assert(persons.recordings[0].artworkUrl == "/recording-artwork/default/test");
    assert(persons.recordings[1].title == "Men in Black");
    assert(persons.recordings[2].character == "Capt. Steven Hiller");
    suiteosd::CoreGlobalSearch search;
    assert(client.globalSearch("default", "Jürgen & Co", 24, 24, search).ok);
    assert(search.offset == 24 && search.recordingTotal == 25 && search.epgTotal == 30);
    assert(search.recordings[0].matchReason == "person");
    assert(search.epg[0].eventId == "7" && search.epgHasMore);
    assert(search.people[0].recordingCount == 25);
    // Real-host response shapes, reduced fixtures with synthetic native paths.
    // Independent person/global counts and ordering must not be conflated.
    assert(client.globalSearch("default", "Will Smith", 3, 0, search).ok);
    assert(search.status == "ready" && search.recordingTotal == 9 && search.epgTotal == 1);
    assert(search.recordingHasMore && !search.epgHasMore);
    assert(search.recordings.size() == 3 && search.recordings[0].title == "Das Streben nach Glück");
    assert(search.recordings[1].nativeId == persons.recordings[2].nativeId);
    assert(search.recordings[1].title == "SciFi/Independence Day");
    assert(search.epg.size() == 1 && search.epg[0].eventId == "62892");
    assert(search.epg[0].channelName == "Warner TV Film HD" && search.epg[0].matchReason == "title");
    assert(search.people.size() == 1 && search.people[0].recordingCount == 3);
    assert(search.people[0].imageUrl == "/api/recordings/metadata/image?kind=person&index=0");
    assert(persons.total == 10);
    client.logout();
    assert(!client.authenticated());
    mock.join();

    suiteosd::CoreAdminClient operatorConfigured("http://192.0.2.1:18080");
    assert(operatorConfigured.configured());
    suiteosd::CoreAdminClient allowed("https://suite.example.invalid");
    assert(allowed.configured());
    return 0;
}
