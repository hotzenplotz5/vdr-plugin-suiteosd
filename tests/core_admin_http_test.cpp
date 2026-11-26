#include "../core_admin_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstdlib>
#include <sys/time.h>
#include <sys/stat.h>
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
            "POST /api/media/continue-watching",
            "POST /api/media/recently-watched",
            "GET /api/metadata/genres?backend=default&scope=recordings&locale=de",
            "GET /api/metadata/genres/recordings?backend=default&genre=action&limit=12&offset=0",
            "GET /api/vdr/recordings/folder?backend=default&path=&limit=12&offset=0",
            "GET /api/vdr/recordings/query?backend=default&sort=movieReleaseDate&order=desc&movieReleaseYearFrom=2022&movieReleaseYearTo=2026&limit=12&offset=0",
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
            R"({"items":[{"backendId":"default","recordingId":"1","backendNativeId":"/video/a.rec","title":"Film A","resumePositionSeconds":3541,"durationKnown":true,"durationSeconds":8716,"recording":{"id":"1","backendId":"default","backendNativeId":"/video/a.rec"}}]})",
            R"({"items":[{"backendId":"default","recordingId":"2","title":"Film B","positionKnown":true,"positionSeconds":25,"completionKnown":true,"completed":false,"recording":{"id":"2","backendId":"default","backendNativeId":"/video/b.rec"}}]})",
            R"({"genres":[{"id":"action","label":"Action","count":395},{"id":"empty","count":0}]})",
            R"({"recordings":[{"id":"1","backendId":"default","backendNativeId":"/video/a.rec","title":"Film A"}],"totalCount":395,"limit":12,"offset":0,"hasMore":true})",
            R"({"backendId":"default","recordingCount":1,"folders":[{"path":"Action","name":"Action","recordingCount":51},{"path":"Film B","name":"Film B","singleRecordingLeaf":true,"singleRecording":{"id":"2","backendId":"default","backendNativeId":"/video/b.rec","title":"Film B"}}],"recordings":[{"id":"1","backendId":"default","backendNativeId":"/video/a.rec","title":"Film A"}]})",
            R"({"recordings":[{"id":"3","backendId":"default","backendNativeId":"/video/c.rec","metadata":{"presentation":{"title":"The Uprising"},"provider":{"contentKind":"movie","releaseDate":"2026-01-01"}}}],"totalCount":1})",
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
    suiteosd::CoreHomePage page;
    using suiteosd::HomeKind;
    assert(client.homePage("default",HomeKind::Continue,"",12,0,2026,page).ok);
    assert(page.cards.size()==1&&page.cards[0].position==3541&&page.cards[0].duration==8716);
    assert(page.cards[0].recording.nativeId=="/video/a.rec");
    assert(client.homePage("default",HomeKind::Recent,"",12,0,2026,page).ok);
    assert(page.cards.size()==1&&page.cards[0].position==25&&!page.cards[0].completed);
    assert(client.homePage("default",HomeKind::Genres,"",12,0,2026,page).ok);
    assert(page.cards.size()==1&&page.cards[0].count==395);
    assert(client.homePage("default",HomeKind::Genre,"action",12,0,2026,page).ok);
    assert(page.total==395&&page.hasMore&&page.nextOffset==1);
    assert(client.homePage("default",HomeKind::Folders,"",12,0,2026,page).ok);
    assert(page.cards.size()==2&&page.cards[0].title=="Hauptverzeichnis"&&page.cards[0].count==2);
    assert(page.cards[1].count==51);
    assert(client.homePage("default",HomeKind::Movies,"",12,0,2026,page).ok);
    assert(page.cards.size()==1&&page.cards[0].subtitle=="2026"&&page.cards[0].title=="The Uprising");
    client.logout();
    assert(!client.authenticated());
    mock.join();

    // A real AF_UNIX request carries no Basic auth, Cookie or CSRF token.
    char directory[]="/tmp/suiteosd-http-XXXXXX";
    assert(mkdtemp(directory));
    const std::string socketPath=std::string(directory)+"/core.sock";
    const int localFd=socket(AF_UNIX,SOCK_STREAM,0);
    assert(localFd>=0);
    sockaddr_un localAddress{};
    localAddress.sun_family=AF_UNIX;
    assert(socketPath.size()<sizeof(localAddress.sun_path));
    std::strcpy(localAddress.sun_path,socketPath.c_str());
    assert(bind(localFd,reinterpret_cast<sockaddr*>(&localAddress),sizeof(localAddress))==0);
    assert(listen(localFd,2)==0);
    std::thread localMock([&]{
        const int fd=accept(localFd,nullptr,nullptr);assert(fd>=0);
        const auto request=readRequest(fd);
        assert(request.method=="GET");
        assert(request.path=="/api/search?backend=default&query=Will%20Smith&limit=20&offset=0");
        assert(!request.headers.count("authorization"));
        assert(!request.headers.count("cookie"));
        assert(!request.headers.count("x-csrf-token"));
        answer(fd,R"({"query":"Will Smith","backendId":"default","status":"ready","recordings":[],"epg":[],"people":[],"recordingTotal":0,"epgTotal":0,"limit":20,"offset":0})");
        close(fd);close(localFd);
    });
    suiteosd::CoreAdminClient native("http://localhost",socketPath);
    native.setOrigin("https://suite.example.invalid");
    suiteosd::CoreGlobalSearch nativeResult;
    assert(native.globalSearch("default","Will Smith",20,0,nativeResult).ok);
    assert(nativeResult.status=="ready"&&!native.authenticated());
    localMock.join();
    unlink(socketPath.c_str());rmdir(directory);
    assert(!native.globalSearch("default","Will Smith",20,0,nativeResult).ok);

    // Pairing uses the public bootstrap token, then only the Device header.
    const int pairFd=socket(AF_INET, SOCK_STREAM, 0);
    assert(pairFd>=0);
    bindTo.sin_port=0;
    assert(bind(pairFd,reinterpret_cast<sockaddr*>(&bindTo),sizeof(bindTo))==0);
    assert(listen(pairFd,2)==0);
    len=sizeof(bindTo);
    assert(getsockname(pairFd,reinterpret_cast<sockaddr*>(&bindTo),&len)==0);
    std::thread pairMock([&]{
        for(int i=0;i<12;++i){
            const int fd=accept(pairFd,nullptr,nullptr);assert(fd>=0);
            const auto req=readRequest(fd);
            assert(!req.headers.count("cookie")&&!req.headers.count("x-csrf-token"));
            if(i==0){
                assert(req.method=="POST"&&req.path=="/api/v1/device-pairings");
                assert(req.body.find("suiteosd")!=std::string::npos);
                assert(!req.headers.count("authorization"));
                answer(fd,R"({"pairingRequestId":"pair-a","userCode":"ABCD","pairingToken":"token-a","status":"pending","pollIntervalSeconds":3,"expiresAt":"2026-11-26T19:47:45Z"})","",201);
            }else if(i>=1&&i<=3){
                assert(req.headers.at("x-vdr-suite-pairing-token")=="token-a");
                assert(!req.headers.count("authorization"));
                if(i<=2){
                    assert(req.method=="GET"&&req.path=="/api/v1/device-pairings/pair-a");
                    answer(fd,i==1?R"({"status":"pending","pollIntervalSeconds":4})":R"({"status":"approved"})");
                }else{
                    assert(req.method=="POST"&&req.path=="/api/v1/device-pairings/pair-a/credential"&&req.body.empty());
                    answer(fd,R"({"deviceId":"device-a","credentialId":"cred-a","credentialSecret":"secret-a"})","",201);
                }
            }else{
                assert(req.method=="GET");
                assert(req.headers.at("authorization")=="VDR-Suite-Device cred-a.secret-a");
                assert(!req.headers.count("x-vdr-suite-pairing-token"));
                if(i<7){
                    assert(req.path=="/api/v1");
                    answer(fd,R"({"authentication":{"authenticated":true}})");
                }else if(i==7){
                    assert(req.path=="/api/v1/search/recordings?backendId=default&query=Will%20Smith&limit=20&offset=0");
                    answer(fd,R"({"items":[{"kind":"recording","backendId":"default","recordingId":"rec_0123456789abcdef0123456789abcdef","title":"I, Robot"}],"page":{"offset":0,"limit":20,"totalCount":11,"hasMore":false}})");
                }else if(i==8){
                    assert(req.path=="/api/v1/recordings/rec_0123456789abcdef0123456789abcdef?backendId=default");
                    answer(fd,R"({"recordingId":"rec_0123456789abcdef0123456789abcdef","backendId":"default","description":"Film"})");
                }else if(i==9){
                    assert(req.path=="/api/v1/search/epg?backendId=default&query=Hobbit&limit=20&offset=20");
                    answer(fd,R"({"items":[{"kind":"epg","backendId":"default","eventId":"evt-a","channelId":"channel-a","title":"Young Sheldon","subtitle":"Hobbit","startTime":"2026-11-26T20:00:00Z","endTime":"2026-11-26T20:30:00Z"}],"page":{"offset":20,"limit":20,"totalCount":21,"hasMore":false}})");
                }else if(i==10){
                    assert(req.path=="/api/v1/search/recordings?backendId=default&query=Ewan%20McGregor&limit=20&offset=0");
                    answer(fd,R"({"code":"permission_denied"})","",403);
                }else{
                    assert(req.path=="/api/v1/events?backendId=default&domain=recordings");
                    assert(req.headers.at("accept")=="text/event-stream");
                    const std::string body="event: hello\ndata: {\"backendId\":\"default\",\"daemonEpoch\":\"daemon_test\",\"subscriptionSequence\":0,\"authorizedDomains\":[\"recordings\"]}\n\n"
                        "event: update\ndata: {\"backendId\":\"default\",\"daemonEpoch\":\"daemon_test\",\"subscriptionSequence\":1,\"changedDomains\":[\"recordings\"]}\n\n";
                    const std::string response="HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n"+body;
                    assert(send(fd,response.data(),response.size(),MSG_NOSIGNAL)==ssize_t(response.size()));
                }
            }
            close(fd);
        }
        close(pairFd);
    });
    suiteosd::CoreAdminClient pairingClient("http://127.0.0.1:"+std::to_string(ntohs(bindTo.sin_port)));
    suiteosd::DevicePairing pairing;
    suiteosd::DeviceCredential credential;
    char vaultDir[]="/tmp/suiteosd-profile-XXXXXX";
    assert(mkdtemp(vaultDir));
    const auto vault=std::string(vaultDir)+"/devices.json";
    assert(pairingClient.beginPairing("OSD","test",pairing).ok&&pairing.userCode=="ABCD");
    assert(pairing.pollIntervalSeconds==3&&pairing.expiresAtEpoch>0);
    assert(!pairingClient.issuePairing(pairing,credential).ok);
    assert(pairingClient.completePairing(pairing,credential,vault).ok&&pairing.state=="pending");
    assert(pairing.pollIntervalSeconds==4&&credential.secret.empty());
    // Approved GET + one-shot POST, simulate local storage failure.
    assert(!pairingClient.completePairing(pairing,credential,vault+"/missing").ok);
    assert(pairing.token.empty()&&!credential.secret.empty());
    const auto issuedCopy=credential;
    // Retry storage only, then the authenticated GET. No second POST.
    assert(pairingClient.completePairing(pairing,credential,vault).ok&&credential.secret.empty());
    // Repeat status after completion and after reopening setup. Both must
    // authenticate with the existing credential, never issue a second POST.
    assert(pairing.requestId.empty()&&pairing.token.empty());
    const auto checked=pairingClient.completePairing(pairing,credential,vault);
    assert(checked.ok&&checked.message=="Erfolgreich gekoppelt");
    suiteosd::CoreAdminClient reopened(issuedCopy.origin);
    const auto restoredStatus=reopened.completePairing(pairing,credential,vault);
    assert(restoredStatus.ok&&restoredStatus.message=="Erfolgreich gekoppelt");
    assert(!reopened.completePairing(pairing,credential,vault+"/missing").ok);
    suiteosd::CoreGlobalSearch publicResults;
    assert(reopened.publicSearch("default","Will Smith",false,20,0,publicResults).ok);
    assert(publicResults.recordingTotal==11&&publicResults.recordings.size()==1&&publicResults.recordings[0].nativeId.empty());
    suiteosd::CoreSearchEvent detail;
    assert(reopened.publicRecordingDetail(publicResults.recordings[0],detail).ok&&detail.description=="Film");
    assert(reopened.publicSearch("default","Hobbit",true,20,20,publicResults).ok);
    assert(publicResults.epgTotal==21&&publicResults.epg[0].subtitle=="Hobbit");
    const auto denied=reopened.publicSearch("default","Ewan McGregor",false,20,0,publicResults);
    assert(!denied.ok&&denied.http==403&&denied.message=="permission_denied");
    std::atomic<bool> stop{false},changed{false};
    reopened.recordingEvents("default",stop,changed);
    assert(changed.load());
    pairMock.join();
    assert(suiteosd::loadDeviceProfile(vault,issuedCopy.origin,credential)&&credential.secret==issuedCopy.secret);
    pairingClient.setOrigin("https://other.invalid");
    assert(!pairingClient.setDeviceCredential(credential));
    assert(!pairingClient.testDevice().ok);
    suiteosd::DeviceCredential other{"https://other.invalid","device-b","cred-b","secret-b"};
    assert(suiteosd::saveDeviceProfile(vault,credential));
    assert(suiteosd::saveDeviceProfile(vault,other));
    struct stat vaultStat{};
    assert(stat(vault.c_str(),&vaultStat)==0&&(vaultStat.st_mode&0777)==0600);
    suiteosd::DeviceCredential restored;
    assert(suiteosd::loadDeviceProfile(vault,credential.origin,restored));
    assert(restored.secret==credential.secret&&restored.deviceId==credential.deviceId);
    assert(suiteosd::loadDeviceProfile(vault,other.origin,restored)&&restored.secret=="secret-b");
    assert(suiteosd::removeDeviceProfile(vault,credential.origin));
    assert(!suiteosd::loadDeviceProfile(vault,credential.origin,restored));
    assert(suiteosd::loadDeviceProfile(vault,other.origin,restored)&&restored.secret=="secret-b");
    unlink(vault.c_str());rmdir(vaultDir);

    suiteosd::CoreAdminClient operatorConfigured("http://192.0.2.1:18080");
    assert(operatorConfigured.configured());
    suiteosd::CoreAdminClient allowed("https://suite.example.invalid");
    assert(allowed.configured());
    return 0;
}
