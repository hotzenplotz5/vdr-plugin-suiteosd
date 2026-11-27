/*
 * VDR Suite OSD – first native Home screen.
 *
 * Draws real VDR EPG/recording data with a remote-only scrollable home.
 * This is not a cSkin replacement or an embedded HTML/video player.
 */
#include "navigation.h"
#include "home_cache.h"
#include "web_palette.h"
#include "sidebar_icons.h"
#include "settings_navigation.h"
#include "core_admin_client.h"
#include "pairing_dialog.h"
#include "core_admin_navigation.h"
#include "epg_timeline.h"
#include "recording_browser.h"
#include "photo_gallery.h"
#include "photo_carousel.h"
#include "logo_locator.h"
#include "artwork_decode.h"
#include "cinematic_home.h"
#include "scraper_detail.h"
#include "recording_detail_view.h"
#include "search_navigation.h"
#include "vendor/tvscraper/services.h"

#include <vdr/channels.h>
#include <vdr/device.h>
#include <vdr/epg.h>
#include <vdr/font.h>
#include <vdr/menu.h>
#include <vdr/menuitems.h>
#include <vdr/config.h>
#include <vdr/osd.h>
#include <vdr/osdbase.h>
#include <vdr/plugin.h>
#include <vdr/recording.h>
#include <vdr/remote.h>
#include <vdr/timers.h>
#include <vdr/videodir.h>
#include <vdr/svdrp.h>

#include <algorithm>
#include <cstdlib>
#include <array>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <future>
#include <deque>
#include <thread>
#include <ctime>
#include <cstring>
#include <map>
#include <sstream>
#include <utility>
#include <string>
#include <vector>

static const char *VERSION = "0.9.23-dev";
static const char *DESCRIPTION = trNOOP("VDR Suite TV OSD");

namespace suiteosd { void RegisterSuiteSkin(); }

namespace {
constexpr tColor kBackground = suiteosd::webBackground;
constexpr tColor kSurface = suiteosd::webSurface;
constexpr tColor kCard = suiteosd::webCard;
constexpr tColor kCardAlt = suiteosd::webCardAlt;
constexpr tColor kFocus = suiteosd::webFocus;
constexpr tColor kHighlight = suiteosd::webPrimary;
constexpr tColor kText = suiteosd::webText;
constexpr tColor kMuted = suiteosd::webMuted;
constexpr tColor kLine = suiteosd::webLine;

// Suite-OSD configuration is stored under the plugin owner in VDR setup.conf.
// This variable is loaded by SetupParse and written only through SetupStore.
int gLivePreviewEnabled = 1;
// Persisted through suiteosd.PhotoView: 0 = grid, 1 = carousel.
int gPhotoViewMode = 1;
char gCoreUrl[256] = "http://127.0.0.1:18080";
char gDeviceName[129] = "VDR Suite OSD";

std::string deviceProfilePath()
{
    const char *dir = cPlugin::ConfigDirectory("suiteosd");
    return dir && *dir ? std::string(dir) + "/device-profiles.json" : "";
}
bool restoreDevice(suiteosd::CoreAdminClient &client)
{
    suiteosd::DeviceCredential credential;
    return suiteosd::loadDeviceProfile(deviceProfilePath(),client.origin(),credential) &&
           client.setDeviceCredential(credential);
}
std::string gReplayPersonSearch;

struct Programme {
    int channelNumber = 0;
    std::string channel;
    std::string title;
    std::string time;
    std::string description;
    int progress = 0;
    tEventID eventId = 0;
    time_t start = 0, end = 0;
    std::string subtitle;
};
struct Recording {
    std::string title;
    std::string baseName;
    std::string time;
    time_t start = 0;
    std::string fileName;
    std::string description;
};
struct RecordingMeta {
    std::string title;
    std::string episode;
    std::string release;
    std::string overview;
    std::vector<std::string> genres;
    int season = 0;
    int episodeNumber = 0;
    int runtime = 0;
    float rating = 0;
    suiteosd::Artwork artwork;
};
struct RecordingDetailImage {
    suiteosd::DetailMetadata metadata;
    suiteosd::Artwork fanart;
    suiteosd::Artwork poster;
};

struct Snapshot {
    Programme live;
    Programme liveNext;
    std::vector<Programme> now;
    std::vector<Programme> next;
    std::vector<Recording> recordings;
};

std::string formatTime(time_t t)
{
    char buf[16] = {};
    struct tm tmValue;
    if (localtime_r(&t, &tmValue))
        strftime(buf, sizeof(buf), "%H:%M", &tmValue);
    return buf;
}

std::string recordingDate(time_t t)
{
    if (!t) return "";
    char buf[64] = {};
    tm value{};
    if (localtime_r(&t,&value)) strftime(buf,sizeof(buf),"%d.%m.%Y %H:%M",&value);
    return buf;
}

std::string eventTime(const cEvent *event)
{
    if (!event)
        return "";
    return formatTime(event->StartTime()) + " – " + formatTime(event->EndTime());
}

Programme fromEvent(const cChannel *ch, const cEvent *event)
{
    Programme p;
    p.channelNumber = ch->Number();
    p.channel = ch->Name() ? ch->Name() : "";
    p.title = event && event->Title() ? event->Title() : "Keine Programminformation";
    p.time = eventTime(event);
    if (event) { p.eventId=event->EventID(); p.start=event->StartTime(); p.end=event->EndTime();
        p.subtitle=event->ShortText()?event->ShortText():""; }
    p.description = event && event->Description() ? event->Description() : "";
    if (event && event->Duration() > 0) {
        const time_t now = time(nullptr);
        p.progress = std::clamp(int(100 * (now - event->StartTime()) / event->Duration()), 0, 100);
    }
    return p;
}

Snapshot readVdrSnapshot()
{
    Snapshot snapshot;
    // Copy strings while holding the authoritative VDR read locks.
    // No cChannel/cEvent/cRecording pointers escape this function.
    {
        LOCK_CHANNELS_READ;
        LOCK_SCHEDULES_READ;
        const int liveNumber = cDevice::CurrentChannel();
        if (Channels) {
            for (const cChannel *ch = Channels->First(); ch; ch = Channels->Next(ch)) {
                if (ch->GroupSep() || ch->Number() <= 0)
                    continue;
                const cSchedule *schedule = Schedules ? Schedules->GetSchedule(ch) : nullptr;
                const cEvent *now = schedule ? schedule->GetPresentEvent() : nullptr;
                const cEvent *next = schedule ? schedule->GetFollowingEvent() : nullptr;
                Programme current = fromEvent(ch, now);
                if (liveNumber > 0 && ch->Number() == liveNumber) {
                    snapshot.live = current;
                    snapshot.liveNext = fromEvent(ch, next);
                }
                // Keep VDR channel order and expose the complete rail;
                // horizontal navigation pages through every channel.
                snapshot.now.push_back(current);
                snapshot.next.push_back(fromEvent(ch, next));
            }
        }
    }
    {
        LOCK_RECORDINGS_READ;
        if (Recordings) {
            for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec)) {
                Recording r;
                r.title = rec->Name() ? rec->Name() : "";
                cString baseName = rec->BaseName();
                r.baseName = *baseName ? *baseName : r.title;
                r.start = rec->Start();
                r.time = formatTime(r.start);
                r.fileName = rec->FileName() ? rec->FileName() : "";
                if (rec->Info() && rec->Info()->Description())
                    r.description = rec->Info()->Description();
                snapshot.recordings.push_back(std::move(r));
            }
        }
    }
    std::stable_sort(snapshot.recordings.begin(),snapshot.recordings.end(),[](const Recording &a,const Recording &b){return a.start>b.start;});
    return snapshot;
}


suiteosd::Timeline ReadNativeTimeline()
{
    suiteosd::Timeline timeline;
    const time_t now = time(nullptr);
    const int tunedChannel = cDevice::CurrentChannel();
    // Read and copy actual schedules. cChannel and cEvent pointers never
    // escape VDR's read lock or enter the EPG navigation model.
    {
        LOCK_CHANNELS_READ;
        LOCK_SCHEDULES_READ;
        if (Channels) {
            std::string currentGroup = "Weitere Sender";
            for (const cChannel *channel = Channels->First(); channel;
                 channel = Channels->Next(channel)) {
                if (channel->GroupSep()) {
                    currentGroup = channel->Name() ? channel->Name() : "";
                    if (currentGroup.empty()) currentGroup = "Weitere Sender";
                    continue;
                }
                if (channel->Number() <= 0) continue;
                suiteosd::TimelineChannel row;
                row.number = channel->Number();
                row.name = channel->Name() ? channel->Name() : "";
                row.shortName = channel->ShortName(true) ? channel->ShortName(true) : "";
                row.group = currentGroup;
                const cSchedule *schedule = Schedules ?
                    Schedules->GetSchedule(channel) : nullptr;
                if (schedule) {
                    for (const cEvent *event = schedule->Events()->First();
                         event; event = schedule->Events()->Next(event)) {
                        if (event->EndTime() <= now - 3600)
                            continue;
                        if (event->StartTime() >= now + 49 * 3600)
                            break;
                        if (event->EndTime() <= event->StartTime())
                            continue;
                        suiteosd::TimelineEvent e;
                        e.id = event->EventID();
                        e.start = event->StartTime();
                        e.end = event->EndTime();
                        e.title = event->Title() ? event->Title() : "";
                        e.subtitle = event->ShortText() ? event->ShortText() : "";
                        e.description = event->Description() ? event->Description() : "";
                        row.events.push_back(std::move(e));
                    }
                }
                timeline.channels.push_back(std::move(row));
            }
        }
    }
    timeline.InitializeGroups();
    timeline.Reset(now, tunedChannel);
    return timeline;
}

const std::array<const char *, 13> kSidebar = {{
    "Home", "Live-TV", "EPG", "Aufnahmen", "Timer",
    "Suchtimer", "HbbTV", "Teletext", "Bilder", "Media Tools",
    "Einstellungen", "Suche", "VDR-Menü"
}};


// This is the menu VDR asks for through cControl::GetInfo() when Info is
// pressed during replay. It displays local TVScraper data but leaves the
// playback engine and transport keys entirely with cReplayControl.
class cSuiteReplayInfo final : public cOsdObject {
    cOsd *view = nullptr;
    cFont *titleFont = nullptr;
    cFont *bodyFont = nullptr;
    cFont *smallFont = nullptr;
    int width = 1600, height = 900;
    std::string title, episode, overview, release, genres;
    std::string fanartPath, posterPath;
    suiteosd::DetailMetadata detail;
    suiteosd::CreditNavigation castNav;
    std::map<std::string, suiteosd::Artwork> castCache;
    suiteosd::Artwork heroFanart, heroPoster;
    std::string recorded;
    int descriptionScroll = 0;
    int runtime = 0;
    int season = 0, episodeNumber = 0;
    float rating = 0;

    int X(int x) const { return x * width / 1600; }
    int Y(int y) const { return y * height / 900; }
    void box(int x0, int y0, int x1, int y1, tColor color)
    {
        if (view) view->DrawRectangle(X(x0), Y(y0), X(x1), Y(y1), color);
    }
    void label(int x, int y, const std::string &s, const cFont *font,
               tColor fg = kText, int max = 0)
    {
        if (view && font && !s.empty())
            view->DrawText(X(x), Y(y), s.c_str(), fg, clrTransparent,
                           font, max ? X(max) : 0);
    }
    void artwork(const std::string &path, int x, int y, int w, int h)
    {
        if (!view || path.empty()) return;
        suiteosd::Artwork image = suiteosd::LoadArtwork(path, X(w), Y(h));
        if (!image.valid()) return;
        cImage osdImage(cSize(image.width, image.height), image.argb.data());
        view->DrawImage(cPoint(X(x) + (X(w) - image.width) / 2,
                               Y(y) + (Y(h) - image.height) / 2), osdImage);
    }

public:
    explicit cSuiteReplayInfo(const cRecording *recording)
    {
        // Caller holds LOCK_RECORDINGS_READ. Copy everything before it ends.
        if (!recording) return;
        recorded = recordingDate(recording->Start());
        cString baseName = recording->BaseName();
        title = *baseName ? *baseName : (recording->Name() ? recording->Name() : "");
        if (recording->Info()) {
            if (recording->Info()->Title())
                title = recording->Info()->Title();
            if (recording->Info()->ShortText())
                episode = recording->Info()->ShortText();
            if (recording->Info()->Description())
                overview = recording->Info()->Description();
        }
        cPlugin *scraper = cPluginManager::GetPlugin("tvscraper");
        if (!scraper) return;
        cGetScraperVideo request(nullptr, recording);
        if (!scraper->Service("GetScraperVideo", &request) || !request.m_scraperVideo)
            return;
        detail = suiteosd::ReadScraperDetail(request.m_scraperVideo.get());
        if (!detail.title.empty()) title = detail.title;
        if (!detail.episode.empty()) episode = detail.episode;
        if (!detail.overview.empty()) overview = detail.overview;
        release = detail.release;
        runtime = detail.runtime;
        rating = detail.rating;
        season = detail.season;
        episodeNumber = detail.episodeNumber;
        fanartPath = detail.fanartPath;
        posterPath = detail.posterPath;
        for (const auto &genre : detail.genres) {
            if (!genres.empty()) genres += "  ·  ";
            genres += genre;
        }
    }

    ~cSuiteReplayInfo() override
    {
        delete view;
        delete titleFont;
        delete bodyFont;
        delete smallFont;
    }
    void Show() override
    {
        width = cOsd::OsdWidth();
        height = cOsd::OsdHeight();
        view = cOsdProvider::NewOsd(cOsd::OsdLeft(), cOsd::OsdTop());
        if (!view) return;
        const tArea full = {0, 0, width - 1, height - 1, 32};
        if (view->SetAreas(&full, 1) != oeOk) {
            delete view;
            view = nullptr;
            esyslog("suiteosd: TrueColor OSD unavailable for replay Info");
            return;
        }
        titleFont = cFont::CreateFont("Sans:Bold", std::max(20, Y(49)));
        bodyFont = cFont::CreateFont("Sans", std::max(14, Y(24)));
        smallFont = cFont::CreateFont("Sans", std::max(12, Y(19)));
        heroFanart = suiteosd::LoadDetailArtwork(fanartPath,X(1050),Y(500),true);
        heroPoster = suiteosd::LoadDetailArtwork(posterPath,X(262),Y(402));
        if(detail.title.empty()) detail.title=title;
        if(detail.episode.empty()) detail.episode=episode;
        Render();
    }
    void Render()
    {
        if (!view) return;
        box(0,0,1599,899,kBackground);
        box(0,0,1599,77,kSurface);
        label(36,25,"VDR Suite",bodyFont,kText,225);
        label(286,25,"WIEDERGABE / AUFNAHMEINFO",bodyFont,kMuted);
        label(1430,25,formatTime(time(nullptr)),bodyFont);
        suiteosd::DrawRecordingHero(view,width,height,titleFont,bodyFont,smallFont,
            detail,title,recorded,overview,heroFanart,heroPoster,descriptionScroll,&castNav,castCache);
        box(17,817,1580,889,kSurface);
        label(56,837,castNav.active ? "OK: Person suchen · Links/Rechts: Auswahl" :
              "OK / Info / Zurück: Wiedergabe",bodyFont,kFocus,820);
        label(925,837,castNav.active ? "Hoch / Zurück: Details" :
              "Runter / Blau: Personen",bodyFont,kText,610);
        view->Flush();
    }
    eOSState ProcessKey(eKeys key) override
    {
        const eKeys base = static_cast<eKeys>(key & ~k_Repeat);
        if (castNav.active) {
            if (base == kOk && castNav.selected >= 0 && castNav.selected < int(detail.people.size())) {
                if (cRemote::CallPlugin("suiteosd")) {
                    gReplayPersonSearch = detail.people[castNav.selected].name;
                    return osBack;
                }
            } else if (base == kBack || base == kUp || base == kInfo) castNav.Close();
            else if (base == kLeft || base == kRight)
                castNav.Move(base == kLeft ? -1 : 1, int(detail.people.size()));
            else return osContinue;
            Render();
            return osContinue;
        }
        if ((base == kDown || base == kBlue) && !detail.people.empty()) {
            castNav.Open(); Render(); return osContinue;
        }
        if (base == kLeft || base == kRight) {
            const auto lines = suiteosd::HeroLines(overview,bodyFont,X(suiteosd::kHeroTextWidth));
            descriptionScroll=std::clamp(descriptionScroll+(base==kLeft?-1:1),0,
                std::max(0,int(lines.size())-suiteosd::kHeroTextRows));
            Render();
            return osContinue;
        }
        return base == kOk || base == kBack || base == kInfo ? osBack : osContinue;
    }
};

class cSuiteReplayControl final : public cReplayControl {
public:
    cOsdObject *GetInfo() override
    {
        // As in VDR's default cReplayControl::GetInfo(), look up the
        // current replay by its canonical name, not a parallel player.
        LOCK_RECORDINGS_READ;
        const char *path = cReplayControl::LastReplayed();
        const cRecording *recording = path && Recordings ?
            Recordings->GetByName(path) : nullptr;
        return recording ? new cSuiteReplayInfo(recording) : nullptr;
    }
};

// Native Home pixels are independent of the device credentials and OSD
// provider. The plugin owns them; VDR's main thread owns all cache mutations.
// Only copied paths and metadata cross to the file/decode worker.
class NativeHomeCache {
    using HL=suiteosd::HomeLayout;
    using Clock=std::chrono::steady_clock;
    struct Work {
        std::string key;
        Programme programme;
        Recording recording;
        bool epg=false;
        int w=0,h=0,logoW=0,logoH=0,pad=0;
        unsigned long generation=0;
    };
    struct Result {
        Work work;
        suiteosd::Artwork poster,logo;
        suiteosd::DetailMetadata title;
        bool missing=false;
    };
    std::deque<Work> pending;
    std::future<Result> job;
    std::string inFlight;
    std::map<std::string,Clock::time_point> missing;
    unsigned long generation=0;
    int width=0,height=0;
    bool menu=false,stopped=false;
    Clock::time_point nextPrime{};
    std::vector<std::string> dirty;
    std::vector<std::uintptr_t> evicted;
    const suiteosd::Artwork empty;
    static std::size_t bytes(const suiteosd::Artwork &a){return a.argb.capacity()*sizeof(uint32_t);}
    static constexpr std::size_t budget=32U*1024U*1024U;
    void discard(const std::string &key,const suiteosd::Artwork &a){
        missing.erase(key);
        if(menu)evicted.push_back(reinterpret_cast<std::uintptr_t>(&a));
    }
    const suiteosd::Artwork &queue(Work request,bool visible){
        auto it=artwork.find(request.key);
        if(it!=artwork.end()){
            ++hits;
            auto failure=missing.find(request.key);
            if(failure==missing.end()||Clock::now()<failure->second)return it->second;
        }else ++misses;
        if(stopped||request.key==inFlight)return it==artwork.end()?empty:it->second;
        auto queued=std::find_if(pending.begin(),pending.end(),[&](const Work &w){return w.key==request.key;});
        if(queued!=pending.end()){
            if(visible&&queued!=pending.begin()){
                auto item=std::move(*queued);pending.erase(queued);pending.push_front(std::move(item));
            }
        }else{
            request.generation=generation;
            if(pending.size()>=64)pending.pop_back();
            if(visible)pending.push_front(std::move(request));else pending.push_back(std::move(request));
        }
        return it==artwork.end()?empty:it->second;
    }
    void launch(Work work){
        // Resolve VDR pointers and TVScraper on the same VDR thread as before.
        // Never retain a pointer or hold a VDR lock during file IO/decode.
        std::string path;
        suiteosd::DetailMetadata title;
        const auto began=Clock::now();
        if(auto *scraper=cPluginManager::GetPlugin("tvscraper")){
            if(work.epg){
                LOCK_CHANNELS_READ;
                LOCK_SCHEDULES_READ;
                const auto *ch=Channels?Channels->GetByNumber(work.programme.channelNumber):nullptr;
                const auto *schedule=ch&&Schedules?Schedules->GetSchedule(ch):nullptr;
                const auto *event=schedule? schedule->GetEventById(work.programme.eventId):nullptr;
                if(event&&work.programme.start&&event->StartTime()==work.programme.start){
                    cGetScraperVideo req(event,nullptr);
                    ++scraperCalls;
                    if(scraper->Service("GetScraperVideo",&req)&&req.m_scraperVideo)
                        path=req.m_scraperVideo->getImage(cImageLevels(eImageLevel::episodeMovie,eImageLevel::seasonMovie,eImageLevel::tvShowCollection),
                            cOrientations(eOrientation::portrait,eOrientation::landscape,eOrientation::banner),true).path;
                }
            }else{
                LOCK_RECORDINGS_READ;
                const auto *r=Recordings?Recordings->GetByName(work.recording.fileName.c_str()):nullptr;
                if(r&&r->Start()==work.recording.start){
                    cGetScraperVideo req(nullptr,r);
                    ++scraperCalls;
                    if(scraper->Service("GetScraperVideo",&req)&&req.m_scraperVideo){
                        auto *video=req.m_scraperVideo.get();
                        video->getOverview(&title.title,&title.episode,nullptr,nullptr,nullptr,nullptr,nullptr);
                        title.season=video->getSeasonNumber();title.episodeNumber=video->getEpisodeNumber();
                        path=video->getImage(cImageLevels(eImageLevel::episodeMovie,eImageLevel::seasonMovie,eImageLevel::tvShowCollection),
                            cOrientations(eOrientation::portrait,eOrientation::landscape),true).path;
                    }
                }
            }
        }
        scraperUs+=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-began).count();
        inFlight=work.key;
        job=std::async(std::launch::async,[work=std::move(work),path=std::move(path),title=std::move(title)]() mutable {
            Result result;result.work=std::move(work);result.title=std::move(title);
            const auto &w=result.work;
            result.poster=suiteosd::CoverHomePoster(suiteosd::LoadArtwork(path,std::min(1024,w.w*2),std::min(1024,w.h*2)),w.w,w.h,0,0xFF061626u);
            result.missing=!result.poster.valid();
            if(w.epg){
                result.logo=suiteosd::LoadArtwork(suiteosd::FindChannelLogo(w.programme.channel,""),w.logoW,w.logoH);
                if(!result.poster.valid()&&result.logo.valid())
                    result.poster={w.w,w.h,std::vector<uint32_t>(size_t(w.w)*w.h,0xFF061626u)};
                suiteosd::CinematicLogo(result.poster,result.logo,w.pad);
            }
            return result;
        });
    }
    void apply(Result result){
        if(result.work.generation!=generation)return;
        const auto &key=result.work.key;
        auto old=artwork.find(key);
        const bool changed=old==artwork.end()||old->second.argb!=result.poster.argb;
        if(old!=artwork.end()){
            // Do not replace good pixels with a transient missing scraper file.
            if(result.missing&&old->second.valid()){
                missing[key]=Clock::now()+std::chrono::minutes(1);return;
            }
            discard(key,old->second);artwork.erase(key);
        }
        while(artwork.size()&&
              (artwork.size()>=192||artwork.totalWeight(bytes)+bytes(result.poster)>budget))
            artwork.evictOldest([&](const auto &k,const auto &a){discard(k,a);});
        artwork.emplace(key,std::move(result.poster));
        if(result.missing)missing[key]=Clock::now()+std::chrono::minutes(1);
        if(!result.work.epg){
            if(titles.size()>=512)titles.evictOldest();
            titles.insertOrAssign(result.work.recording.fileName,std::move(result.title));
        }else if(result.logo.valid()&&logos.find(result.work.programme.channel)==logos.end()){
            if(logos.size()>=128)logos.evictOldest();
            logos.emplace(result.work.programme.channel,std::move(result.logo));
        }
        if(menu&&(changed||!result.work.epg))dirty.push_back(key);
    }
public:
    suiteosd::HomeLru<std::string,suiteosd::Artwork> artwork,logos;
    suiteosd::HomeLru<std::string,suiteosd::DetailMetadata> titles;
    unsigned long long hits=0,misses=0,scraperCalls=0,scraperUs=0,completed=0;
    static std::string programmeKey(const Programme &p){
        return "epg:"+std::to_string(p.channelNumber)+":"+p.channel+":"+std::to_string(p.eventId)+":"+std::to_string(p.start);
    }
    static std::string recordingKey(const Recording &r,int w,int h){
        return "rec:"+r.fileName+":"+std::to_string(w)+":"+std::to_string(h)+":"+std::to_string(r.start);
    }
    void reset(){
        ++generation;pending.clear();missing.clear();artwork.clear();titles.clear();logos.clear();
        dirty.clear();evicted.clear();nextPrime={};
    }
    void setSize(int w,int h){
        if(w==width&&h==height)return;
        reset();width=w;height=h;
    }
    void opened(){menu=true;dirty.clear();evicted.clear();}
    void closed(){menu=false;dirty.clear();evicted.clear();nextPrime={};}
    void cancelPending(){++generation;pending.clear();nextPrime={};}
    std::size_t pixelBytes()const{return artwork.totalWeight(bytes)+logos.totalWeight(bytes);}
    const suiteosd::Artwork &programme(const Programme &p,bool visible=true){
        Work w;w.key=programmeKey(p);w.programme=p;w.epg=true;
        w.w=HL::posterW*width/1600;w.h=HL::posterH*height/900;
        w.logoW=std::max(1,49*width/1600);w.logoH=std::max(1,30*height/900);w.pad=std::max(2,8*width/1600);
        return queue(std::move(w),visible);
    }
    const suiteosd::Artwork &recording(const Recording &r,bool visible=true){
        Work w;w.key=recordingKey(r,HL::posterW,HL::posterH);w.recording=r;
        w.w=HL::posterW*width/1600;w.h=HL::posterH*height/900;
        return queue(std::move(w),visible);
    }
    void takeChanges(std::vector<std::string> &keys,std::vector<std::uintptr_t> &ids){
        keys.swap(dirty);ids.swap(evicted);
    }
    void tick(){
        if(stopped)return;
        if(job.valid()&&job.wait_for(std::chrono::seconds(0))==std::future_status::ready){
            try{apply(job.get());++completed;}catch(const std::exception &){esyslog("suiteosd: native Home preparation failed");}
            inFlight.clear();
        }
        if(!menu&&Clock::now()>=nextPrime){
            nextPrime=Clock::now()+std::chrono::minutes(1);
            int w=0,h=0;double aspect=1.0;
            if(auto *device=cDevice::PrimaryDevice())device->GetOsdSize(w,h,aspect);
            const auto area=suiteosd::FullscreenOsdRect(w,h,{cOsd::OsdLeft(),cOsd::OsdTop(),cOsd::OsdWidth(),cOsd::OsdHeight()});
            setSize(area.w,area.h);nextPrime=Clock::now()+std::chrono::minutes(1);
            // Native snapshot only, never a server poll. Bound preparation to
            // the initial view, then the next rail and horizontal neighbors.
            const auto snapshot=readVdrSnapshot();
            for(size_t i=0;i<HL::columns&&i<snapshot.now.size();++i)programme(snapshot.now[i],false);
            for(size_t i=0;i<HL::columns&&i<snapshot.next.size();++i)programme(snapshot.next[i],false);
            for(size_t i=0;i<HL::columns&&i<snapshot.recordings.size();++i)recording(snapshot.recordings[i],false);
            for(size_t i=HL::columns;i<2*HL::columns&&i<snapshot.now.size();++i)programme(snapshot.now[i],false);
            for(size_t i=HL::columns;i<2*HL::columns&&i<snapshot.next.size();++i)programme(snapshot.next[i],false);
        }
        if(!job.valid()&&!pending.empty()){
            auto work=std::move(pending.front());pending.pop_front();
            try{launch(std::move(work));}catch(const std::exception &){inFlight.clear();esyslog("suiteosd: cannot launch native Home preparation");}
        }
    }
    void stop(){
        stopped=true;pending.clear();
        if(job.valid())job.wait();
    }
};

class cSuiteHome final : public cOsdObject {
private:
    NativeHomeCache &nativeHome;
    cOsd *osd = nullptr;
    cFont *fontTitle = nullptr;
    cFont *fontLarge = nullptr;
    cFont *fontBody = nullptr;
    cFont *fontSmall = nullptr;
    suiteosd::Navigation nav;
    suiteosd::Timeline timeline;
    suiteosd::SettingsNavigation settingsNav;
    bool settingsActive = false;
    std::string settingsNotice;
    suiteosd::CoreAdminClient coreAdmin{gCoreUrl};
    suiteosd::CoreAdminNavigation coreNav;
    // HTTP reads own their client on a worker, never the OSD/VDR objects.
    struct SearchRead {
        std::string origin, profile, query;
        suiteosd::CoreSearchRecording recording;
        int offset=0, selected=0;
        bool epg=false, detail=false, people=false, home=false, resume=false;
        std::int64_t position=-1;
        std::vector<std::string> nativeNames;
        unsigned long generation=0;
    };
    struct SearchReadback {
        SearchRead request;
        suiteosd::CoreStatus status;
        suiteosd::CoreGlobalSearch results;
        suiteosd::CoreSearchEvent detail;
        std::string nativeId;
    };
    bool homeDetailPending=false;
    std::future<SearchReadback> searchJob;
    SearchRead pendingSearch;
    bool searchQueued=false;
    unsigned long searchGeneration=0;
    suiteosd::SearchNavigation searchNav;
    suiteosd::CorePersonSearch personSearch;
    suiteosd::CoreGlobalSearch globalSearch;
    bool searchActive = false;
    struct HomeRail {
        suiteosd::CoreHomePage page;
        suiteosd::CoreStatus status;
        bool attempted = false;
        bool stale = false, loading = false;
        unsigned long requestId=0;
    };
    std::map<int,HomeRail> homeRails;
    std::atomic<bool> homeEventStop{false},homeEventChanged{false};
    std::future<void> homeEventJob,homeProgressJob;
    std::atomic<bool> homeProgressChanged{false};
    struct HomeRead {
        unsigned long id=0;
        int section=-1, offset=0, year=0;
        suiteosd::HomeKind kind=suiteosd::HomeKind::New;
        std::string origin,profile,key;
    };
    struct HomeReadback { HomeRead request; suiteosd::CoreStatus status; suiteosd::CoreHomePage page; };
    std::future<HomeReadback> homeReadJob;
    std::deque<HomeRead> homeReadQueue;
    struct HomePosterRead {
        std::string key,nativeId,assetId;
        suiteosd::Artwork poster;
        suiteosd::CoreSearchEvent detail;
        suiteosd::CoreStatus detailStatus,artStatus;
        std::chrono::steady_clock::time_point fetched{};
        unsigned long generation=0,revision=0;
        bool reused=false;
    };
    std::map<std::string,std::future<HomePosterRead>> homePosterJobs;
    suiteosd::HomeLru<std::string,HomePosterRead> publicHomePosters;
    unsigned long homePosterGeneration=0,homePosterRevision=0;
    unsigned long homeReadSerial=0;
    std::string recommendedGenreId,recommendedGenreTitle;
    struct HomeCollection {
        suiteosd::HomeKind kind;
        std::string key,title;
        HomeRail rail;
        int offset=0,selected=0,first=0;
        std::vector<int> previous;
    };
    std::vector<HomeCollection> homeCollections;
    Recording homeDetailRecording;
    std::int64_t homeResumeSeconds=-1;
    bool homeResumeDetail=false;
    bool searchFromReplay = false;
    int detailScroll = 0, searchOriginScroll = 0;
    std::string searchNotice;
    Recording searchRecording;
    suiteosd::CoreSearchEvent searchEvent;
    struct SearchFrame {
        suiteosd::SearchNavigation nav;
        suiteosd::CorePersonSearch person;
        suiteosd::CoreGlobalSearch global;
        Recording recording;
        suiteosd::CoreSearchEvent event;
        suiteosd::CreditNavigation cast;
        int scroll;
        std::string notice;
    };
    std::vector<SearchFrame> searchHistory;
    suiteosd::CreditNavigation searchOriginCast;
    std::vector<suiteosd::CoreItem> coreBackends;
    std::vector<suiteosd::CoreItem> coreAccounts;
    std::vector<suiteosd::CoreItem> coreDevices;
    suiteosd::CoreAddon coreAddon;
    suiteosd::CoreEncoding coreEncoding;
    suiteosd::CoreEncoding coreEncodingEdit;
    bool coreAddonLoaded = false;
    bool coreEncodingLoaded = false;
    std::string coreNotice;
    bool timelineActive = false;
    bool timelineDetails = false;
    bool timelineTimerConfirm = false;
    std::string timelineTimerNotice;
    std::map<std::string, suiteosd::Artwork> channelLogos;
    suiteosd::HomeLru<std::string, suiteosd::Artwork> &homeArtwork=nativeHome.artwork;
    std::map<std::string, suiteosd::Artwork> timelineArtwork;
    Snapshot data;
    std::map<std::string, RecordingMeta> recordingMeta;
    // Titles are copied together with the portrait path, not obtained by a second scraper request.
    suiteosd::HomeLru<std::string, suiteosd::DetailMetadata> &homeRecordingTitles=nativeHome.titles;
    std::map<std::string, RecordingDetailImage> recordingDetailImages;
    suiteosd::CreditNavigation detailCastNav;
    std::map<std::string, suiteosd::Artwork> detailCastCache;
    std::map<std::string, std::vector<std::string>> recordingGenres;
    suiteosd::RecordingBrowser recordingBrowser;
    bool recordingsActive = false;
    bool photosActive = false;
    suiteosd::PhotoGallery photoGallery;
    bool photoInfoVisible = false;
    float photoVisualCenter = 0.0f;
    float photoAnimationFrom = 0.0f;
    float photoAnimationTo = 0.0f;
    bool photoAnimating = false;
    std::chrono::steady_clock::time_point photoAnimationStart{};
    std::chrono::steady_clock::time_point photoLastFrame{};
    std::string photoNotice;
    std::map<std::string, imagenext::Thumbnail> photoThumbnails;
    bool browserDetails = false;
    bool browserDeleteConfirm = false;
    std::string browserNotice;
    cDevice *scaledDevice = nullptr;
    cRect scaledRect;
    bool scaledForHome = false;
    bool scaledHomeExpanded = false;
    std::chrono::steady_clock::time_point nextPreviewAttempt{};
    using HL = suiteosd::HomeLayout;
    // Only rail drawing is redirected while preparing a clipped animation strip.
    cPixmap *railDrawPixmap=nullptr;
    int railDrawX=0,railDrawY=0;
    std::array<suiteosd::Artwork,2> cinematicBackgrounds;
    suiteosd::HomeLru<std::array<int,7>,suiteosd::CinemaPatch> cinematicFrames;
    suiteosd::HomeLru<std::pair<std::array<int,7>,std::uintptr_t>,suiteosd::CinemaPatch> cinematicComposites;
    std::map<std::array<int,4>,suiteosd::CinemaPatch> cinematicGlyphs;
    cFont *fontCinemaBrand=nullptr,*fontCinemaCard=nullptr,*fontCinemaMeta=nullptr;
    suiteosd::HomeLru<std::string,suiteosd::Artwork> &cinematicLogos=nativeHome.logos;
    struct FocusPreview {
        suiteosd::DetailMetadata metadata;
        suiteosd::Artwork fanart;
        std::array<suiteosd::CinemaPatch,2> composedFanart;
        std::string channel;
        int duration = -1;
    };
    suiteosd::HomeLru<std::string,FocusPreview> focusPreviews;
    std::string focusKey;
    std::chrono::steady_clock::time_point focusSince{};
    bool focusPending=false;
    time_t focusClockMinute=-1;
    std::string notice;
    bool recordingDetails = false;
    bool homeProgrammeDetails = false;
    bool homeTimerConfirm = false;
    std::string homeTimerNotice;
    int width = 1600;
    int height = 900;
    int osdOriginX=0,osdOriginY=0;

    using PerfClock=std::chrono::steady_clock;
    const PerfClock::time_point openedAt=PerfClock::now();
    PerfClock::time_point keyAt{};
    const unsigned long long startHits=nativeHome.hits,startMisses=nativeHome.misses,startScraper=nativeHome.scraperCalls,startScraperUs=nativeHome.scraperUs,startCompleted=nativeHome.completed;
    const bool warmOpen=nativeHome.artwork.size()!=0;
    unsigned long long fullDraws=0,flushes=0,flushUs=0,keySamples=0,keyUs=0,keyMaxUs=0;
    long long firstFrameUs=-1;
    bool keyPending=false;
    void flushHomeOsd(){
        const auto began=PerfClock::now();osd->Flush();
        const auto now=PerfClock::now();
        ++flushes;flushUs+=std::chrono::duration_cast<std::chrono::microseconds>(now-began).count();
        if(firstFrameUs<0)firstFrameUs=std::chrono::duration_cast<std::chrono::microseconds>(now-openedAt).count();
        if(keyPending){
            const auto us=static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::microseconds>(now-keyAt).count());
            ++keySamples;keyUs+=us;keyMaxUs=std::max(keyMaxUs,us);keyPending=false;
        }
    }
    int X(int px) const { return px * width / 1600; }
    int Y(int px) const { return px * height / 900; }

    void fill(int left, int top, int right, int bottom, tColor color)
    {
        int x0 = std::max(0, X(left));
        int y0 = std::max(0, Y(top));
        int x1 = std::min(width - 1, suiteosd::InclusiveOsdEnd(right,1600,width));
        int y1 = std::min(height - 1, suiteosd::InclusiveOsdEnd(bottom,900,height));
        if (osd && x1 >= x0 && y1 >= y0) {
            if(railDrawPixmap)
                railDrawPixmap->DrawRectangle(cRect(x0+railDrawX,y0+railDrawY,x1-x0+1,y1-y0+1),color);
            else osd->DrawRectangle(x0, y0, x1, y1, color);
        }
    }

    void print(int x, int y, const std::string &s, const cFont *f,
               tColor fg = kText, int maxWidth = 0)
    {
        if (osd && f && !s.empty() && y >= 0 && y < 885) {
            const int w=maxWidth>0?X(maxWidth):0;
            if(railDrawPixmap)
                railDrawPixmap->DrawText(cPoint(X(x)+railDrawX,Y(y)+railDrawY),s.c_str(),fg,clrTransparent,f,w);
            else osd->DrawText(X(x),Y(y),s.c_str(),fg,clrTransparent,f,w);
        }
    }

    void panel(int x, int y, int w, int h, bool focused = false, tColor bg = kCard)
    {
        if (focused)
            fill(x - 3, y - 3, x + w + 3, y + h + 3, kFocus);
        fill(x, y, x + w, y + h, bg);
    }

    int count(int section) const
    {
        switch (section) {
        case 0: return int(data.now.size());
        case 1: return int(data.now.size());
        case 2: return int(data.next.size());
        case suiteosd::Navigation::kNew:
            if(homeRails.find(section)==homeRails.end()||!homeRails.at(section).status.ok)
                return int(data.recordings.size());
            break;
        }
        auto it=homeRails.find(section);
        return it==homeRails.end()?1:std::max(1,int(it->second.page.cards.size())+
            (it->second.page.hasMore?1:0));
    }

    const suiteosd::Artwork &channelLogo(const suiteosd::TimelineChannel &channel,int w,int h)
    {
        const std::string key = channel.name + "\n" + channel.shortName + ":" + std::to_string(w) + ":" + std::to_string(h);
        auto it = channelLogos.find(key);
        if (it != channelLogos.end()) return it->second;
        if (channelLogos.size() >= 48) channelLogos.clear();
        const std::string path = suiteosd::FindChannelLogo(channel.name, channel.shortName);
        auto art = suiteosd::LoadArtwork(path, std::max(1,X(w)), std::max(1,Y(h)));
        suiteosd::OpaqueArtwork(art, suiteosd::webLogo);
        return channelLogos.emplace(key, std::move(art)).first->second;
    }

    bool drawChannelLogo(const suiteosd::TimelineChannel &channel,
                         int x, int y, int w, int h)
    {
        const suiteosd::Artwork &art = channelLogo(channel,std::max(1,w-10),std::max(1,h-8));
        if (!art.valid() || !osd) return false;
        cImage image(cSize(art.width, art.height), art.argb.data());
        fill(x, y, x + w - 1, y + h - 1, suiteosd::webLogo);
        const int px = X(x) + (X(w) - art.width) / 2;
        const int py = Y(y) + (Y(h) - art.height) / 2;
        osd->DrawImage(cPoint(px, py), image);
        return true;
    }

    // Suite-Webfrontend EPG: dark rounded channel labels, proportional
    // programme tiles, anchored time-axis ticks, orange Jetzt marker.
    // The wide Web timeline is zoomed to a four-hour television viewport.
    static const char *ViewLabel(suiteosd::Timeline::View view)
    {
        switch (view) {
        case suiteosd::Timeline::View::Horizontal: return "Zeit horizontal";
        case suiteosd::Timeline::View::Vertical: return "Zeit vertikal";
        case suiteosd::Timeline::View::Live: return "Live-Liste";
        case suiteosd::Timeline::View::Now: return "Läuft jetzt";
        case suiteosd::Timeline::View::Next: return "Als nächstes";
        }
        return "EPG";
    }

    void drawTimelineHeader()
    {
        fill(0, 0, 1600, 77, kSurface);
        print(33, 24, "VDR Suite", fontLarge, kText, 226);
        print(276, 24, "EPG Zeitleiste", fontLarge);
        print(1250, 25, "Jetzt " + formatTime(time(nullptr)), fontBody, kMuted);
        panel(149, 88, 1418, 96, false, kSurface);
        print(168, 96, std::string(ViewLabel(timeline.view)) +
              "  ·  Kanäle " +
              (timeline.channels.empty() ? "0" : std::to_string(timeline.firstChannel + 1)) +
              "–" + std::to_string(std::min(int(timeline.channels.size()),
                                            timeline.firstChannel + suiteosd::Timeline::kRows)) +
              " / " + std::to_string(timeline.channels.size()) +
              "   ·   Gruppe: " + timeline.GroupLabel(),
              fontBody, kText, 1200);
        const time_t end = timeline.visibleStart + suiteosd::Timeline::kWindowSeconds;
        print(168, 143, formatTime(timeline.visibleStart) + " – " +
              formatTime(end) + (timeline.dayPage ? "  ·  Nächste 24h" : "  ·  Aktuelle 24h"),
              fontSmall, kFocus);
        print(1065, 145, "0: Gruppe  ·  Blau: Ansicht", fontSmall, kMuted);
    }

    int timelineX(time_t stamp, int trackX, int trackWidth) const
    {
        return trackX + int(double(stamp - timeline.visibleStart) *
                 trackWidth / suiteosd::Timeline::kWindowSeconds);
    }

    void drawTimelineHorizontal()
    {
        const int channelX = 151;
        const int trackX = 393;
        const int trackWidth = 1171;
        const int top = 190;
        const int rowHeight = 76;
        const int rowGap = 5;
        panel(channelX, top, 222, 48, false, kSurface);
        print(channelX + 14, top + 10, "KANAL", fontBody, kFocus);
        panel(trackX, top, trackWidth, 48, false, kSurface);
        const time_t end = timeline.visibleStart + suiteosd::Timeline::kWindowSeconds;
        const time_t firstTick = ((timeline.visibleStart + 1799) / 1800) * 1800;
        for (time_t tick = firstTick; tick < end; tick += 1800) {
            const int x = timelineX(tick, trackX, trackWidth);
            fill(x, top + 34, x + 1, 825, kLine);
            print(x + 5, top + 10, formatTime(tick), fontSmall, kText, 86);
        }

        for (int n = 0; n < suiteosd::Timeline::kRows; ++n) {
            const int index = timeline.firstChannel + n;
            if (index >= int(timeline.channels.size()))
                break;
            const auto &channel = timeline.channels[index];
            const int y = top + 55 + n * (rowHeight + rowGap);
            const bool focusedChannel = index == timeline.channel;
            panel(channelX, y, 222, rowHeight, focusedChannel, kSurface);
            drawChannelLogo(channel, channelX + 10, y + 13, 53, 39);
            print(channelX + 77, y + 9, std::to_string(channel.number),
                  fontSmall, kFocus, 50);
            print(channelX + 76, y + 38, channel.name, fontBody, kText, 135);
            panel(trackX, y, trackWidth, rowHeight, false, kBackground);
            int shown = 0;
            for (int eIndex = 0; eIndex < int(channel.events.size()); ++eIndex) {
                const auto &e = channel.events[eIndex];
                double left = 0, span = 0;
                if (!timeline.Geometry(e, left, span))
                    continue;
                const int x = trackX + int(std::round(left * trackWidth));
                const int w = std::min(trackWidth - (x - trackX),
                                       std::max(3, int(std::round(span * trackWidth)) - 5));
                if (w <= 0) continue;
                const bool selected = focusedChannel && timeline.event == eIndex;
                panel(x + 2, y + 8, w - 3, rowHeight - 16, selected,
                      selected ? kHighlight : kCardAlt);
                if (w >= 65)
                    print(x + 12, y + 17, formatTime(e.start), fontSmall,
                          selected ? kText : kMuted, w - 25);
                if (w >= 85)
                    print(x + 12, y + 41, e.title, fontBody, kText, w - 25);
                ++shown;
            }
            if (!shown)
                print(trackX + 17, y + 29, "Kein EPG im sichtbaren Zeitfenster",
                      fontSmall, kMuted);
        }
        const time_t current = time(nullptr);
        if (current >= timeline.visibleStart && current < end) {
            const int x = timelineX(current, trackX, trackWidth);
            fill(x, top + 33, x + 2, 824, 0xFFF97316);
            print(x + 7, top - 4, "JETZT", fontSmall, 0xFFF97316, 83);
        }
    }

    void drawTimelineVertical()
    {
        // Same alternative vertical-time mode as Webfrontend: channels at
        // the top, time running downward. TV can show five channel columns.
        const int startX = 300;
        const int top = 259;
        const int trackW = 252;
        const int trackH = 538;
        const int first = std::max(0, timeline.channel - 4);
        panel(149, 205, 1420, 608, false, kBackground);
        print(161, 231, "ZEIT", fontSmall, kFocus);
        for (int i = 0; i < 5 && first + i < int(timeline.channels.size()); ++i) {
            const int x = startX + i * trackW;
            const int index = first + i;
            const auto &channel = timeline.channels[index];
            panel(x, 213, trackW - 5, 40, index == timeline.channel, kSurface);
            drawChannelLogo(channel, x + 7, 218, 43, 28);
            print(x + 57, 224, std::to_string(channel.number) + "  " + channel.name,
                  fontBody, kText, trackW - 66);
            panel(x, top, trackW - 5, trackH, false, kBackground);
            for (int j = 0; j < int(channel.events.size()); ++j) {
                const auto &e = channel.events[j];
                double rel = 0, span = 0;
                if (!timeline.Geometry(e, rel, span)) continue;
                const int y = top + int(rel * trackH);
                const int h = std::min(trackH - (y - top),
                    std::max(3, int(span * trackH) - 4));
                panel(x + 3, y + 2, trackW - 12, std::max(1, h - 3),
                      index == timeline.channel && timeline.event == j,
                      kCardAlt);
                if (h >= 30)
                    print(x + 13, y + 8, e.title, fontSmall, kText, trackW - 35);
            }
        }
        for (time_t tick = ((timeline.visibleStart + 1799) / 1800) * 1800;
             tick < timeline.visibleStart + suiteosd::Timeline::kWindowSeconds;
             tick += 1800) {
            const int y = top + int(double(tick - timeline.visibleStart) * trackH /
                                   suiteosd::Timeline::kWindowSeconds);
            fill(281, y, 1550, y + 1, kLine);
            print(160, y - 9, formatTime(tick), fontSmall);
        }
        const time_t current = time(nullptr);
        if (current >= timeline.visibleStart &&
            current < timeline.visibleStart + suiteosd::Timeline::kWindowSeconds) {
            const int y = top + int(double(current - timeline.visibleStart) * trackH /
                                   suiteosd::Timeline::kWindowSeconds);
            fill(282, y, 1555, y + 2, 0xFFF97316);
            print(159, y + 6, "JETZT", fontSmall, 0xFFF97316);
        }
    }

    int timelineListEventIndex(const suiteosd::TimelineChannel &row) const
    {
        int present = -1;
        int following = -1;
        for (int i = 0; i < int(row.events.size()); ++i) {
            const auto &e = row.events[i];
            if (e.start <= timeline.now && e.end > timeline.now) {
                present = i;
                if (i + 1 < int(row.events.size()))
                    following = i + 1;
                break;
            }
            if (e.start > timeline.now) {
                following = i;
                break;
            }
        }
        if (timeline.view == suiteosd::Timeline::View::Next)
            return following;
        return present;
    }

    void drawTimelineList()
    {
        const int top = 211;
        for (int n = 0; n < suiteosd::Timeline::kRows; ++n) {
            const int index = timeline.firstChannel + n;
            if (index >= int(timeline.channels.size())) break;
            const auto &ch = timeline.channels[index];
            const int y = top + n * 87;
            const bool selected = index == timeline.channel;
            panel(153, y, 1409, 77, selected, kSurface);
            drawChannelLogo(ch, 172, y + 18, 53, 39);
            print(232, y + 10, std::to_string(ch.number) + "   " + ch.name,
                  fontBody, kFocus, 180);
            const int eventIndex = timelineListEventIndex(ch);
            if (eventIndex < 0) {
                print(452, y + 17, "Keine Programminformation", fontBody, kMuted);
                continue;
            }
            const auto &e = ch.events[eventIndex];
            print(451, y + 13, e.title, fontBody, kText, 800);
            print(451, y + 47, formatTime(e.start) + "–" + formatTime(e.end) +
                  (e.subtitle.empty() ? "" : "  ·  " + e.subtitle),
                  fontSmall, kMuted, 900);
            if (timeline.view == suiteosd::Timeline::View::Live ||
                timeline.view == suiteosd::Timeline::View::Now) {
                const int progress = e.end > e.start ?
                    std::clamp(int(100 * (timeline.now - e.start) / (e.end - e.start)), 0, 100) : 0;
                fill(1302, y + 63, 1533, y + 67, kLine);
                fill(1302, y + 63, 1302 + progress * 231 / 100, y + 67, kFocus);
            }
        }
    }

    const suiteosd::TimelineEvent *timelineSelectedEvent() const
    {
        if (timeline.view == suiteosd::Timeline::View::Horizontal ||
            timeline.view == suiteosd::Timeline::View::Vertical)
            return timeline.Selected();
        if (timeline.channel < 0 || timeline.channel >= int(timeline.channels.size()))
            return nullptr;
        const auto &row = timeline.channels[timeline.channel];
        const int index = timelineListEventIndex(row);
        return index >= 0 ? &row.events[index] : nullptr;
    }

    const suiteosd::Artwork &selectedEventArtwork()
    {
        static const suiteosd::Artwork empty;
        if (timeline.channel < 0 || timeline.channel >= int(timeline.channels.size()))
            return empty;
        const suiteosd::TimelineEvent *selected = timelineSelectedEvent();
        if (!selected) return empty;

        const int number = timeline.channels[timeline.channel].number;
        const std::string key = std::to_string(number) + ":" +
            std::to_string(selected->id) + ":" + std::to_string(selected->start);
        auto it = timelineArtwork.find(key);
        if (it != timelineArtwork.end()) return it->second;
        if (timelineArtwork.size() >= 12) timelineArtwork.clear();

        std::string imagePath;
        cPlugin *scraper = cPluginManager::GetPlugin("tvscraper");
        if (scraper) {
            // TVScraper borrows cEvent. Keep VDR schedule/channel locks only
            // while calling its service and copying the returned image path.
            LOCK_CHANNELS_READ;
            LOCK_SCHEDULES_READ;
            const cChannel *channel = Channels ? Channels->GetByNumber(number) : nullptr;
            const cSchedule *schedule = channel && Schedules ?
                Schedules->GetSchedule(channel) : nullptr;
            const cEvent *event = schedule ? schedule->GetEventById(selected->id) : nullptr;
            if (event && event->StartTime() == selected->start) {
                cGetScraperVideo request(event, nullptr);
                if (scraper->Service("GetScraperVideo", &request) && request.m_scraperVideo) {
                    cTvMedia picture = request.m_scraperVideo->getImage(
                        cImageLevels(eImageLevel::episodeMovie,
                                     eImageLevel::seasonMovie,
                                     eImageLevel::tvShowCollection),
                        cOrientations(eOrientation::landscape,
                                      eOrientation::banner,
                                      eOrientation::portrait), true);
                    imagePath = picture.path;
                }
            }
        }
        // Decode only the chosen EPG detail, without holding VDR locks.
        return timelineArtwork.emplace(key,
            suiteosd::LoadArtwork(imagePath, X(310), Y(210))).first->second;
    }

    std::string createSelectedVdrTimer()
    {
        const suiteosd::TimelineEvent *selected = timelineSelectedEvent();
        if (!selected || timeline.channel < 0 ||
            timeline.channel >= int(timeline.channels.size()))
            return "Keine Sendung ausgewählt";
        if (selected->end <= time(nullptr))
            return "Diese Sendung ist bereits beendet";

        const int channelNumber = timeline.channels[timeline.channel].number;
        const tEventID eventID = selected->id;
        const time_t eventStart = selected->start;
        return createNativeTimer(channelNumber,eventID,eventStart);
    }

    std::string createNativeTimer(int channelNumber,tEventID eventID,time_t eventStart)
    {
        // Match the lock order and timer life cycle of VDR's own
        // cMenuSchedule::Record(). Resolve the original event again under
        // its lock, so an outdated EPG snapshot never writes a wrong timer.
        LOCK_TIMERS_WRITE;
        LOCK_CHANNELS_READ;
        LOCK_SCHEDULES_READ;
        const cChannel *channel = Channels ?
            Channels->GetByNumber(channelNumber) : nullptr;
        const cSchedule *schedule = channel && Schedules ?
            Schedules->GetSchedule(channel) : nullptr;
        const cEvent *event = schedule ? schedule->GetEventById(eventID) : nullptr;
        if (!event || event->StartTime() != eventStart ||
            event->EndTime() <= time(nullptr))
            return "Die Sendung wurde geändert. Bitte EPG neu öffnen";

        eTimerMatch match = tmNone;
        if (Timers->GetMatch(event, &match) && match == tmFull)
            return "Ein Timer für diese Sendung ist bereits vorhanden";

        cTimer *timer = new cTimer(event);
        if (Setup.SVDRPPeering && *Setup.SVDRPDefaultHost)
            timer->SetRemote(Setup.SVDRPDefaultHost);
        if (Timers->GetTimer(timer)) {
            delete timer;
            return "Ein entsprechender Timer ist bereits vorhanden";
        }

        Timers->SetExplicitModify();
        Timers->Add(timer);
        Timers->SetModified();
        cString failure;
        if (!HandleRemoteTimerModifications(timer, nullptr, &failure)) {
            Timers->Del(timer);
            Timers->SetModified();
            return *failure ? std::string(*failure) : "VDR konnte den Timer nicht erstellen";
        }
        if (timer->Remote())
            Timers->SetSyncStateKey(StateKeySVDRPRemoteTimersPoll);
        // The VDR list is authoritative; GetTimer() verifies local readback.
        return Timers->GetTimer(timer) ? "Aufnahme programmiert" :
                                       "Timer nicht bestätigt";
    }

    void drawTimelineDetails()
    {
        if (!timelineDetails) return;
        const auto *e = timelineSelectedEvent();
        if (!e || timeline.channel >= int(timeline.channels.size()))
            return;
        panel(255, 157, 1102, 560, true, kSurface);
        print(292, 181, e->title, fontTitle, kText, 1005);
        print(293, 247, timeline.channels[timeline.channel].name + " · " +
              formatTime(e->start) + " – " + formatTime(e->end),
              fontBody, kFocus, 1010);
        if (!e->subtitle.empty())
            print(293, 296, e->subtitle, fontBody, kText, 1000);
        const suiteosd::Artwork &art = selectedEventArtwork();
        const bool hasImage = art.valid();
        if (hasImage && osd) {
            fill(998, 339, 1320, 566, kCardAlt);
            cImage image(cSize(art.width, art.height), art.argb.data());
            const int px = X(1005) + (X(307) - art.width) / 2;
            const int py = Y(348) + (Y(204) - art.height) / 2;
            osd->DrawImage(cPoint(px, py), image);
        }
        std::string description = e->description.empty() ?
            "Keine Beschreibung vorhanden." : e->description;
        std::replace(description.begin(), description.end(), '\n', ' ');
        std::istringstream words(description);
        std::string line, word;
        const int textWidth = hasImage ? 670 : 1000;
        int y = 347;
        while (words >> word && y < 635) {
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && fontBody && fontBody->Width(candidate.c_str()) > X(textWidth)) {
                print(292, y, line, fontBody, kText, textWidth + 10);
                line = word;
                y += 36;
            } else
                line = candidate;
        }
        if (!line.empty() && y < 660)
            print(292, y, line, fontBody, kText, textWidth + 10);
        if (timelineTimerConfirm)
            print(294, 640, "Timer erstellen? OK: Bestätigen  ·  Zurück: Abbrechen",
                  fontBody, kFocus, 1000);
        else
            print(294, 640, "Rot: Aufnahme programmieren  ·  Info/Zurück: Schließen",
                  fontSmall, kFocus, 1000);
        if (!timelineTimerNotice.empty())
            print(294, 681, timelineTimerNotice, fontSmall, kFocus, 1000);
    }

    void drawTimeline()
    {
        fill(0, 0, 1599, 899, kBackground);
        drawTimelineHeader();
        if (timeline.view == suiteosd::Timeline::View::Horizontal)
            drawTimelineHorizontal();
        else if (timeline.view == suiteosd::Timeline::View::Vertical)
            drawTimelineVertical();
        else
            drawTimelineList();
        fill(16, 843, 1584, 894, kSurface);
        print(153, 851, "Pfeile: Sender / Sendung", fontSmall);
        print(536, 851, "Rot: Jetzt", fontSmall);
        print(749, 851, "Grün/Gelb: 24h", fontSmall);
        print(1025, 851, "Blau: Ansicht", fontSmall);
        print(1341, 851, "OK: Details", fontSmall);
        drawTimelineDetails();
        drawSidebar();
    }

    void openTimeline()
    {
        photosActive = false;
        photoThumbnails.clear();

        settingsActive = false;
        if (recordingsActive) closeRecordingBrowser();
        timeline = ReadNativeTimeline();
        timelineActive = true;
        timelineDetails = false;
        timelineTimerConfirm = false;
        timelineTimerNotice.clear();
        timelineArtwork.clear();
        channelLogos.clear();
        notice.clear();
    }

    eOSState timelineKey(eKeys base)
    {
        if (timelineDetails) {
            if (timelineTimerConfirm) {
                if (base == kOk) {
                    timelineTimerNotice = createSelectedVdrTimer();
                    timelineTimerConfirm = false;
                } else if (base == kBack || base == kInfo) {
                    timelineTimerConfirm = false;
                }
            } else if (base == kRed || base == kRecord) {
                timelineTimerConfirm = true;
                timelineTimerNotice.clear();
            } else if (base == kBack || base == kInfo || base == kOk) {
                timelineDetails = false;
                timelineTimerNotice.clear();
            } else {
                return osContinue;
            }
            draw();
            return osContinue;
        }
        switch (base) {
        case kBack:
            timelineActive = false;
            nav.sideFocused = false;
            nav.section = 0;
            draw();
            return osContinue;
        case kRed:
            timeline.JumpNow();
            break;
        case kGreen:
            timeline.SetPage(0);
            break;
        case kYellow:
            timeline.SetPage(1);
            if (timeline.view == suiteosd::Timeline::View::Now ||
                timeline.view == suiteosd::Timeline::View::Next ||
                timeline.view == suiteosd::Timeline::View::Live)
                timeline.view = suiteosd::Timeline::View::Horizontal;
            break;
        case kBlue:
            timeline.NextView();
            break;
        case k0:
            timeline.CycleGroup();
            break;
        case kUp:
            if (timeline.view == suiteosd::Timeline::View::Vertical)
                timeline.MoveEvent(-1);
            else
                timeline.MoveChannel(-1);
            break;
        case kDown:
            if (timeline.view == suiteosd::Timeline::View::Vertical)
                timeline.MoveEvent(1);
            else
                timeline.MoveChannel(1);
            break;
        case kLeft:
            if (timeline.view == suiteosd::Timeline::View::Vertical)
                timeline.MoveChannel(-1);
            else
                timeline.MoveEvent(-1);
            break;
        case kRight:
            if (timeline.view == suiteosd::Timeline::View::Vertical)
                timeline.MoveChannel(1);
            else
                timeline.MoveEvent(1);
            break;
        case kInfo:
        case kOk:
            if (timelineSelectedEvent()) {
                timelineDetails = true;
                timelineTimerConfirm = false;
                timelineTimerNotice.clear();
            }
            break;
        default: return osContinue;
        }
        draw();
        return osContinue;
    }

    eOSState playBrowserRecording()
    {
        const Recording *rec = browserSelectedRecording();
        if (!rec || rec->fileName.empty()) {
            browserNotice = "Bitte eine Aufnahme auswählen";
            return osContinue;
        }
        restoreLivePreview(); // Fullscreen replay owns the video plane
        cReplayControl::SetRecording(rec->fileName.c_str());
        cControl::Launch(new cSuiteReplayControl());
        return osEnd;
    }

    eOSState deleteBrowserRecording()
    {
        const Recording *rec = browserSelectedRecording();
        if (!rec) return osContinue;
        const std::string fileName = rec->fileName;
        // VDR's existing recordings menu owns the more complex "currently
        // recording", remote timer and background editing workflows. Hand
        // those cases to it rather than implement a conflicting deletion path.
        if (cRecordControls::GetRecordControl(fileName.c_str()) ||
            RecordingsHandler.GetUsage(fileName.c_str())) {
            browserNotice = "Laufende/Bearbeitete Aufnahme: VDR-Verwaltung öffnen";
            if (cRemote::Put(kRecordings, true)) {
                restoreLivePreview();
                return osEnd;
            }
            return osContinue;
        }
        RecordingsHandler.Del(fileName.c_str()); // never while holding list locks
        bool removed = false;
        {
            cStateKey writeKey, deletedKey;
            cRecordings *recordings = cRecordings::GetRecordingsWrite(writeKey);
            cRecordings *deleted = cRecordings::GetDeletedRecordingsWrite(deletedKey);
            if (recordings && deleted) {
                cRecording *recording = recordings->GetByName(fileName.c_str());
                if (recording && recording->Delete()) {
                    cReplayControl::ClearLastReplayed(fileName.c_str());
                    if (cReplayControl::NowReplaying() &&
                        std::strcmp(cReplayControl::NowReplaying(), fileName.c_str()) == 0)
                        cControl::Shutdown();
                    recordings->Del(recording, false);
                    deleted->Add(recording);
                    recordings->SetModified();
                    deleted->SetModified();
                    removed = true;
                }
            }
            deletedKey.Remove();
            writeKey.Remove();
        }
        if (removed) {
            cVideoDiskUsage::ForceCheck();
            cRecordings::TouchUpdate();
            recordingMeta.erase(fileName);
            nativeHome.cancelPending();
            homeRecordingTitles.erase(fileName);
            const auto prefix="rec:"+fileName+":";
            homeArtwork.eraseIf([&](const auto &key,const auto &art){
                if(key.rfind(prefix,0)!=0)return false;forgetComposite(art);return true;
            });
            ++homePosterRevision;
            recordingDetailImages.erase(fileName);
            recordingGenres.erase(fileName);
            refreshHomeSnapshot();
            refreshBrowser();
            browserNotice = "Aufnahme gelöscht";
        } else
            browserNotice = "VDR konnte die Aufnahme nicht löschen";
        return osContinue;
    }

    eOSState browserKey(eKeys base)
    {
        if (browserDeleteConfirm) {
            if (base == kBack || base == kYellow) {
                browserDeleteConfirm = false;
            } else if (base == kOk) {
                browserDeleteConfirm = false;
                const eOSState result = deleteBrowserRecording();
                if (result != osContinue) return result;
            } else return osContinue;
            draw();
            return osContinue;
        }
        if (browserDetails) {
            if (handleDetailCast(base, browserSelectedRecording())) return osContinue;
            if (base == kOk || base == kPlay || base == kRed) {
                const eOSState result = playBrowserRecording();
                if (result != osContinue) return result;
            } else if (base == kYellow) {
                browserDetails = false;
                browserDeleteConfirm = browserSelectedRecording() != nullptr;
            } else if (base == kBack || base == kInfo || base == kBlue) {
                browserDetails = false;
            } else return osContinue;
            draw();
            return osContinue;
        }
        if (nav.sideFocused) {
            if (base == kBack || base == kRight) {
                nav.sideFocused = false;
                draw();
                return osContinue;
            }
            if (base == kUp)
                nav.Move(suiteosd::Direction::Up, 1, 4);
            else if (base == kDown)
                nav.Move(suiteosd::Direction::Down, 1, 4);
            else if (base == kOk) {
                const int target = nav.sideIndex;
                nav.sideFocused = false;
                if (target == 3) {
                    recordingBrowser.tabsFocused = true;
                } else if (target == 0) {
                    closeRecordingBrowser();
                } else {
                    closeRecordingBrowser();
                    const eOSState result = action(suiteosd::Activation::Module);
                    if (result != osContinue) return result;
                }
            }
            draw();
            return osContinue;
        }

        if (base == kBack) {
            if (!recordingBrowser.Back())
                closeRecordingBrowser();
        } else if (base == kGreen || base == k0) {
            recordingBrowser.ToggleSort();
            browserNotice.clear();
        } else if (base == kInfo || base == kBlue) {
            if (browserSelectedRecording()) { detailCastNav.Close(); detailScroll=0; browserDetails = true; }
        } else if (base == kYellow) {
            if (browserSelectedRecording()) browserDeleteConfirm = true;
        } else if (base == kRed || base == kPlay) {
            const eOSState result = playBrowserRecording();
            if (result != osContinue) return result;
        } else if (recordingBrowser.tabsFocused) {
            if (base == kLeft) {
                if (recordingBrowser.view == suiteosd::RecordingBrowser::View::Folders) {
                    nav.sideFocused = true;
                    nav.sideIndex = 3;
                } else {
                    recordingBrowser.ChangeView(static_cast<suiteosd::RecordingBrowser::View>(
                        int(recordingBrowser.view) - 1));
                    refreshBrowser();
                }
            } else if (base == kRight) {
                if (recordingBrowser.view != suiteosd::RecordingBrowser::View::All) {
                    recordingBrowser.ChangeView(static_cast<suiteosd::RecordingBrowser::View>(
                        int(recordingBrowser.view) + 1));
                    refreshBrowser();
                }
            } else if (base == kDown || base == kOk)
                recordingBrowser.tabsFocused = false;
        } else {
            switch (base) {
            case kLeft: recordingBrowser.Move(-1, 0); break;
            case kRight: recordingBrowser.Move(1, 0); break;
            case kUp: recordingBrowser.Move(0, -1); break;
            case kDown: recordingBrowser.Move(0, 1); break;
            case kOk: {
                const std::string fileName = recordingBrowser.Open();
                if (!fileName.empty()) {
                    detailCastNav.Close();
                    detailScroll = 0;
                    browserDetails = true;
                }
                break;
            }
            default: return osContinue;
            }
        }
        draw();
        return osContinue;
    }

    void drawHeader()
    {
        print(104, 15, "VDR Suite", fontCinemaBrand, kText, 200);
        fill(303,20,304,51,0xFF59738Du);
        print(338, 24, "TV · Aufnahmen · Mediathek", fontBody, kText, 650);
        const auto now=time(nullptr);
        tm date{};char buf[64]={};
        if(localtime_r(&now,&date))strftime(buf,sizeof(buf),"%a, %d. %b.",&date);
        print(1320,24,buf,fontBody,kText,143);
        fill(1473,20,1474,51,0xFF59738Du);
        print(1495,19,formatTime(now),fontLarge,kText,102);
    }

    void drawHero(int scroll)
    {
        int y = 89 - scroll;
        if (y + 275 < 80 || y > 830)
            return;
        panel(154, y, 1000, 270, nav.section == 0 && !nav.sideFocused, kSurface);
        suiteosd::TimelineChannel logo; logo.name=data.live.channel;
        drawChannelLogo(logo,187,y+22,130,88);
        print(337, y + 23, "LIVE-TV  ·  KANAL " + std::to_string(data.live.channelNumber),
              fontSmall, kFocus);
        print(337, y + 61, data.live.channel.empty() ? "Kein aktiver Sender" : data.live.channel,
              fontTitle, kText, 482);
        // Match the approved Webfrontend Home: Jetzt and Als Nächstes
        // come from VDR's present/following events for the tuned channel.
        print(189, y + 130, "JETZT", fontSmall, kFocus);
        print(189, y + 161, data.live.title.empty() ? "EPG nicht verfügbar" : data.live.title,
              fontBody, kText, 302);
        print(189, y + 207, data.live.time, fontSmall, kMuted, 303);
        print(518, y + 130, "ALS NÄCHSTES", fontSmall, kFocus);
        print(518, y + 161,
              data.liveNext.title.empty() ? "EPG nicht verfügbar" : data.liveNext.title,
              fontBody, kText, 285);
        print(518, y + 207, data.liveNext.time, fontSmall, kMuted, 286);
        fill(189, y + 242, 805, y + 250, kLine);
        fill(189, y + 242, 189 + (616 * data.live.progress / 100), y + 250, kFocus);
        panel(830, y + 18, 272, 224, false, kCardAlt);
        print(845, y + 30, "ZUR SENDUNG", fontSmall, kFocus, 242);
        const std::string synopsis = data.live.description.empty() ?
            "Keine Beschreibung verfügbar." : data.live.description;
        std::istringstream words(synopsis);
        std::string line, word;
        int lineY = y + 65;
        while (words >> word && lineY <= y + 200) {
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && fontSmall && fontSmall->Width(candidate.c_str()) > X(242)) {
                print(845, lineY, line, fontSmall, kText, 242);
                lineY += 26;
                line = word;
            } else line = candidate;
        }
        if (lineY <= y + 200) print(845, lineY, line, fontSmall, kText, 242);

    }

    suiteosd::Artwork loadHomePoster(const std::string &path) {
        const int w=X(HL::posterW),h=Y(HL::posterH);
        return suiteosd::CoverHomePoster(
            suiteosd::LoadArtwork(path,std::min(1024,w*2),std::min(1024,h*2)),
            w,h,0,0xFF061626u);
    }
    size_t homePosterCapacity() const {
        const size_t bytes=size_t(std::max(1,X(HL::posterW)))*std::max(1,Y(HL::posterH))*4;
        return std::clamp<size_t>((32U*1024U*1024U)/bytes,1,192);
    }
    void forgetComposite(const suiteosd::Artwork &art){
        const auto id=reinterpret_cast<std::uintptr_t>(&art);
        cinematicComposites.eraseIf([&](const auto &key,const auto &){return key.second==id;});
    }
    void trimLocalHomeArtwork(){
        if(homeArtwork.size()>=homePosterCapacity())
            homeArtwork.evictOldest([&](const auto &,const auto &art){forgetComposite(art);});
    }
    const suiteosd::Artwork &programmePoster(const Programme &item)
    {
        return nativeHome.programme(item);
    }

    void homePicture(const suiteosd::Artwork &art,int x,int y,int w,int h)
    {
        if(!art.valid())return;
        cImage img(cSize(art.width,art.height),art.argb.data());
        osd->DrawImage(cPoint(X(x)+(X(w)-art.width)/2,Y(y)+(Y(h)-art.height)/2),img);
    }
    void homeCard(int x,int y,int w,int h,bool focused)
    {
        fill(x-1,y-1,x+w+1,y+h+1,focused?kFocus:kLine);
        fill(x,y,x+w,y+h,kCard);
    }
    const suiteosd::Artwork &cinematicLogo(const std::string &name)
    {
        auto it=cinematicLogos.find(name);
        if(it!=cinematicLogos.end())return it->second;
        if(cinematicLogos.size()>=128)cinematicLogos.evictOldest();
        return cinematicLogos.emplace(name,suiteosd::LoadArtwork(
            suiteosd::FindChannelLogo(name,""),std::max(1,X(49)),std::max(1,Y(30)))).first->second;
    }
    int cinematicSidebarWidth() const {return nav.sideFocused?272:112;}
    int homeCardX(int col) const {return HL::cardX(col,nav.sideFocused);}
    int homeLeft() const {return homeCardX(0);}
    const suiteosd::Artwork &cinematicBackground()
    {
        auto &bg=cinematicBackgrounds[nav.sideFocused?1:0];
        if(!bg.valid()||bg.width!=width||bg.height!=height){
            cinematicFrames.clear();cinematicComposites.clear();cinematicGlyphs.clear();
            bg=suiteosd::CinematicBackground(width,height,cinematicSidebarWidth());
        }
        return bg;
    }
    void drawCinemaImage(const suiteosd::Artwork &art,int x,int y)
    {
        if(!art.valid())return;
        cImage img(cSize(art.width,art.height),art.argb.data());
        if(railDrawPixmap)
            railDrawPixmap->DrawImage(cPoint(x+railDrawX,y+railDrawY),img);
        else osd->DrawImage(cPoint(x,y),img);
    }
    void paintHomeBackground()
    {
        drawCinemaImage(cinematicBackground(),0,0);
    }
    const suiteosd::CinemaPatch &cinemaFrame(int x,int y,int w,int h,int radius,suiteosd::CinemaStyle style)
    {
        const auto &bg=cinematicBackground();
        const std::array<int,7> key{{x,y,w,h,radius,int(style),nav.sideFocused?1:0}};
        auto it=cinematicFrames.find(key);
        if(it!=cinematicFrames.end())return it->second;
        if(cinematicFrames.size()>=160)cinematicFrames.evictOldest();
        return cinematicFrames.emplace(key,suiteosd::CinematicFrame(bg,X(x),Y(y),X(w),Y(h),
            std::max(1,X(radius)),style,std::max(.5,double(width)/1600),
            nav.sideFocused&&(style==suiteosd::CinemaStyle::Poster||style==suiteosd::CinemaStyle::Focus)?7:9)).first->second;
    }
    void cinemaPanel(int x,int y,int w,int h,int radius,suiteosd::CinemaStyle style)
    {
        const auto &f=cinemaFrame(x,y,w,h,radius,style);
        drawCinemaImage(f.image,f.x,f.y);
    }
    void portraitCard(int x,int y,bool focused,const suiteosd::Artwork *art=nullptr)
    {
        const int w=HL::cardWidth,h=HL::cardHeight,r=HL::radius;
        const auto &frame=cinemaFrame(x,y,w,h,r,focused?suiteosd::CinemaStyle::Focus:suiteosd::CinemaStyle::Poster);
        if(!art||!art->valid()){
            drawCinemaImage(frame.image,frame.x,frame.y);
            if(art) {
                // A deliberately neutral image placeholder, no invented poster or metadata.
                const int cx=x+w/2,cy=y+55;
                fill(cx-15,cy-18,cx+15,cy+18,0xFF324257u);
                fill(cx-11,cy-14,cx+11,cy+14,0xFF142335u);
                for(int j=-11;j<=11;j+=8){fill(cx-14,cy+j,cx-11,cy+j+2,kMuted);fill(cx+11,cy+j,cx+14,cy+j+2,kMuted);}
                print(x+8,cy+26,"Kein Poster",fontCinemaMeta,kMuted,w-16);
            }
            return;
        }
        const std::array<int,7> geom{{x,y,w,h,r,int(focused),int(nav.sideFocused)}};
        const auto key=std::make_pair(geom,reinterpret_cast<std::uintptr_t>(art));
        auto it=cinematicComposites.find(key);
        if(it==cinematicComposites.end()){
            if(cinematicComposites.size()>=80)cinematicComposites.evictOldest();
            auto patch=frame;
            suiteosd::CinematicArtwork(patch,*art,X(x)+(X(w)-art->width)/2,
                Y(y)+(Y(h)-art->height)/2,X(x),Y(y),X(w),Y(h),X(r),
                std::max(1.0,(focused?3.5:1.2)*width/1600.0));
            it=cinematicComposites.emplace(key,std::move(patch)).first;
        }
        drawCinemaImage(it->second.image,it->second.x,it->second.y);
    }
    void printFit(int x,int y,const std::string &value,const cFont *font,tColor color,int maxWidth)
    {
        if(!font||value.empty())return;
        print(x,y,suiteosd::EllipsizeUtf8(value,X(maxWidth),
            [font](const std::string &s){return font->Width(s.c_str());}),font,color,maxWidth);
    }
    void portraitTitle(int x,int top,const std::string &title,const std::string &subtitle,
                       const std::string &episode={}) {
        const auto *f=fontCinemaCard?fontCinemaCard:fontSmall;
        auto lines=suiteosd::HeroLines(title,f,X(HL::cardWidth-16));
        if(lines.size()==1&&!episode.empty()) {
            auto episodes=suiteosd::HeroLines(episode,f,X(HL::cardWidth-16));
            if(!episodes.empty())lines.push_back(episodes.front()+(episodes.size()>1?"…":""));
        }
        const int titleY=top+HL::cardHeight-(subtitle.empty()?(lines.size()>1?47:29):65);
        for(int i=0;i<2&&i<int(lines.size());++i)
            printFit(x+8,titleY+i*18,lines[i]+(i==1&&lines.size()>2?"…":""),f,kText,HL::cardWidth-16);
        if(!subtitle.empty())
            printFit(x+8,top+HL::cardHeight-28,subtitle,fontCinemaMeta?fontCinemaMeta:fontSmall,0xFFD8E4F2u,HL::cardWidth-16);
    }
    void cinematicHeading(int y,const std::string &heading)
    {
        print(homeLeft(),y,heading,fontLarge,kText,800);
        if(!fontLarge)return;
        const int x=homeLeft()+fontLarge->Width(heading.c_str())*1600/std::max(1,width)+15;
        for(int j=0;j<8;++j){fill(x+j,y+8+j,x+j+1,y+9+j,kText);
            fill(x+7-j,y+16+j,x+8-j,y+17+j,kText);}
    }
    void cinemaProgress(int x,int y,int w,int percent)
    {
        fill(x,y,x+w-1,y+4,0xFF567088u);
        const int done=w*std::clamp(percent,0,100)/100;
        if(done>0)fill(x,y,x+done-1,y+4,0xFF12E5F5u);
    }
    void drawPrograms(int section, int y, const char *heading, const std::vector<Programme> &items)
    {
        cinematicHeading(y,heading);
        for(int col=0;col<HL::visibleColumns(nav.sideFocused) && nav.firstCard[section]+col<int(items.size());++col){
            const int index=nav.firstCard[section]+col,x=homeCardX(col),top=y+HL::titleHeight;
            const auto &item=items[index];
            const bool focus=!nav.sideFocused&&nav.section==section&&nav.card[section]==index;
            const auto &poster=programmePoster(item);
            portraitCard(x,top,focus,&poster);
            if(!cinematicLogo(item.channel).valid())
                print(x+8,top+8,item.channel,fontSmall,kText,HL::cardWidth-16);
            portraitTitle(x,top,item.title,item.time);
            if(section==1)cinemaProgress(x+9,top+HL::cardHeight-9,HL::cardWidth-18,
                HL::progressPercent(item.start,item.end,time(nullptr)));
        }
        if(items.empty())print(homeLeft(),y+94,"Keine EPG-Daten verfügbar",fontBody,kMuted);
    }

    RecordingMeta loadRecordingMeta(const Recording &item)
    {
        RecordingMeta meta;
        std::string imagePath;
        cPlugin *scraper = cPluginManager::GetPlugin("tvscraper");
        if (!scraper || item.fileName.empty())
            return meta;

        // The TVScraper service borrows the cRecording pointer. Read and
        // copy every returned value while its VDR read lock is valid.
        {
            LOCK_RECORDINGS_READ;
            const cRecording *recording = Recordings ?
                Recordings->GetByName(item.fileName.c_str()) : nullptr;
            if (!recording)
                return meta;
            cGetScraperVideo request(nullptr, recording);
            if (!scraper->Service("GetScraperVideo", &request) || !request.m_scraperVideo)
                return meta;
            cScraperVideo *video = request.m_scraperVideo.get();
            video->getOverview(&meta.title, &meta.episode, &meta.release,
                               &meta.runtime, nullptr, nullptr, nullptr);
            meta.season = video->getSeasonNumber();
            meta.episodeNumber = video->getEpisodeNumber();
            video->getMovieOrTv(
                nullptr, nullptr, nullptr, &meta.overview,
                &meta.genres, nullptr, nullptr, nullptr, nullptr, nullptr,
                &meta.rating, nullptr, nullptr, nullptr, nullptr,
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            if (video->getVideoType() == tSeries) {
                std::string episodeOverview;
                int episodeRuntime = 0;
                if (video->getEpisode(nullptr, &episodeOverview, nullptr,
                                      nullptr, &episodeRuntime, nullptr, nullptr, nullptr)) {
                    if (!episodeOverview.empty()) meta.overview = std::move(episodeOverview);
                    if (episodeRuntime > 0) meta.runtime = episodeRuntime;
                }
            }
            cTvMedia image = video->getImage(
                cImageLevels(eImageLevel::episodeMovie,
                             eImageLevel::seasonMovie,
                             eImageLevel::tvShowCollection),
                cOrientations(eOrientation::landscape,
                              eOrientation::banner,
                              eOrientation::portrait),
                true);
            imagePath = image.path; // TVScraper owns the scraper cache files.
        }
        // Image disk I/O and decode must not hold the VDR recordings lock.
        if (!imagePath.empty())
            meta.artwork = suiteosd::LoadArtwork(imagePath, X(164), Y(84));
        return meta;
    }

    const RecordingMeta &metaFor(const Recording &item)
    {
        const auto cached = recordingMeta.find(item.fileName);
        if (cached != recordingMeta.end())
            return cached->second;
        // Limit thumbnail memory while scrolling large recording libraries.
        if (recordingMeta.size() >= 48)
            recordingMeta.clear();
        return recordingMeta.emplace(item.fileName, loadRecordingMeta(item)).first->second;
    }

    const Recording *findRecording(const std::string &fileName) const
    {
        for (const Recording &item : data.recordings)
            if (item.fileName == fileName)
                return &item;
        return nullptr;
    }

    const std::vector<std::string> &genresFor(const Recording &item)
    {
        const auto cached = recordingGenres.find(item.fileName);
        if (cached != recordingGenres.end())
            return cached->second;

        std::vector<std::string> genres;
        const auto art = recordingMeta.find(item.fileName);
        if (art != recordingMeta.end()) {
            genres = art->second.genres;
        } else if (cPlugin *scraper = cPluginManager::GetPlugin("tvscraper")) {
            // Genre is a native TVScraper attribute, not an HTTP metadata
            // endpoint. Copy it under VDR's recordings read lock.
            LOCK_RECORDINGS_READ;
            const cRecording *rec = Recordings ?
                Recordings->GetByName(item.fileName.c_str()) : nullptr;
            if (rec) {
                cGetScraperVideo request(nullptr, rec);
                if (scraper->Service("GetScraperVideo", &request) && request.m_scraperVideo) {
                    request.m_scraperVideo->getMovieOrTv(
                        nullptr, nullptr, nullptr, nullptr,
                        &genres, nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr, nullptr, nullptr);
                }
            }
        }
        return recordingGenres.emplace(item.fileName, std::move(genres)).first->second;
    }

    void refreshBrowser()
    {
        recordingBrowser.source.clear();
        recordingBrowser.source.reserve(data.recordings.size());
        for (const Recording &item : data.recordings) {
            suiteosd::BrowserRecording row;
            row.fileName = item.fileName;
            row.name = item.title;
            row.title = item.baseName;
            row.start = item.start;
            // Loading genre data is demand-driven (only when the Genre tab
            // is selected), never while opening Home or the Folder tab.
            if (recordingBrowser.view == suiteosd::RecordingBrowser::View::Genres)
                row.genres = genresFor(item);
            recordingBrowser.source.push_back(std::move(row));
        }
        recordingBrowser.Refresh();
    }

    void startLivePreview(bool home)
    {
        if (!gLivePreviewEnabled) { restoreLivePreview(); return; }
        cDevice *device = cDevice::PrimaryDevice();
        if (scaledDevice && scaledDevice==device && scaledForHome==home &&
            (!home||scaledHomeExpanded==nav.sideFocused)&&
            !(device->Replaying()&&!device->Transferring())) return;
        restoreLivePreview();
        // Live DVB transfer mode has an attached player too. Only exclude
        // real recording replay, not the transfer player used for live TV.
        if (!device || (device->Replaying() && !device->Transferring()))
            return;
        const int left = osdOriginX;
        const int top = osdOriginY;
        // Reuse the exact rectangle verified by the user in recordings.
        // 4:2:0 output providers may round odd screen coordinates down.
        // Align inward before negotiation, so normalization cannot make an
        // otherwise supported video rectangle escape the reserved slot.
        const int x=left+X(home?HL::videoX(nav.sideFocused):1197), y=top+Y(home?HL::videoY:151);
        const int w=X(home?HL::videoW:338), h=Y(home?HL::videoH:190);
        const auto aligned=suiteosd::AlignPreviewRect(x,y,w,h);
        if(aligned.w<2||aligned.h<2)return;
        const cRect requested(aligned.x,aligned.y,aligned.w,aligned.h);
        const cRect available = device->CanScaleVideo(requested);
        if (available.IsEmpty() ||
            available.X() < requested.X() || available.Y() < requested.Y() ||
            available.X() + available.Width() > requested.X() + requested.Width() ||
            available.Y() + available.Height() > requested.Y() + requested.Height())
            return; // never cover surrounding controls with the video plane
        scaledDevice = device;
        scaledForHome = home;
        scaledHomeExpanded=home&&nav.sideFocused;
        // The device uses screen coordinates, the OSD draws locally.
        scaledRect = cRect(available.X() - left, available.Y() - top,
                           available.Width(), available.Height());
        device->ScaleVideo(available);
    }

    void restoreLivePreview()
    {
        if (!scaledDevice) return;
        scaledDevice->ScaleVideo(cRect::Null);
        scaledDevice = nullptr;
        scaledRect = cRect::Null;
    }

    void openRecordingBrowser()
    {
        photosActive = false;
        photoThumbnails.clear();

        settingsActive = false;
        if (timelineActive) timelineActive = false;
        recordingBrowser.ChangeView(suiteosd::RecordingBrowser::View::Folders);
        recordingBrowser.tabsFocused = true;
        browserDetails = browserDeleteConfirm = false;
        browserNotice.clear();
        refreshHomeSnapshot();
        refreshBrowser();
        recordingsActive = true;
    }

    void closeRecordingBrowser()
    {
        restoreLivePreview();
        recordingsActive = false;
        browserDetails = browserDeleteConfirm = false;
        browserNotice.clear();
        nav.sideFocused = false;
        nav.section = 0;
    }

    std::string homeRecordingTitle(const Recording &rec,const std::string &fallback) const
    {
        const auto found=homeRecordingTitles.find(rec.fileName);
        return found==homeRecordingTitles.end()?fallback:suiteosd::RecordingDisplayTitle(found->second,fallback);
    }
    std::string homeRecordingEpisode(const Recording &rec) const
    {
        const auto found=homeRecordingTitles.find(rec.fileName);
        return found==homeRecordingTitles.end()?std::string{}:suiteosd::RecordingEpisodeLabel(found->second);
    }
    const suiteosd::Artwork &recordingPoster(const Recording &rec,int w=184,int h=240)
    {
        if(w==HL::posterW&&h==HL::posterH)return nativeHome.recording(rec);
        const std::string key=NativeHomeCache::recordingKey(rec,w,h);
        auto it=homeArtwork.find(key);if(it!=homeArtwork.end()){homeRecordingTitles.find(rec.fileName);return it->second;}
        trimLocalHomeArtwork();
        std::string path;
        suiteosd::DetailMetadata metadata;
        if(auto *scraper=cPluginManager::GetPlugin("tvscraper")){
            LOCK_RECORDINGS_READ;
            const auto *r=Recordings?Recordings->GetByName(rec.fileName.c_str()):nullptr;
            if(r){
                cGetScraperVideo req(nullptr,r);
                if(scraper->Service("GetScraperVideo",&req)&&req.m_scraperVideo){
                    auto *video=req.m_scraperVideo.get();
                    video->getOverview(&metadata.title,&metadata.episode,nullptr,nullptr,nullptr,nullptr,nullptr);
                    metadata.season=video->getSeasonNumber();metadata.episodeNumber=video->getEpisodeNumber();
                    path=video->getImage(cImageLevels(eImageLevel::episodeMovie,eImageLevel::seasonMovie,eImageLevel::tvShowCollection),
                        cOrientations(eOrientation::portrait,eOrientation::landscape),true).path;
                }
            }
        }
        if(homeRecordingTitles.size()>=512)homeRecordingTitles.evictOldest();
        homeRecordingTitles.insertOrAssign(rec.fileName,std::move(metadata));
        return homeArtwork.emplace(key,w==HL::posterW&&h==HL::posterH?loadHomePoster(path):suiteosd::LoadDetailArtwork(path,X(w),Y(h))).first->second;
    }
    void drawRecordings(int y)
    {
        cinematicHeading(y,"Neu aufgenommen");
        const int first=nav.firstCard[suiteosd::Navigation::kNew];
        for(int col=0;col<HL::visibleColumns(nav.sideFocused) && first+col<int(data.recordings.size());++col){
            const auto &item=data.recordings[first+col];
            const int x=homeCardX(col),top=y+HL::titleHeight;
            const auto &poster=recordingPoster(item,HL::posterW,HL::posterH);
            portraitCard(x,top,!nav.sideFocused&&nav.section==suiteosd::Navigation::kNew&&
                         nav.card[suiteosd::Navigation::kNew]==first+col,&poster);
            portraitTitle(x,top,homeRecordingTitle(item,item.baseName),{},homeRecordingEpisode(item));
        }
    }

    const RecordingDetailImage &detailImageFor(const Recording &rec)
    {
        auto found = recordingDetailImages.find(rec.fileName);
        if (found != recordingDetailImages.end()) return found->second;
        if (recordingDetailImages.size() >= 3) recordingDetailImages.clear();
        RecordingDetailImage image;
        // The VDR-owned cRecording and TVScraper service result are used only
        // under VDR's read lock. Decoding follows outside the lock.
        cPlugin *scraper = cPluginManager::GetPlugin("tvscraper");
        if (scraper && !rec.fileName.empty()) {
            {
                LOCK_RECORDINGS_READ;
                const cRecording *recording = Recordings ?
                    Recordings->GetByName(rec.fileName.c_str()) : nullptr;
                if (recording) {
                    cGetScraperVideo request(nullptr, recording);
                    if (scraper->Service("GetScraperVideo", &request) && request.m_scraperVideo) {
                        image.metadata = suiteosd::ReadScraperDetail(request.m_scraperVideo.get());
                    }
                }
            }
        }
        if (image.metadata.title.empty()) image.metadata.title = rec.baseName;
        if (!image.metadata.fanartPath.empty())
            image.fanart = suiteosd::LoadDetailArtwork(image.metadata.fanartPath, X(1050), Y(500), true);
        if (!image.metadata.posterPath.empty())
            image.poster = suiteosd::LoadDetailArtwork(image.metadata.posterPath, X(262), Y(402));
        return recordingDetailImages.emplace(rec.fileName, std::move(image)).first->second;
    }

    void drawRecordingDetailPage(const Recording &rec)
    {
        const auto &image = detailImageFor(rec);
        const auto &detail = image.metadata;
        fill(0,0,1599,899,kBackground);
        fill(0,0,1599,78,kSurface);
        print(35,22,"VDR Suite",fontLarge,kText,226);
        print(284,24,"AUFNAHMEN / DETAILS",fontBody,kMuted);
        print(1440,23,formatTime(time(nullptr)),fontLarge);
        suiteosd::DrawRecordingHero(osd,width,height,fontTitle,fontBody,fontSmall,
            detail,rec.baseName,recordingDate(rec.start),
            detail.overview.empty()?rec.description:detail.overview,
            image.fanart,image.poster,detailScroll,&detailCastNav,detailCastCache);
        fill(18,822,1580,892,kSurface);
        print(52,838,detailCastNav.active ?
              "OK: Person suchen · Links/Rechts: Auswahl" : "OK / Rot: Wiedergabe",
              fontBody,kFocus,810);
        print(900,838,detailCastNav.active ? "Hoch / Zurück: Details" :
              "Runter / Blau: Personen",fontBody,kText,640);
    }

    bool handleDetailCast(eKeys base, const Recording *rec)
    {
        if (!rec) { detailCastNav.Close(); return false; }
        if (detailCastNav.active) {
            if (base == kOk) {
                const auto &people = detailImageFor(*rec).metadata.people;
                if (detailCastNav.selected >= 0 && detailCastNav.selected < int(people.size())) {
                    const std::string name = people[detailCastNav.selected].name;
                    openSearch(name);
                }
            } else if (base == kBack || base == kInfo || base == kUp) detailCastNav.Close();
            else if (base == kLeft || base == kRight)
                detailCastNav.Move(base == kLeft ? -1 : 1, int(detailImageFor(*rec).metadata.people.size()));
            draw();
            return true;
        }
        if (base == kLeft || base == kRight) {
            const auto &meta=detailImageFor(*rec).metadata;
            const auto lines=suiteosd::HeroLines(meta.overview.empty()?rec->description:meta.overview,
                          fontBody,X(suiteosd::kHeroTextWidth));
            detailScroll=std::clamp(detailScroll+(base==kLeft?-1:1),0,
                          std::max(0,int(lines.size())-suiteosd::kHeroTextRows));
            draw(); return true;
        }
        if ((base == kDown || base == kBlue) && !detailImageFor(*rec).metadata.people.empty()) {
            detailCastNav.Open();
            draw();
            return true;
        }
        return false;
    }

    void drawRecordingDetails()
    {
        if (recordingDetails) {
            drawRecordingDetailPage(homeDetailRecording);
            if(homeResumeDetail&&!detailCastNav.active){
                fill(20,822,1580,892,kSurface);
                print(52,837,"OK / Rot: Fortsetzen · Grün: Von vorn",fontBody,kFocus,850);
                print(900,837,"Runter / Blau: Personen",fontBody,kText,640);
            }
            if(!notice.empty())print(55,785,notice,fontSmall,kFocus,1470);
        }
    }

    const Recording *browserSelectedRecording() const
    {
        const suiteosd::BrowserEntry *entry = recordingBrowser.Selected();
        return entry && entry->kind == suiteosd::BrowserEntry::Kind::Recording ?
               findRecording(entry->fileName) : nullptr;
    }

    void drawBrowserDetails()
    {
        const Recording *rec = browserSelectedRecording();
        if (rec && browserDetails) drawRecordingDetailPage(*rec);
    }

    void drawBrowserDeleteConfirm()
    {
        if (!browserDeleteConfirm) return;
        const Recording *rec = browserSelectedRecording();
        if (!rec) return;
        panel(370, 325, 859, 240, true, kSurface);
        print(409, 366, "Aufnahme löschen?", fontTitle, kText);
        print(409, 435, rec->baseName, fontBody, kMuted, 776);
        print(409, 512, "OK: Löschen   ·   Zurück: Abbrechen", fontBody, kFocus);
    }

    void drawLivePreviewPanel()
    {
        // Live-TV preview is VDR's existing primary video plane, not a
        // streamed thumbnail or another player.
        panel(1184, 105, 372, 667, false, kSurface);
        print(1207, 115, "LIVE-TV", fontBody, kFocus);
        if (scaledDevice && !scaledRect.IsEmpty()) {
            // Carve an actual transparent OSD hole above the scaled video.
            // This must happen AFTER the background was painted.
            osd->DrawRectangle(
                scaledRect.X(), scaledRect.Y(),
                scaledRect.X() + scaledRect.Width() - 1,
                scaledRect.Y() + scaledRect.Height() - 1, clrTransparent);
        } else {
            panel(1197, 151, 338, 190, false, kCard);
            suiteosd::TimelineChannel logoChannel;
            logoChannel.name = data.live.channel;
            drawChannelLogo(logoChannel, 1207, 175, 318, 86);
            print(1217, 273, "Live-TV läuft weiter", fontBody, kText, 300);
        }
        print(1202, 365, data.live.channel.empty() ? "Live-TV" : data.live.channel,
              fontBody, kText, 322);
        print(1202, 403, data.live.title, fontSmall, kMuted, 322);
    }

    void drawRecordingBrowser()
    {
        fill(0, 0, 1599, 899, kBackground);
        fill(0, 0, 1599, 76, kSurface);
        print(33, 23, "VDR Suite", fontLarge, kText, 226);
        print(276, 23, "Aufnahmen", fontLarge);
        print(1059, 23, std::to_string(data.recordings.size()) + " Aufnahmen", fontBody, kMuted);

        const std::array<const char *, 3> views = {{"Ordner", "Genre", "Alle"}};
        for (int i = 0; i < 3; ++i) {
            const int x = 157 + i * 173;
            const bool selected = int(recordingBrowser.view) == i;
            panel(x, 107, 157, 51,
                  recordingBrowser.tabsFocused && selected && !nav.sideFocused,
                  selected ? kHighlight : kSurface);
            print(x + 28, 120, views[i], fontBody, selected ? kText : kMuted);
        }
        const std::string sortText =
            recordingBrowser.sort == suiteosd::RecordingBrowser::Sort::Newest ?
            "Neueste zuerst" : "Nach Namen";
        print(794, 123, "Sortierung: " + sortText, fontSmall, kMuted, 335);

        std::string crumb = recordingBrowser.view ==
            suiteosd::RecordingBrowser::View::Folders ?
            recordingBrowser.folder : recordingBrowser.genre;
        std::replace(crumb.begin(), crumb.end(), '~', '/');
        if (crumb.empty())
            crumb = recordingBrowser.view == suiteosd::RecordingBrowser::View::Folders ?
                    "Alle Ordner" :
                    recordingBrowser.view == suiteosd::RecordingBrowser::View::Genres ?
                    "Alle Genres" : "Alle Aufnahmen";
        print(166, 185, crumb, fontBody, kFocus, 900);

        constexpr int columns = suiteosd::RecordingBrowser::kColumns;
        for (int i = 0; i < suiteosd::RecordingBrowser::kVisibleItems; ++i) {
            const int index = recordingBrowser.firstVisible + i;
            if (index >= int(recordingBrowser.entries.size())) break;
            const auto &entry = recordingBrowser.entries[index];
            const int x = 157 + (i % columns) * 247;
            const int y = 236 + (i / columns) * 291;
            const bool focus = !nav.sideFocused && !recordingBrowser.tabsFocused &&
                               index == recordingBrowser.selected;
            homeCard(x,y,224,276,focus);
            if (entry.kind != suiteosd::BrowserEntry::Kind::Recording) {
                print(x+16,y+76,entry.title,fontBody,kText,192);
                print(x+16,y+125,std::to_string(entry.count)+" Aufnahmen",fontSmall,kMuted,192);
                continue;
            }
            const Recording *rec = findRecording(entry.fileName);
            if (!rec) continue;
            const RecordingMeta &meta = metaFor(*rec);
            homePicture(recordingPoster(*rec,204,194),x+10,y+8,204,194);
            const auto lines=suiteosd::HeroLines(meta.title.empty()?entry.title:meta.title,fontBody,X(192));
            for(int row=0;row<2 && row<int(lines.size());++row)
                print(x+15,y+201+row*24,lines[row],fontBody,kText,192);
            print(x+15,y+251,meta.episode.empty()?recordingDate(rec->start):meta.episode,
                  fontSmall,kMuted,192);
        }
        if (recordingBrowser.entries.empty())
            print(178, 345, "Keine Aufnahmen in dieser Ansicht", fontBody, kMuted, 922);

        drawLivePreviewPanel();
        const Recording *selected = browserSelectedRecording();
        if (selected) {
            const RecordingMeta &meta = metaFor(*selected);
            print(1202, 466, "AUSGEWÄHLT", fontSmall, kFocus);
            print(1202, 498, meta.title.empty() ? selected->baseName : meta.title,
                  fontBody, kText, 320);
            print(1202, 544, selected->time, fontSmall, kMuted);
            if (!meta.genres.empty()) {
                std::string joined;
                for (const auto &name : meta.genres)
                    joined += (joined.empty() ? "" : ", ") + name;
                print(1202, 589, joined, fontSmall, kMuted, 323);
            }
        }
        fill(16, 835, 1581, 895, kSurface);
        print(155, 848, "Rot: Abspielen", fontSmall);
        print(397, 848, "Grün: Sortieren", fontSmall);
        print(650, 848, "Gelb: Löschen", fontSmall);
        print(885, 848, "OK / Blau: Info", fontSmall);
        print(1105, 848, "0: Sortierung  ·  Zurück: Ordner/Home", fontSmall);
        if (!browserNotice.empty()) {
            fill(159, 782, 1563, 823, kSurface);
            print(180, 791, browserNotice, fontSmall, kFocus, 1340);
        }
        drawSidebar();
        drawBrowserDetails();
        drawBrowserDeleteConfirm();
    }

    const Programme *selectedHomeProgramme() const
    {
        switch (nav.section) {
        case 0: return heroProgramme(0);
        case 1:
            return nav.card[1] >= 0 && nav.card[1] < int(data.now.size()) ?
                &data.now[nav.card[1]] : nullptr;
        case 2:
            return nav.card[2] >= 0 && nav.card[2] < int(data.next.size()) ?
                &data.next[nav.card[2]] : nullptr;
        default: return nullptr;
        }
    }

    const suiteosd::CoreHomeCard *focusedCoreCard() const {
        if(!homeCollections.empty()){
            const auto &c=homeCollections.back();
            return c.selected>=0&&c.selected<int(c.rail.page.cards.size())?
                &c.rail.page.cards[c.selected]:nullptr;
        }
        const auto it=homeRails.find(nav.section);
        if(it==homeRails.end()||!it->second.status.ok)return nullptr;
        const int index=nav.card[nav.section];
        return index>=0&&index<int(it->second.page.cards.size())?
            &it->second.page.cards[index]:nullptr;
    }
    void liveBadge(int x,int y,int w,int h,tColor color)
    {
        if(!osd||w<h)return;
        const int r=h/2;
        fill(x+r,y,x+w-r-1,y+h-1,color);
        osd->DrawEllipse(X(x),Y(y),X(x+h)-1,Y(y+h)-1,color);
        osd->DrawEllipse(X(x+w-h),Y(y),X(x+w)-1,Y(y+h)-1,color);
    }
    const Programme *heroProgramme(int offset) const {
        const int count=int(data.now.size());
        return count?&data.now[HL::channelIndex(nav.card[0],offset,count)]:nullptr;
    }
    void refreshHomeSnapshot(bool initial=false){
        const auto *selected=heroProgramme(0);
        const int number=!initial&&selected?selected->channelNumber:0;
        data=readVdrSnapshot();
        int index=0;
        for(int i=0;i<int(data.now.size());++i){
            if(data.now[i].channelNumber==data.live.channelNumber)index=i;
        }
        if(number)for(int i=0;i<int(data.now.size());++i)
            if(data.now[i].channelNumber==number){index=i;break;}
        nav.card[0]=index;nav.firstCard[0]=0;
    }
    bool homeHeroVisible() const {return homeCollections.empty()&&nav.firstVisibleSection==0;}
    void drawHomeHero(int y) {
        focusClockMinute=time(nullptr)/60;
        cinematicHeading(y,"Live-TV");
        const int top=y+HL::titleHeight;
        auto focused=[&](int i){return nav.section==0&&!nav.sideFocused&&i==1;};
        auto neighbor=[&](int i,int x,int w){
            cinemaPanel(x,top,w,HL::liveH,16,focused(i)?suiteosd::CinemaStyle::Focus:suiteosd::CinemaStyle::Panel);
            const auto *p=heroProgramme(i-1);
            if(!p)return;
            suiteosd::TimelineChannel channel;channel.name=p->channel;
            // Use the cached logo through the same draw target as the rail.
            const auto &logo=cinematicLogo(p->channel);
            if(logo.valid()){
                auto art=logo;suiteosd::OpaqueArtwork(art,0xFF05172Au);
                drawCinemaImage(art,X(x+w/2)-art.width/2,Y(top+42)-art.height/2);
            }
            printFit(x+10,top+76,p->channel,fontBody,kText,w-20);
            const auto lines=suiteosd::HeroLines(p->title,fontSmall,X(w-20));
            for(int j=0;j<2&&j<int(lines.size());++j)
                printFit(x+10,top+112+j*22,lines[j]+(j==1&&lines.size()>2?"…":""),fontSmall,kText,w-20);
            printFit(x+10,top+170,p->time,fontCinemaMeta,kMuted,w-20);
        };
        const bool expanded=nav.sideFocused;
        neighbor(0,homeLeft(),HL::neighborW(expanded));
        neighbor(2,HL::nextX(expanded),HL::neighborW(expanded));
        cinemaPanel(HL::heroX(expanded),top,HL::heroW(expanded),HL::liveH,16,
            focused(1)?suiteosd::CinemaStyle::Focus:suiteosd::CinemaStyle::Panel);
        const auto *selected=heroProgramme(0);
        if(!selected){printFit(HL::heroX(expanded)+12,top+55,"Keine Sender vorhanden",fontBody,kMuted,HL::heroW(expanded)-24);return;}
        const auto &current=*selected;
        const auto &next=nav.card[0]<int(data.next.size())?data.next[nav.card[0]]:current;
        const bool tuned=current.channelNumber==data.live.channelNumber;
        const int vx=HL::videoX(expanded),vy=top+8;
        fill(vx,vy,vx+HL::videoW-1,vy+HL::videoH-1,0xFF061626u);
        if(tuned&&!railDrawPixmap&&y==HL::top&&scaledDevice&&!scaledRect.IsEmpty())
            osd->DrawRectangle(scaledRect.X(),scaledRect.Y(),scaledRect.X()+scaledRect.Width()-1,
                scaledRect.Y()+scaledRect.Height()-1,clrTransparent);
        else {
            const auto &logo=cinematicLogo(current.channel);
            if(logo.valid()){
                auto art=logo;suiteosd::OpaqueArtwork(art,0xFF061626u);
                drawCinemaImage(art,X(vx+HL::videoW/2)-art.width/2,Y(vy+HL::videoH/2)-art.height/2);
            }
            if(!tuned)printFit(vx+12,vy+HL::videoH-28,"OK: Live ansehen",fontSmall,kText,HL::videoW-24);
        }
        const int tx=vx+HL::videoW+12,tw=HL::heroX(expanded)+HL::heroW(expanded)-tx-12;
        printFit(tx,top+9,current.channel,fontBody,kFocus,tw);
        printFit(tx,top+40,current.title,fontBody,kText,tw);
        printFit(tx,top+67,current.time,fontSmall,kMuted,tw);
        print(tx,top+103,"Als nächstes",fontSmall,kFocus,tw);
        const auto lines=suiteosd::HeroLines(next.title,fontSmall,X(tw));
        for(int i=0;i<2&&i<int(lines.size());++i)
            printFit(tx,top+128+i*21,lines[i]+(i==1&&lines.size()>2?"…":""),fontSmall,kText,tw);
        printFit(tx,top+174,next.time,fontCinemaMeta,kMuted,tw);
        cinemaProgress(vx,top+HL::liveH-15,HL::videoW,
            HL::progressPercent(current.start,current.end,time(nullptr)));
    }
    void drawHomeLivePanel() {if(homeHeroVisible())drawHomeHero(HL::top);}
    void drawHomeFocusPanel() {
        cinemaPanel(HL::right,HL::detailY,HL::rightWidth,HL::detailBottom-HL::detailY,16,suiteosd::CinemaStyle::Panel);
        const Programme *p=homeCollections.empty()?selectedHomeProgramme():nullptr;
        const auto *card=p?nullptr:focusedCoreCard();
        const Recording *rec=nullptr;
        const auto *loaded=card?publicHomePoster(card->recording):nullptr;
        if(card&&card->kind==suiteosd::CoreHomeCard::Kind::Recording&&card->recording.backendId=="default")
            rec=findRecording(loaded&&!loaded->nativeId.empty()?loaded->nativeId:card->recording.nativeId);
        if(!p&&!card&&homeCollections.empty()&&nav.section==suiteosd::Navigation::kNew){
            const int i=nav.card[nav.section];
            const auto it=homeRails.find(nav.section);
            if((it==homeRails.end()||!it->second.status.ok)&&i>=0&&i<int(data.recordings.size()))
                rec=&data.recordings[i];
        }
        std::string key,title,description,facts,genre;
        if(p){
            key="event:"+std::to_string(p->channelNumber)+":"+std::to_string(p->eventId)+":"+std::to_string(p->start);
            title=p->title;description=p->description;facts=p->channel;
        }else if(rec){
            key="recording:"+rec->fileName;title=rec->baseName;description=rec->description;
        }else if(card){
            key="card:"+card->key;title=loaded&&!loaded->detail.title.empty()?loaded->detail.title:card->title;facts=card->subtitle;
            if(loaded)description=loaded->detail.description;
        }
        if(key!=focusKey){focusKey=key;focusSince=std::chrono::steady_clock::now();focusPending=!key.empty()&&(p||rec);}
        auto cached=focusPreviews.find(key);
        if(cached!=focusPreviews.end())focusPending=false;
        if(focusPending&&std::chrono::steady_clock::now()-focusSince>=std::chrono::milliseconds(220)){
            FocusPreview preview;
            if(p&&p->start&&p->eventId){
                if(auto *scraper=cPluginManager::GetPlugin("tvscraper")){
                    LOCK_CHANNELS_READ;
                    LOCK_SCHEDULES_READ;
                    const auto *ch=Channels?Channels->GetByNumber(p->channelNumber):nullptr;
                    const auto *schedule=ch&&Schedules?Schedules->GetSchedule(ch):nullptr;
                    const auto *event=schedule?schedule->GetEventById(p->eventId):nullptr;
                    if(event&&event->StartTime()==p->start){
                        cGetScraperVideo req(event,nullptr);
                        if(scraper->Service("GetScraperVideo",&req)&&req.m_scraperVideo)
                            preview.metadata=suiteosd::ReadScraperDetail(req.m_scraperVideo.get());
                    }
                }
            }else if(rec){
                LOCK_RECORDINGS_READ;
                const auto *r=Recordings?Recordings->GetByName(rec->fileName.c_str()):nullptr;
                if(r){
                    preview.duration=r->LengthInSeconds();
                    if(r->Info()&&r->Info()->ChannelName())preview.channel=r->Info()->ChannelName();
                    if(auto *scraper=cPluginManager::GetPlugin("tvscraper")){
                        cGetScraperVideo req(nullptr,r);
                        if(scraper->Service("GetScraperVideo",&req)&&req.m_scraperVideo)
                            preview.metadata=suiteosd::ReadScraperDetail(req.m_scraperVideo.get());
                    }
                }
            }
            // Decode only the settled selection, outside native VDR read locks.
            preview.fanart=suiteosd::CoverHomePoster(
                suiteosd::LoadArtwork(preview.metadata.fanartPath,std::min(1024,X(HL::rightWidth)*2),std::min(1024,Y(250)*2)),
                X(HL::rightWidth-4),Y(210),0,0xFF0B2138u);
            if(focusPreviews.size()>=12)focusPreviews.evictOldest();
            cached=focusPreviews.emplace(key,std::move(preview)).first;
            focusPending=false;
        }
        if(cached!=focusPreviews.end()){
            const auto &v=cached->second;
            if(v.fanart.valid()){
                const auto &frame=cinemaFrame(HL::right,HL::detailY,HL::rightWidth,HL::detailBottom-HL::detailY,16,suiteosd::CinemaStyle::Panel);
                auto &patch=cached->second.composedFanart[nav.sideFocused?1:0];
                if(!patch.image.valid()){
                    patch=frame;
                    suiteosd::CinematicArtwork(patch,v.fanart,X(HL::right+2),Y(HL::detailY+2),
                        X(HL::right),Y(HL::detailY),X(HL::rightWidth),Y(HL::detailBottom-HL::detailY),X(16),std::max(1,X(2)));
                }
                drawCinemaImage(patch.image,patch.x,patch.y);
            }else print(HL::right+20,HL::detailY+92,"Keine Fanart verfügbar",fontSmall,kMuted,HL::rightWidth-40);
            if(!v.metadata.title.empty())title=v.metadata.title;
            if(!v.metadata.episode.empty()&&v.metadata.episode!=title)title+=" – "+v.metadata.episode;
            if(description.empty())description=v.metadata.overview;
            if(rec){
                facts=v.channel;
                const int minutes=v.duration>0?(v.duration+59)/60:v.metadata.runtime;
                if(minutes>0)facts+=(facts.empty()?"":" · ")+std::to_string(minutes)+" Min.";
            }
            if(!v.metadata.genres.empty())genre=v.metadata.genres.front();
            if(rec&&v.metadata.season>0){
                facts+=(facts.empty()?"":" · ")+std::string("Staffel ")+std::to_string(v.metadata.season);
                if(v.metadata.episodeNumber>0)facts+=", Folge "+std::to_string(v.metadata.episodeNumber);
            }
        }
        const int tx=HL::right+20,tw=HL::rightWidth-40;
        const auto titleLines=suiteosd::HeroLines(title.empty()?"Sendung oder Aufnahme auswählen":title,fontLarge,X(tw));
        int y=HL::detailY+185;
        for(int i=0;i<2&&i<int(titleLines.size());++i){
            printFit(tx,y,titleLines[i]+(i==1&&titleLines.size()>2?"…":""),fontLarge,kText,tw);y+=30;
        }
        fill(tx,y+3,tx+tw-1,y+3,0xFF294762u);y+=12;
        if(!facts.empty()){
            const auto factLines=suiteosd::HeroLines(facts,fontSmall,X(tw));
            for(size_t i=0;i<factLines.size()&&i<2;++i){
                printFit(tx,y,factLines[i]+(i==1&&factLines.size()>2?"…":""),fontSmall,0xFFE2EDF8u,tw);y+=23;
            }
        }
        if(p&&p->end>p->start){
            const int left=HL::remainingMinutes(p->start,p->end,time(nullptr));
            std::string timing=p->time+" · "+std::to_string((p->end-p->start+59)/60)+" Min.";
            print(tx,y,timing,fontSmall,kText,tw);y+=23;
            if(left>=0){print(tx,y,"Noch "+std::to_string(left)+" Min.",fontSmall,0xFF16DDF2u,tw);y+=23;}
            else if(p->start>time(nullptr)){print(tx,y,"Beginn: "+formatTime(p->start),fontSmall,kFocus,tw);y+=23;}
        }else if(rec){print(tx,y,recordingDate(rec->start),fontSmall,kMuted,tw);y+=23;}
        if(!genre.empty()){print(tx,y,genre,fontCinemaMeta?fontCinemaMeta:fontSmall,0xFF70DBF5u,tw);y+=20;}
        y+=8;
        std::string director,cast;
        if(cached!=focusPreviews.end()){
            for(const auto &person:cached->second.metadata.people){
                if(person.role=="Regie"&&director.empty())director=person.name;
                if(person.role=="Schauspiel"||person.role=="Gastrolle"){
                    cast+=(cast.empty()?"":", ")+person.name;
                }
            }
        }
        std::vector<std::string> credits;
        if(!director.empty())credits.push_back("Regie: "+director);
        if(!cast.empty()){
            const auto castLines=suiteosd::HeroLines("Mit: "+cast,fontSmall,X(tw));
            for(size_t i=0;i<castLines.size()&&i<2;++i)
                credits.push_back(castLines[i]+(i==1&&castLines.size()>2?"…":""));
        }
        const int summaryBottom=HL::detailBottom-26-int(credits.size())*23;
        const auto lines=suiteosd::HeroLines(description,fontSmall,X(tw));
        for(size_t i=0;i<lines.size()&&y<=summaryBottom;++i){
            if(!title.empty())printFit(tx,y,lines[i]+(y+23>summaryBottom&&i+1<lines.size()?" …":""),fontSmall,kText,tw);
            y+=23;
        }
        for(const auto &credit:credits){
            if(y>HL::detailBottom-26)break;
            printFit(tx,y,credit,fontSmall,kMuted,tw);y+=23;
        }
    }
    void drawHomeSidePanel() {
        focusClockMinute=time(nullptr)/60;
        drawHomeFocusPanel();
    }

    void drawHomeProgrammeInfo()
    {
        if (!homeProgrammeDetails) return;
        const Programme *p = selectedHomeProgramme();
        if (!p) return;
        panel(266, 163, 1087, 550, true, kSurface);
        print(301, 190, p->title, fontTitle, kText, 980);
        print(301, 256, p->channel + "  ·  " + p->time, fontBody, kFocus, 950);
        std::string synopsis = p->description.empty() ?
            "Keine EPG-Beschreibung vorhanden." : p->description;
        std::replace(synopsis.begin(), synopsis.end(), '\n', ' ');
        std::istringstream words(synopsis);
        std::string line, word;
        int y = 317;
        while (words >> word && y < 655) {
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && fontBody && fontBody->Width(candidate.c_str()) > X(972)) {
                print(301, y, line, fontBody, kText, 988);
                line = word;
                y += 33;
            } else
                line = candidate;
        }
        if (!line.empty() && y <= 671)
            print(301, y, line, fontBody, kText, 988);
        fill(290,638,1328,704,kSurface);
        print(301,643,homeTimerConfirm?"OK: Timer bestätigen · Zurück: Abbrechen":"Rot: Timer erstellen · Info/Zurück: Schließen",fontSmall,kFocus,1000);
        print(301,676,homeTimerNotice,fontSmall,kText,1000);
    }

    static const char *settingsTitle(int id)
    {
        static const char *const titles[] = {
            "VDR", "VDR-Suite", "Plugins", "TV-Oberfläche",
            "Benutzer & Geräte", "System"
        };
        return id >= 0 && id < 6 ? titles[id] : "Einstellungen";
    }

    static const char *settingsSubtitle(int id)
    {
        static const char *const titles[] = {
            "OSD · EPG · DVB · Aufnahmen · Wiedergabe",
            "Backend · Addons · Media Tools",
            "Einstellungen installierter VDR-Plugins",
            "Suite-OSD · Vorschau · Sprache",
            "Konten · Rechte · TV-Kopplung",
            "Versionen · VDR-Pluginstatus"
        };
        return id >= 0 && id < 6 ? titles[id] : "";
    }

    static const char *settingsOwner(int id)
    {
        switch (id) {
        case suiteosd::SettingsNavigation::Vdr: return "Eigentümer: VDR-Kern";
        case suiteosd::SettingsNavigation::Suite: return "Eigentümer: VDR-Suite Core / Agent";
        case suiteosd::SettingsNavigation::Plugins: return "Eigentümer: jeweiliges VDR-Plugin";
        case suiteosd::SettingsNavigation::Television: return "Eigentümer: Suite-OSD / VDR";
        case suiteosd::SettingsNavigation::Accounts: return "Eigentümer: VDR-Suite Core";
        default: return "Lokale VDR-Systeminformationen";
        }
    }

    const std::vector<suiteosd::CoreItem> &coreList() const
    {
        return coreNav.page == suiteosd::CorePage::Backends ||
               coreNav.page == suiteosd::CorePage::BackendSettings ?
            coreBackends : coreNav.page == suiteosd::CorePage::Devices ||
                           coreNav.page == suiteosd::CorePage::DeviceDetails ?
            coreDevices : coreAccounts;
    }

    const suiteosd::CoreItem *coreSelectedItem() const
    {
        const auto &items = coreList();
        return coreNav.selected >= 0 && coreNav.selected < int(items.size())
            ? &items[coreNav.selected] : nullptr;
    }

    void coreReport(const suiteosd::CoreStatus &result,
                    const std::string &success = "")
    {
        coreNotice = result.ok ? success :
            (result.message.empty() ? "Core-Aktion fehlgeschlagen" : result.message);
    }

    void refreshCoreAdmin()
    {
        const auto page = coreNav.page;
        suiteosd::CoreStatus result;
        if (page == suiteosd::CorePage::Backends) {
            result = coreAdmin.backends(coreBackends);
        } else if (page == suiteosd::CorePage::Accounts) {
            result = coreAdmin.accounts(coreAccounts);
        } else if (page == suiteosd::CorePage::Devices) {
            result = coreAdmin.devices(coreDevices);
        } else if (page == suiteosd::CorePage::BackendSettings) {
            const auto *item = coreSelectedItem();
            if (!item) return;
            const std::string id = item->id;
            const auto addon = coreAdmin.addon(id, coreAddon);
            coreAddonLoaded = addon.ok;
            const auto encoding = coreAdmin.encoding(id, coreEncoding);
            coreEncodingLoaded = encoding.ok;
            if (coreEncodingLoaded)
                coreEncodingEdit = coreEncoding;
            if (!addon.ok && !encoding.ok)
                result = addon;
            else if (!addon.ok)
                result = addon;
            else if (!encoding.ok)
                result = encoding;
            else
                result = {true, 200, ""};
        } else return;
        coreReport(result, result.ok ? "Core-Daten aktualisiert" : "");
        if (result.http == 401 || result.http == 403) {
            if (page == suiteosd::CorePage::Backends) coreBackends.clear();
            if (page == suiteosd::CorePage::Accounts) coreAccounts.clear();
            if (page == suiteosd::CorePage::Devices) coreDevices.clear();
            coreNotice = result.http == 401 ?
                "Geräteanmeldung prüfen: VDR-Setup / Plugins / suiteosd" :
                "Berechtigungen und Administration im Webfrontend verwalten";
        }
        if (coreNav.selected >= int(coreList().size()))
            coreNav.selected = std::max(0, int(coreList().size()) - 1);
    }

    void openCoreAdmin(suiteosd::CorePage target)
    {
        coreAdmin.setOrigin(gCoreUrl);
        coreNotice.clear();
        coreNav.selected = coreNav.action = 0;
        coreNav.confirmation = suiteosd::CoreConfirmation::None;
        coreNav.page = target;
        if (!restoreDevice(coreAdmin)) {
            coreAdmin.forgetDevice();
            coreBackends.clear(); coreAccounts.clear(); coreDevices.clear();
            coreNotice = "Gerät koppeln: VDR-Setup / Plugins / suiteosd";
            return;
        }
        refreshCoreAdmin();
    }

    static const char *corePageName(suiteosd::CorePage page)
    {
        switch (page) {
        case suiteosd::CorePage::Backends: return "Backend auswählen";
        case suiteosd::CorePage::BackendSettings: return "Backend und Addons";
        case suiteosd::CorePage::Accounts: return "Benutzerkonten";
        case suiteosd::CorePage::AccountDetails: return "Konto bearbeiten";
        case suiteosd::CorePage::Devices: return "Gekoppelte TV-Geräte";
        case suiteosd::CorePage::DeviceDetails: return "Gerät verwalten";
        case suiteosd::CorePage::Rename: return "Anzeigename ändern";
        default: return "VDR-Suite Core";
        }
    }

    void drawCoreAdmin()
    {
        fill(0, 0, 1599, 899, kBackground);
        fill(0, 0, 1599, 76, kSurface);
        print(33, 23, "VDR Suite", fontLarge, kText, 226);
        print(279, 23, corePageName(coreNav.page), fontLarge);
        print(1250, 29, "Core-Verwaltung", fontSmall, kMuted);
        panel(148, 113, 1417, 700, false, kSurface);

        const auto *item = coreSelectedItem();
        if (coreNav.page == suiteosd::CorePage::Rename) {
            print(189, 140, "Anzeigename des ausgewählten Benutzerkontos",
                  fontBody, kFocus, 1280);
            panel(185, 187, 1326, 64, true, kCard);
            print(207, 200, "Anzeigename: " + coreNav.editingName,
                  fontBody, kText, 1270);
            print(192, 281, coreNav.uppercase ? "GROSSBUCHSTABEN" :
                  "Kleinbuchstaben", fontSmall, kFocus);
            const std::string &chars = suiteosd::CoreAdminNavigation::keys();
            for (int i = 0; i < int(chars.size()); ++i) {
                const int x = 193 + (i % 10) * 130;
                const int y = 319 + (i / 10) * 79;
                panel(x, y, 111, 67, i == coreNav.keyIndex, kCardAlt);
                char c = chars[i];
                if (coreNav.uppercase && c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
                print(x + 39, y + 15, c == ' ' ? "␣" : std::string(1,c),
                      fontBody, kText, 79);
            }
            print(193, 740, "OK: Zeichen  ·  Grün: Feld leeren  ·  Rot: Namen speichern",
                  fontSmall, kText, 1200);
            print(193, 777, "Gelb: Löschen  ·  Blau: Groß/Klein  ·  Zifferntasten: Direkt",
                  fontSmall, kMuted, 1220);
        } else if (coreNav.page == suiteosd::CorePage::Backends ||
                   coreNav.page == suiteosd::CorePage::Accounts ||
                   coreNav.page == suiteosd::CorePage::Devices) {
            const auto &items = coreList();
            print(185, 144, coreAdmin.hasDeviceCredential() ?
                  "Geräteberechtigung gespeichert · VDR-Suite Core" :
                  "Nicht gekoppelt · VDR-Suite Core",
                  fontBody, kFocus, 1280);
            const int rows = 8;
            const int first = coreNav.selected / rows * rows;
            for (int i = 0; i < rows && first + i < int(items.size()); ++i) {
                const int index = first + i;
                const auto &item = items[index];
                const int y = 212 + i * 68;
                panel(183, y, 1327, 58, index == coreNav.selected, kCardAlt);
                print(206, y + 11, item.name, fontBody, kText, 768);
                print(1044, y + 12,
                      item.revoked ? "Widerrufen" :
                      (item.active ? "Aktiv" : "Inaktiv"),
                      fontSmall, item.revoked ? kMuted : kFocus, 200);
                print(1235, y + 16, item.id, fontSmall, kMuted, 230);
            }
            if (items.empty())
                print(211, 298, "Keine Einträge gefunden", fontBody, kMuted, 1000);
            print(189, 775, "OK: Öffnen   ·   Blau: Konten / Geräte   ·   Gelb: Aktualisieren",
                  fontSmall, kFocus, 1210);
        } else {
            if (item) {
                print(189, 150, item->name, fontTitle, kText, 1255);
                print(190, 214, "ID: " + item->id, fontSmall, kMuted, 1200);
            }
            if (coreNav.page == suiteosd::CorePage::BackendSettings) {
                const std::string addonValue = coreAddonLoaded ?
                    (coreAddon.enabled ? "Aktiviert" : "Deaktiviert") :
                    "Aktivierung derzeit nicht lesbar";
                const std::string encodingValue = coreEncodingLoaded ?
                    "Import H.264 CRF: " + std::to_string(coreEncodingEdit.importCrf) :
                    "Import CRF nicht lesbar";
                const std::string shrinkValue = coreEncodingLoaded ?
                    "Shrink H.265 CRF: " + std::to_string(coreEncodingEdit.shrinkCrf) :
                    "Shrink CRF nicht lesbar";
                const std::array<std::string, 5> actions = {{
                    "Media Tools: " + addonValue + " (OK: Umschalten)",
                    encodingValue + " (Links/Rechts)",
                    shrinkValue + " (Links/Rechts)",
                    "Encoding für neue Aufträge speichern",
                    "Status vom Core neu laden"
                }};
                print(189, 267, "Originalwerte und Revision vom Core / Agent",
                      fontBody, kFocus);
                for (int i = 0; i < int(actions.size()); ++i) {
                    const int y = 331 + i * 88;
                    panel(184, y, 1315, 75, coreNav.action == i, kCardAlt);
                    print(207, y + 20, actions[i], fontBody, kText, 1210);
                }
                print(197, 781, "OK: Aktion  ·  Pfeile: CRF ändern  ·  Zurück: Backends",
                      fontSmall, kMuted);
            } else if (coreNav.page == suiteosd::CorePage::AccountDetails) {
                print(191, 294, std::string("Kontostatus: ") +
                      (item && item->active ? "Aktiv" : "Inaktiv"), fontBody, kFocus);
                print(191, 366, "Rot: Konto aktivieren / deaktivieren",
                      fontBody, kText);
                print(191, 421, "Grün: Anzeigenamen ändern", fontBody, kText);
                print(191, 507, "Prüfung von ETag, Rechten und letztem Admin: VDR-Suite Core",
                      fontSmall, kMuted, 1200);
            } else if (coreNav.page == suiteosd::CorePage::DeviceDetails) {
                print(190, 291, item && item->revoked ? "Gerät widerrufen" :
                      item && item->active ? "Gerät aktiv" : "Gerät inaktiv",
                      fontBody, kFocus);
                print(190, 376, "Rot: Gerätezugang widerrufen (alle Credentials)",
                      fontBody, kText);
                print(190, 449, "Widerruf ist nicht rückgängig zu machen.",
                      fontBody, kMuted);
                print(190, 511, "Aktuelle Geräte-ID und ETag werden vor Änderung geprüft.",
                      fontSmall, kFocus, 1150);
            }
        }
        fill(16, 832, 1581, 895, kSurface);
        print(164, 850, "Zurück: Vorherige Seite", fontSmall, kMuted);
        if (!coreNotice.empty()) {
            fill(162, 782, 1540, 829, kCard);
            print(188, 795, coreNotice, fontSmall, kFocus, 1310);
        }
        if (coreNav.confirmation != suiteosd::CoreConfirmation::None) {
            panel(333, 289, 928, 303, true, kSurface);
            std::string question;
            if (coreNav.confirmation == suiteosd::CoreConfirmation::ToggleAddon)
                question = coreAddon.enabled ? "Media Tools deaktivieren?" :
                                              "Media Tools aktivieren?";
            else if (coreNav.confirmation == suiteosd::CoreConfirmation::ToggleAccount)
                question = item && item->active ? "Konto deaktivieren?" :
                                                   "Konto aktivieren?";
            else
                question = "Ausgewähltes Gerät unwiderruflich widerrufen?";
            print(378, 354, question, fontLarge, kText, 829);
            if (item) print(379, 416, item->id, fontSmall, kMuted, 820);
            print(377, 514, "OK: Bestätigen     Zurück: Abbrechen",
                  fontBody, kFocus);
        }
        drawSidebar();
    }

    void executeCoreConfirmation()
    {
        const auto confirm = coreNav.confirmation;
        coreNav.confirmation = suiteosd::CoreConfirmation::None;
        const suiteosd::CoreItem *current = coreSelectedItem();
        suiteosd::CoreStatus result;

        if (confirm == suiteosd::CoreConfirmation::ToggleAddon) {
            if (!coreAddonLoaded) {
                coreNotice = "Addon-Revision nicht verfügbar";
                return;
            }
            suiteosd::CoreAddon updated;
            result = coreAdmin.setAddon(coreAddon, !coreAddon.enabled, updated);
            if (result.ok) {
                coreAddon = updated;
                coreNotice = updated.enabled ? "Media Tools aktiviert" :
                                            "Media Tools deaktiviert";
            } else coreReport(result);
        } else if (confirm == suiteosd::CoreConfirmation::ToggleAccount) {
            if (!current) return;
            const suiteosd::CoreItem target = *current;
            result = coreAdmin.changeAccount(target, "", true, !target.active);
            if (result.ok) {
                coreNav.page = suiteosd::CorePage::Accounts;
                refreshCoreAdmin();
                if (coreNav.page == suiteosd::CorePage::Accounts &&
                    coreSelectedItem())
                    coreNav.page = suiteosd::CorePage::AccountDetails;
                coreNotice = target.active ? "Konto deaktiviert" : "Konto aktiviert";
            } else coreReport(result);
        } else if (confirm == suiteosd::CoreConfirmation::RevokeDevice) {
            if (!current) return;
            const suiteosd::CoreItem target = *current;
            result = coreAdmin.revokeDevice(target);
            if (result.ok) {
                coreNav.page = suiteosd::CorePage::Devices;
                refreshCoreAdmin();
                coreNav.page = suiteosd::CorePage::DeviceDetails;
                coreNotice = "Gerätezugang widerrufen";
            } else coreReport(result);
        }
    }

    void submitCoreKeyboard()
    {
        if (coreNav.page == suiteosd::CorePage::Rename) {
            const auto *item = coreSelectedItem();
            if (!item) {
                coreNotice = "Benutzerkonto nicht ausgewählt";
                return;
            }
            const auto changed = coreAdmin.changeAccount(
                *item, coreNav.editingName, false, false);
            if (changed.ok) {
                coreNav.page = suiteosd::CorePage::Accounts;
                refreshCoreAdmin();
                coreNav.page = suiteosd::CorePage::AccountDetails;
                coreNav.editingName.clear();
                coreNotice = "Anzeigename gespeichert";
            } else coreReport(changed);
        }
    }

    eOSState coreAdminKey(eKeys base)
    {
        using suiteosd::CorePage;
        using suiteosd::CoreConfirmation;
        if (coreNav.confirmation != CoreConfirmation::None) {
            if (base == kOk)
                executeCoreConfirmation();
            else if (base == kBack || base == kRed)
                coreNav.confirmation = CoreConfirmation::None;
            else
                return osContinue;
            draw();
            return osContinue;
        }

        if (coreNav.page == CorePage::Rename) {
            if (base == kBack) {
                coreNav.back();
            } else if (base == kGreen) {
                coreNav.editingName.clear();
            } else if (base == kBlue)
                coreNav.uppercase = !coreNav.uppercase;
            else if (base == kYellow)
                coreNav.backspace();
            else if (base == kRed)
                submitCoreKeyboard();
            else if (base == kOk)
                coreNav.typeKey();
            else if (base == kUp)
                coreNav.moveKey(0, -1);
            else if (base == kDown)
                coreNav.moveKey(0, 1);
            else if (base == kLeft)
                coreNav.moveKey(-1, 0);
            else if (base == kRight)
                coreNav.moveKey(1, 0);
            else {
                char digit = '\0';
                switch (base) {
                case k0: digit = '0'; break;
                case k1: digit = '1'; break;
                case k2: digit = '2'; break;
                case k3: digit = '3'; break;
                case k4: digit = '4'; break;
                case k5: digit = '5'; break;
                case k6: digit = '6'; break;
                case k7: digit = '7'; break;
                case k8: digit = '8'; break;
                case k9: digit = '9'; break;
                default: return osContinue;
                }
                coreNav.digit(digit);
            }
            draw();
            return osContinue;
        }

        if (base == kBack) {
            coreNav.back();
        } else if (coreNav.page == CorePage::Backends ||
                   coreNav.page == CorePage::Accounts ||
                   coreNav.page == CorePage::Devices) {
            const int total = int(coreList().size());
            if (base == kUp)
                coreNav.selected = std::max(0, coreNav.selected - 1);
            else if (base == kDown)
                coreNav.selected = std::min(std::max(0,total - 1),coreNav.selected + 1);
            else if (base == kLeft)
                coreNav.selected = std::max(0, coreNav.selected - 8);
            else if (base == kRight)
                coreNav.selected = std::min(std::max(0,total - 1),coreNav.selected + 8);
            else if (base == kBlue) {
                if (coreNav.page == CorePage::Backends ||
                    coreNav.page == CorePage::Devices)
                    coreNav.page = CorePage::Accounts;
                else
                    coreNav.page = CorePage::Devices;
                coreNav.selected = 0;
                refreshCoreAdmin();
            } else if (base == kYellow) {
                refreshCoreAdmin();
            } else if (base == kOk) {
                if (coreSelectedItem()) {
                    if (coreNav.page == CorePage::Backends) {
                        coreNav.page = CorePage::BackendSettings;
                        coreNav.action = 0;
                        refreshCoreAdmin();
                    } else {
                        coreNav.page = coreNav.page == CorePage::Accounts ?
                                       CorePage::AccountDetails : CorePage::DeviceDetails;
                    }
                }
            } else return osContinue;
        } else if (coreNav.page == CorePage::BackendSettings) {
            if (base == kUp)
                coreNav.action = std::max(0, coreNav.action - 1);
            else if (base == kDown)
                coreNav.action = std::min(4, coreNav.action + 1);
            else if (base == kLeft || base == kRight) {
                if (coreEncodingLoaded && (coreNav.action == 1 || coreNav.action == 2)) {
                    int &crf = coreNav.action == 1 ?
                        coreEncodingEdit.importCrf : coreEncodingEdit.shrinkCrf;
                    crf = std::clamp(crf + (base == kRight ? 1 : -1), 0, 51);
                }
            } else if (base == kOk) {
                if (coreNav.action == 0 && coreAddonLoaded) {
                    coreNav.confirmation = CoreConfirmation::ToggleAddon;
                } else if (coreNav.action == 3 && coreEncodingLoaded) {
                    suiteosd::CoreEncoding updated;
                    const auto result =
                        coreAdmin.setEncoding(coreEncoding, coreEncodingEdit, updated);
                    if (result.ok) {
                        coreEncoding = coreEncodingEdit = updated;
                        coreNotice = "Encoding für neue Aufträge gespeichert";
                    } else coreReport(result);
                } else if (coreNav.action == 4)
                    refreshCoreAdmin();
            } else return osContinue;
        } else if (coreNav.page == CorePage::AccountDetails) {
            if (base == kRed) {
                if (coreSelectedItem())
                    coreNav.confirmation = CoreConfirmation::ToggleAccount;
            } else if (base == kGreen) {
                if (const auto *item = coreSelectedItem()) {
                    coreNav.editingName = item->name;
                    coreNav.page = CorePage::Rename;
                    coreNav.keyIndex = 0;
                    coreNav.uppercase = false;
                }
            } else return osContinue;
        } else if (coreNav.page == CorePage::DeviceDetails) {
            if (base == kRed) {
                if (const auto *item = coreSelectedItem()) {
                    if (!item->revoked)
                        coreNav.confirmation = CoreConfirmation::RevokeDevice;
                    else coreNotice = "Dieses Gerät ist bereits widerrufen";
                }
            } else return osContinue;
        } else return osContinue;
        draw();
        return osContinue;
    }

    void drawSettings()
    {
        if (coreNav.page != suiteosd::CorePage::None) {
            drawCoreAdmin();
            return;
        }
        fill(0, 0, 1599, 899, kBackground);
        fill(0, 0, 1599, 76, kSurface);
        print(33, 23, "VDR Suite", fontLarge, kText, 226);
        print(274, 23, "Einstellungen", fontLarge);
        print(1154, 27, "VDR + Suite", fontBody, kMuted);

        if (!settingsNav.detail) {
            print(160, 111, "Eine Oberfläche · Bestehende Konfigurationen",
                  fontBody, kMuted, 1160);
            for (int i = 0; i < suiteosd::SettingsNavigation::kCategories; ++i) {
                const int x = 159 + (i % 2) * 698;
                const int y = 183 + (i / 2) * 210;
                const bool focus = !nav.sideFocused && settingsNav.selected == i;
                panel(x, y, 661, 186, focus, focus ? kHighlight : kCardAlt);
                print(x + 28, y + 28, settingsTitle(i), fontLarge, kText, 598);
                print(x + 28, y + 83, settingsSubtitle(i), fontBody, kMuted, 598);
                print(x + 28, y + 138, settingsOwner(i), fontSmall,
                      focus ? kText : kFocus, 602);
            }
        } else {
            const int category = settingsNav.selected;
            print(164, 113, std::string("Einstellungen  /  ") +
                  settingsTitle(category), fontBody, kFocus, 1240);
            panel(152, 168, 1410, 641, false, kSurface);
            print(194, 196, settingsTitle(category), fontTitle, kText, 1200);
            print(194, 258, settingsOwner(category), fontBody, kMuted, 1100);

            switch (category) {
            case suiteosd::SettingsNavigation::Vdr:
                print(194, 329, "VDR verwaltet seine Einstellungen selbst.",
                      fontBody, kText, 1120);
                print(194, 368, "OSD, EPG, DVB, LNB, CAM, Aufnahmen, Wiedergabe und Plugins",
                      fontBody, kMuted, 1190);
                break;
            case suiteosd::SettingsNavigation::Suite:
                print(194, 327, "Backend, Addons und Media Tools direkt im Core verwalten.",
                      fontBody, kText, 1160);
                print(194, 368, "Gerät in VDR-Setup / Plugins / suiteosd koppeln.",
                      fontBody, kMuted, 1200);
                print(194, 418, "Core: " + std::string(gCoreUrl),
                      fontSmall, kFocus, 1160);
                break;
            case suiteosd::SettingsNavigation::Plugins: {
                print(194, 325, "Plugin-Einstellungen über das native VDR-Menü.",
                      fontBody, kText, 1160);
                print(194, 366, "Nach dem Öffnen: Einstellungen  >  Plugins auswählen.",
                      fontBody, kMuted, 1170);
                std::string installed;
                for (const char *name : {"tvscraper", "epgsearch", "web", "osdteletext"}) {
                    if (cPluginManager::GetPlugin(name)) {
                        if (!installed.empty()) installed += "  ·  ";
                        installed += name;
                    }
                }
                print(194, 414, "Installiert (Auswahl): " +
                      (installed.empty() ? "keine der vier geprüften Plugins" : installed),
                      fontSmall, kFocus, 1150);
                break;
            }
            case suiteosd::SettingsNavigation::Television:
                print(194, 327, "VDR-Suite-Design und Fernbedienungsnavigation.",
                      fontBody, kText, 1100);
                print(194, 366, "Die Live-TV-Vorschau nutzt die native Videoausgabe.",
                      fontBody, kMuted, 1130);
                print(194, 409, "Die VDR-Sprache und OSD-Grundeinstellungen bleiben bei VDR.",
                      fontSmall, kMuted, 1160);
                break;
            case suiteosd::SettingsNavigation::Accounts:
                print(194, 328, "Konten und Geräteberechtigungen im Webfrontend verwalten.",
                      fontBody, kText, 1150);
                print(194, 369, "Geräte koppeln: VDR-Setup / Plugins / suiteosd.",
                      fontBody, kMuted, 1170);
                print(194, 420, "Der Core prüft die bestehenden Geräteberechtigungen.",
                      fontSmall, kFocus, 1190);
                break;
            case suiteosd::SettingsNavigation::System:
                print(194, 325, std::string("Suite-OSD-Version: ") + VERSION,
                      fontBody, kText);
                print(194, 372, std::string("Kompiliert für VDR: ") + VDRVERSION,
                      fontBody, kText);
                print(194, 419, std::string("TVScraper: ") +
                      (cPluginManager::GetPlugin("tvscraper") ? "installiert" : "nicht geladen"),
                      fontBody, kMuted);
                print(194, 462, std::string("Epgsearch: ") +
                      (cPluginManager::GetPlugin("epgsearch") ? "installiert" : "nicht geladen"),
                      fontBody, kMuted);
                print(194, 514, "Backend-Status wird hier noch nicht aus Core gelesen.",
                      fontSmall, kFocus, 1170);
                break;
            }

            if (settingsNav.ActionCount() > 0) {
                for (int i = 0; i < settingsNav.ActionCount(); ++i) {
                    const int y = 512 + i * 94;
                    const bool selected = settingsNav.action == i && !nav.sideFocused;
                    panel(190, y, 1180, 69, selected, selected ? kHighlight : kCard);
                    std::string title;
                    if (category == suiteosd::SettingsNavigation::Vdr)
                        title = "VDR-Einstellungen öffnen";
                    else if (category == suiteosd::SettingsNavigation::Suite)
                        title = "Backend und Media Tools im Core verwalten";
                    else if (category == suiteosd::SettingsNavigation::Accounts)
                        title = "Konten- und Geräteinformationen";
                    else if (category == suiteosd::SettingsNavigation::Plugins)
                        title = "VDR-Setup mit Plugin-Einstellungen öffnen";
                    else if (category == suiteosd::SettingsNavigation::Television)
                        title = i == 0 ?
                            std::string("Live-TV-Fenster in Aufnahmen: ") +
                                (gLivePreviewEnabled ? "Ein" : "Aus") :
                            "VDR-OSD und Spracheinstellungen öffnen";
                    print(218, y + 16, title, fontBody, kText, 1096);
                }
            } else
                print(194, 665, "Informationen · Weitere Änderungen im Suite-Webfrontend",
                      fontSmall, kFocus, 1180);
        }

        fill(16, 837, 1581, 893, kSurface);
        print(151, 852, "Pfeile: Navigation", fontSmall);
        print(518, 852, "OK: Öffnen / Ändern", fontSmall);
        print(992, 852, "Zurück: Übersicht / Home", fontSmall);
        if (!settingsNotice.empty()) {
            fill(155, 785, 1552, 833, kSurface);
            print(177, 795, settingsNotice, fontSmall, kFocus, 1280);
        }
        drawSidebar();
    }

    void openSettings()
    {
        photosActive = false;
        photoThumbnails.clear();
        if (recordingsActive)
            closeRecordingBrowser(); // restore scaled video before changing views
        timelineActive = false;
        recordingDetails = false;
        homeProgrammeDetails = false;
        settingsActive = true;
        settingsNotice.clear();
        coreAdmin.setOrigin(gCoreUrl);
        coreNav.page = suiteosd::CorePage::None;

        settingsNav.Open();
        nav.sideFocused = false;
    }

    bool toggleLivePreviewSetting()
    {
        cPlugin *owner = cPluginManager::GetPlugin("suiteosd");
        if (!owner) {
            settingsNotice = "VDR-Suite-Plugin-Konfiguration nicht verfügbar";
            return false;
        }
        const int previous = gLivePreviewEnabled;
        const int next = previous ? 0 : 1;
        // Reuse VDR's authoritative plugin section in setup.conf.
        owner->SetupStore("LivePreview", next);
        if (!Setup.Save()) {
            owner->SetupStore("LivePreview", previous);
            settingsNotice = "VDR konnte die Einstellung nicht speichern";
            return false;
        }
        gLivePreviewEnabled = next;
        settingsNotice = next ? "Live-TV-Vorschau eingeschaltet" :
                                "Live-TV-Vorschau ausgeschaltet";
        return true;
    }

    eOSState settingsKey(eKeys key)
    {
        if (coreNav.page != suiteosd::CorePage::None)
            return coreAdminKey(key);
        if (nav.sideFocused) {
            switch (key) {
            case kUp: nav.Move(suiteosd::Direction::Up, 1, 2); break;
            case kDown: nav.Move(suiteosd::Direction::Down, 1, 2); break;
            case kLeft:
            case kBack:
            case kRight: nav.sideFocused = false; break;
            case kOk: {
                const int dest = nav.sideIndex;
                nav.sideFocused = false;
                if (dest == 10) {
                    settingsNav.Open();
                } else if (dest == 0) {
                    settingsActive = false;
                } else {
                    settingsActive = false;
                    const eOSState state = action(suiteosd::Activation::Module);
                    if (state != osContinue) return state;
                }
                break;
            }
            default: return osContinue;
            }
        } else if (key == kBack) {
            if (!settingsNav.Back())
                settingsActive = false;
        } else if (key == kUp)
            settingsNav.Move(0, -1);
        else if (key == kDown)
            settingsNav.Move(0, 1);
        else if (key == kLeft) {
            if (!settingsNav.detail &&
                settingsNav.selected % suiteosd::SettingsNavigation::kColumns == 0) {
                nav.sideIndex = 10;
                nav.sideFocused = true;
            } else
                settingsNav.Move(-1, 0);
        } else if (key == kRight)
            settingsNav.Move(1, 0);
        else if (key == kOk) {
            const suiteosd::SettingsAction selected = settingsNav.Select();
            if (selected == suiteosd::SettingsAction::OpenVdrSetup ||
                selected == suiteosd::SettingsAction::OpenPluginSetup) {
                // VDR owns Setup and plugin setup pages. The public VDR OSD
                // API doesn't provide a direct link to its private submenus.
                // Never claim a direct plugin submenu jump.
                if (cRemote::Put(kSetup, true))
                    return osEnd;
                settingsNotice = "VDR-Einstellungen konnten nicht geöffnet werden";
            } else if (selected == suiteosd::SettingsAction::ToggleLivePreview)
                toggleLivePreviewSetting();
            else if (selected == suiteosd::SettingsAction::OpenCoreBackend)
                openCoreAdmin(suiteosd::CorePage::Backends);
            else if (selected == suiteosd::SettingsAction::OpenCoreAccounts)
                openCoreAdmin(suiteosd::CorePage::Accounts);
            else
                settingsNotice.clear();
        } else
            return osContinue;
        if (!settingsActive) {

            coreNav.page = suiteosd::CorePage::None;
        }
        draw();
        return osContinue;
    }

    // SuiteOSD only presents and navigates; image_next owns configured
    // sources, directory enumeration, decoding and the VDR photo player.
    cPlugin *imageNext() const
    {
        return cPluginManager::GetPlugin("image_next");
    }

    bool loadPhotoSources()
    {
        photoThumbnails.clear();
        imagenext::Browse request;
        cPlugin *provider = imageNext();
        if (!provider || !provider->Service(imagenext::kBrowse, &request) ||
            !request.success) {
            photoGallery.SetSources({});
            photoNotice = provider ? "image_next: Galerie-Service nicht verfügbar" :
                                     "image_next ist nicht installiert oder geladen";
            return false;
        }
        photoGallery.SetSources(std::move(request.sources));
        photoVisualCenter = 0.0f;
        photoAnimating = false;
        photoNotice.clear();
        if (photoGallery.sources.empty())
            photoNotice = "Keine Bildquellen konfiguriert (imagesources.conf)";
        else if (photoGallery.sources.size() == 1) {
            const int source = photoGallery.sources.front().index;
            photoGallery.OpenSource(source);
            return loadPhotoDirectory();
        }
        return true;
    }

    bool loadPhotoDirectory()
    {
        photoThumbnails.clear();
        imagenext::Browse request;
        request.source = photoGallery.source;
        request.directory = photoGallery.directory;
        cPlugin *provider = imageNext();
        if (!provider || !provider->Service(imagenext::kBrowse, &request) ||
            !request.success) {
            photoGallery.SetEntries({});
            photoNotice = request.error.empty() ? "Bilderordner nicht verfügbar" :
                                                request.error;
            return false;
        }
        photoGallery.SetEntries(std::move(request.entries));
        photoVisualCenter = 0.0f;
        photoAnimating = false;
        photoNotice = photoGallery.entries.empty() ? "Dieser Ordner enthält keine Bilder" : "";
        return true;
    }

    void openPhotoGallery()
    {

        settingsActive = false;
        if (recordingsActive) closeRecordingBrowser();
        timelineActive = false;
        timelineDetails = false;
        homeProgrammeDetails = false;
        recordingDetails = false;
        photosActive = true;
        photoInfoVisible = false;
        photoGallery.SetView(gPhotoViewMode ? suiteosd::PhotoGallery::View::Carousel :
                                             suiteosd::PhotoGallery::View::Grid);
        nav.sideIndex = 8;
        nav.sideFocused = false;
        loadPhotoSources();
    }

    // Requests the visible bitmaps only. image_next returns not-ready
    // immediately and completes the load on its existing thumbnail worker.
    bool pollPhotoThumbnails()
    {
        cPlugin *provider = imageNext();
        if (!provider || photoGallery.InSources())
            return false;
        bool changed = false;
        const bool carousel = photoGallery.view == suiteosd::PhotoGallery::View::Carousel;
        const int first = carousel ?
            std::max(0, std::min(photoGallery.selected,
                                  int(std::floor(photoVisualCenter))) - 3) :
            photoGallery.firstVisible;
        const int last = carousel ?
            std::min(photoGallery.Count(),
                     std::max(photoGallery.selected, int(std::ceil(photoVisualCenter))) + 4) :
            std::min(photoGallery.Count(),
                     photoGallery.firstVisible + suiteosd::PhotoGallery::kVisible);
        for (int i = first; i < last; ++i) {
            const auto &item = photoGallery.entries[i];
            if (item.cover.empty()) continue;
            const std::string key = std::to_string(photoGallery.source) + ":" + item.cover;
            if (photoThumbnails.find(key) != photoThumbnails.end()) continue;
            imagenext::Thumbnail request;
            request.source = photoGallery.source;
            request.path = item.cover;
            request.maxWidth = std::min(896, std::max(1, X(carousel ? 720 : 266)));
            request.maxHeight = std::min(896, std::max(1, Y(carousel ? 450 : 140)));
            if (provider->Service(imagenext::kThumbnail, &request) && request.ready) {
                if (photoThumbnails.size() >= 20)
                    photoThumbnails.clear(); // bound the foreground ARGB copies
                photoThumbnails.emplace(key, std::move(request));
                changed = true;
            }
        }
        return changed;
    }

    eOSState playPhoto(bool slideshow)
    {
        const imagenext::Entry *selected = photoGallery.SelectedEntry();
        if (!selected || selected->directory)
            return osContinue;
        imagenext::Play request;
        request.source = photoGallery.source;
        request.path = selected->path;
        request.slideshow = slideshow;
        cPlugin *provider = imageNext();
        if (provider && provider->Service(imagenext::kPlay, &request) && request.started)
            return osEnd; // The existing image_next cControl now owns playback.
        photoNotice = "Bild konnte nicht mit image_next geöffnet werden";
        draw();
        return osContinue;
    }

    void advancePhotoCarousel()
    {
        if (!photoAnimating) return;
        const auto now = std::chrono::steady_clock::now();
        const float elapsed = float(std::chrono::duration_cast<
                                    std::chrono::milliseconds>(
                                    now - photoAnimationStart).count());
        const float t = std::clamp(elapsed / 220.0f, 0.0f, 1.0f);
        const float eased = t * t * (3.0f - 2.0f * t);
        photoVisualCenter = photoAnimationFrom +
                            (photoAnimationTo - photoAnimationFrom) * eased;
        if (t >= 1.0f) {
            photoVisualCenter = photoAnimationTo;
            photoAnimating = false;
        }
    }

    void startPhotoCarouselMove(int oldIndex)
    {
        if (oldIndex == photoGallery.selected) return;
        advancePhotoCarousel(); // a rapid second key continues smoothly
        photoAnimationFrom = photoVisualCenter;
        photoAnimationTo = float(photoGallery.selected);
        photoAnimationStart = std::chrono::steady_clock::now();
        photoLastFrame = photoAnimationStart;
        photoAnimating = true;
    }

    // VDR's existing setup.conf is the only persistent preference owner.
    void togglePhotoView()
    {
        const int previous = gPhotoViewMode;
        const int next = previous ? 0 : 1;
        cPlugin *owner = cPluginManager::GetPlugin("suiteosd");
        if (!owner) {
            photoNotice = "SuiteOSD-Einstellungen nicht verfügbar";
            return;
        }
        owner->SetupStore("PhotoView", next);
        if (!Setup.Save()) {
            owner->SetupStore("PhotoView", previous);
            photoNotice = "Ansicht konnte nicht gespeichert werden";
            return;
        }
        gPhotoViewMode = next;
        photoGallery.SetView(next ? suiteosd::PhotoGallery::View::Carousel :
                                    suiteosd::PhotoGallery::View::Grid);
        photoThumbnails.clear();
        photoVisualCenter = float(photoGallery.selected);
        photoAnimating = false;
        photoNotice.clear();
    }

    eOSState photoKey(eKeys base)
    {
        const bool carousel =
            photoGallery.view == suiteosd::PhotoGallery::View::Carousel;
        if (base == kNone) {
            bool refresh = pollPhotoThumbnails();
            if (carousel && photoAnimating) {
                const auto now = std::chrono::steady_clock::now();
                if (now - photoLastFrame >= std::chrono::milliseconds(33)) {
                    photoLastFrame = now;
                    advancePhotoCarousel();
                    refresh = true;
                }
            }
            if (refresh) draw();
            return osContinue;
        }

        // The approved gallery is fullscreen: no permanent left-hand sidebar.
        if (nav.sideFocused)
            nav.sideFocused = false;

        if (base == k1) {
            togglePhotoView();
        } else if (base == kBack || base == kBlue) {
            if (!photoGallery.Back()) {
                photosActive = false;
                photoThumbnails.clear();
                nav.section = 0;
            } else if (photoGallery.InSources()) {
                imagenext::Browse sources;
                cPlugin *provider = imageNext();
                if (provider && provider->Service(imagenext::kBrowse, &sources) &&
                    sources.success)
                    photoGallery.SetSources(std::move(sources.sources));
            } else {
                loadPhotoDirectory();
            }
            photoThumbnails.clear();
            photoVisualCenter = float(photoGallery.selected);
            photoAnimating = false;
            photoInfoVisible = false;
        } else if (base == kLeft || base == kRight) {
            const int previous = photoGallery.selected;
            if (carousel) {
                photoGallery.MoveCarousel(base == kLeft ? -1 : 1);
                startPhotoCarouselMove(previous);
            } else {
                photoGallery.Move(base == kLeft ? -1 : 1, 0);
            }
        } else if (base == kUp || base == kDown) {
            if (!carousel)
                photoGallery.Move(0, base == kUp ? -1 : 1);
            else
                return osContinue;
        } else if (base == kChanUp || base == kChanDn) {
            if (carousel) {
                photoGallery.MoveCarousel(base == kChanUp ? 5 : -5);
                photoVisualCenter = float(photoGallery.selected);
                photoAnimating = false;
            } else {
                photoGallery.Page(base == kChanUp ? 1 : -1);
            }
        } else if (base == k0) {
            if (photoGallery.InSources())
                loadPhotoSources();
            else
                loadPhotoDirectory();
            photoVisualCenter = float(photoGallery.selected);
            photoAnimating = false;
        } else if (base == kYellow || base == kInfo) {
            photoInfoVisible = !photoInfoVisible;
        } else if (base == kOk || base == kRed || base == kGreen) {
            if (photoGallery.InSources()) {
                const auto *source = photoGallery.SelectedSource();
                if (source) {
                    photoGallery.OpenSource(source->index);
                    loadPhotoDirectory();
                }
            } else {
                const auto *entry = photoGallery.SelectedEntry();
                if (entry && entry->directory) {
                    photoGallery.OpenDirectory(entry->path);
                    loadPhotoDirectory();
                } else if (entry) {
                    return playPhoto(base == kGreen);
                }
            }
            photoVisualCenter = float(photoGallery.selected);
            photoAnimating = false;
            photoInfoVisible = false;
        } else {
            return osContinue;
        }
        draw();
        return osContinue;
    }

    std::string photoFolderTitle() const
    {
        if (photoGallery.InSources())
            return "Bildquellen";
        std::string source = "Bilder";
        for (const auto &item : photoGallery.sources) {
            if (item.index == photoGallery.source) {
                source = item.title;
                break;
            }
        }
        return photoGallery.directory.empty() ?
               source : source + " / " + photoGallery.directory;
    }

    void drawPhotoCarousel()
    {
        // The gallery uses all available width. The floor/reflections are
        // native true-colour OSD pixels; there is no browser or GPU dependency.
        for (int y = 568; y < 810; y += 12) {
            const int intensity = std::max(0, 0x34 - (y - 568) / 5);
            fill(0, y, 1599, y + 11,
                 (0xD0U << 24) | (uint32_t(intensity / 3) << 16) |
                 (uint32_t(intensity) << 8) | uint32_t(intensity + 12));
        }

        // Photos farther away are painted first, so arriving photos appear
        // to travel in front of the receding ones, towards the centre.
        const float centre = photoVisualCenter;
        std::vector<int> visible;
        const int first = std::max(0, int(std::floor(centre)) - 3);
        const int last = std::min(photoGallery.Count(), int(std::ceil(centre)) + 4);
        for (int index = first; index < last; ++index)
            if (std::fabs(float(index) - centre) <= 2.22f)
                visible.push_back(index);
        std::stable_sort(visible.begin(), visible.end(), [centre](int a, int b) {
            return std::fabs(float(a) - centre) > std::fabs(float(b) - centre);
        });

        for (int index : visible) {
            const auto slot = suiteosd::CarouselGeometry(float(index) - centre);
            if (slot.width < 1 || slot.height < 1) continue;
            const bool foreground =
                std::fabs(float(index) - centre) < 0.5f;
            const int drawX = X(slot.x);
            const int drawY = Y(slot.y);
            const int drawW = std::max(1, X(slot.width));
            const int drawH = std::max(1, Y(slot.height));

            std::string label;
            std::string cover;
            bool folder = true;
            if (photoGallery.InSources()) {
                label = photoGallery.sources[index].title;
            } else {
                const auto &item = photoGallery.entries[index];
                label = item.name;
                cover = item.cover;
                folder = item.directory;
            }

            // Deep cyan focus surround; side cards have quieter blue edges.
            if (foreground) {
                fill(slot.x - 9, slot.y - 9,
                     slot.x + slot.width + 9, slot.y + slot.height + 9,
                     0x6637C8EE);
                fill(slot.x - 4, slot.y - 4,
                     slot.x + slot.width + 4, slot.y + slot.height + 4,
                     kFocus);
            }

            const std::string key = std::to_string(photoGallery.source) + ":" + cover;
            const auto found = cover.empty() ? photoThumbnails.end() :
                              photoThumbnails.find(key);
            bool rendered = false;
            if (found != photoThumbnails.end() && found->second.ready &&
                !found->second.argb.empty()) {
                const auto &thumb = found->second;
                const auto picture = suiteosd::WarpCarousel(
                    thumb.argb.data(), thumb.width, thumb.height,
                    drawW, drawH, slot.side, slot.inset, slot.opacity);
                if (picture.valid()) {
                    cImage image(cSize(picture.width, picture.height),
                                 picture.argb.data());
                    osd->DrawImage(cPoint(drawX, drawY), image);
                    const auto reflection = suiteosd::ReflectCarousel(
                        picture, std::max(1, Y(foreground ? 72 : 48)));
                    if (reflection.valid()) {
                        cImage mirror(cSize(reflection.width, reflection.height),
                                      reflection.argb.data());
                        osd->DrawImage(cPoint(drawX, drawY + drawH + Y(9)), mirror);
                    }
                    rendered = true;
                }
            }
            if (!rendered) {
                fill(slot.x + 1, slot.y + 1,
                     slot.x + slot.width - 1, slot.y + slot.height - 1,
                     foreground ? kCardAlt : kCard);
                print(slot.x + 16, slot.y + slot.height / 2 - 20,
                      folder ? "ORDNER" : "FOTO", fontBody,
                      foreground ? kFocus : kMuted,
                      std::max(1, slot.width - 28));
                if (foreground)
                    print(slot.x + 17, slot.y + slot.height / 2 + 16,
                          label, fontSmall, kText, slot.width - 35);
            }
        }

        std::string name;
        if (photoGallery.InSources()) {
            const auto *entry = photoGallery.SelectedSource();
            if (entry) name = entry->title;
        } else {
            const auto *entry = photoGallery.SelectedEntry();
            if (entry) name = entry->name;
        }
        if (photoGallery.Count()) {
            print(454, 694, name, fontLarge, kText, 692);
            print(678, 745, "<   " +
                  std::to_string(photoGallery.selected + 1) + " / " +
                  std::to_string(photoGallery.Count()) + "   >",
                  fontBody, kMuted, 250);
            // Five position markers underneath the focussed photo.
            for (int i = -2; i <= 2; ++i)
                fill(795 + i * 36, 793, 808 + i * 36, 801,
                     i == 0 ? kFocus : kLine);
        } else if (photoNotice.empty()) {
            print(376, 480, "Dieser Ordner enthält keine Fotos",
                  fontLarge, kMuted, 890);
        }
    }

    void drawPhotoGrid()
    {
        // The original grid remains available and uses the same cached
        // image_next thumbnails, now without an unnecessary right panel.
        const int start = photoGallery.firstVisible;
        const int last = std::min(photoGallery.Count(),
                                  start + suiteosd::PhotoGallery::kVisible);
        for (int index = start; index < last; ++index) {
            const int cell = index - start;
            const int x = 41 + (cell % suiteosd::PhotoGallery::kColumns) * 310;
            const int y = 210 + (cell / suiteosd::PhotoGallery::kColumns) * 194;
            const bool focused = index == photoGallery.selected;
            panel(x, y, 291, 176, focused, kCardAlt);
            fill(x + 10, y + 10, x + 280, y + 133, kLine);
            std::string name;
            std::string cover;
            bool folder = true;
            if (photoGallery.InSources()) {
                name = photoGallery.sources[index].title;
            } else {
                const auto &item = photoGallery.entries[index];
                name = item.name;
                cover = item.cover;
                folder = item.directory;
            }
            const auto found = photoThumbnails.find(
                std::to_string(photoGallery.source) + ":" + cover);
            if (!cover.empty() && found != photoThumbnails.end() &&
                found->second.ready && !found->second.argb.empty()) {
                const auto &thumb = found->second;
                cImage picture(cSize(thumb.width, thumb.height), thumb.argb.data());
                osd->DrawImage(cPoint(X(x + 145) - thumb.width / 2,
                                      Y(y + 70) - thumb.height / 2), picture);
            } else
                print(x + 32, y + 53, folder ? "ORDNER" : "FOTO",
                      fontBody, focused ? kFocus : kMuted, 221);
            print(x + 13, y + 144, name, fontSmall, kText, 263);
        }
        if (!photoGallery.Count() && photoNotice.empty())
            print(200, 410, "Keine Bilder vorhanden", fontLarge, kMuted);
    }

    void drawPhotoGallery()
    {
        fill(0, 0, 1599, 899, kBackground);
        fill(0, 0, 1599, 76, kSurface);
        print(32, 23, "VDR Suite", fontLarge, kText, 226);
        print(284, 24, "Bilder", fontLarge, kMuted, 1000);
        print(1390, 24, formatTime(time(nullptr)), fontLarge, kText, 175);

        // The title bar contains only the currently selected folder/source.
        panel(20, 94, 1560, 63, false, kSurface);
        print(76, 105, photoFolderTitle(), fontLarge, kText, 1430);

        if (photoGallery.view == suiteosd::PhotoGallery::View::Carousel)
            drawPhotoCarousel();
        else
            drawPhotoGrid();

        // One compact footer; no sidebar, permanent info panel or menus.
        panel(20, 827, 1560, 66, false, kSurface);
        fill(81, 845, 129, 875, 0xFFE42330);
        print(91, 846, "OK", fontSmall, kText);
        print(151, 846, "Vollbild", fontSmall, kText);
        fill(366, 845, 410, 875, 0xFF138D35);
        print(377, 847, ">", fontSmall, kText);
        print(425, 846, "Diashow", fontSmall, kText);
        fill(682, 845, 722, 875, 0xFFEAC10C);
        print(698, 846, "i", fontSmall, kBackground);
        print(738, 846, "Info", fontSmall, kText);
        fill(909, 845, 949, 875, 0xFF1468D9);
        print(919, 846, "1", fontSmall, kText);
        print(969, 846,
              photoGallery.view == suiteosd::PhotoGallery::View::Carousel ?
              "Raster wählen" : "Karussell wählen", fontSmall, kText, 265);
        print(1328, 846, "Zurück: Ordner", fontSmall, kMuted, 248);

        if (photoInfoVisible) {
            const auto *entry = photoGallery.SelectedEntry();
            if (entry) {
                fill(235, 775, 1390, 814, kSurface);
                print(257, 784, entry->path, fontSmall, kText, 1100);
            }
        }
        if (!photoNotice.empty()) {
            fill(161, 777, 1480, 820, kSurface);
            print(188, 785, photoNotice, fontSmall, kFocus, 1262);
        }
    }

    // Resolve exact native IDs in the existing local VDR recording list.
    bool resolveSearchRecording(const suiteosd::CoreSearchRecording &row, Recording &out)
    {
        if (row.backendId != "default" || row.nativeId.empty()) return false;
        LOCK_RECORDINGS_READ;
        const cRecording *rec = Recordings ? Recordings->GetByName(row.nativeId.c_str()) : nullptr;
        if (!rec) return false;
        out = Recording{};
        out.fileName = rec->FileName() ? rec->FileName() : "";
        out.title = rec->Name() ? rec->Name() : "";
        const cString base = rec->BaseName();
        out.baseName = *base ? *base : out.title;
        out.start = rec->Start();
        out.time = formatTime(out.start);
        if (rec->Info() && rec->Info()->Description()) out.description = rec->Info()->Description();
        return !out.fileName.empty();
    }

    void invalidateSearchRead()
    {
        ++searchGeneration;
        searchQueued=false;
    }
    void launchSearchRead()
    {
        if(searchJob.valid()||!searchQueued)return;
        auto request=std::move(pendingSearch);
        searchQueued=false;
        try {
            searchJob=std::async(std::launch::async,[request=std::move(request)]{
                SearchReadback result;
                result.request=request;
                suiteosd::CoreAdminClient client(request.origin);
                suiteosd::DeviceCredential credential;
                if(!suiteosd::loadDeviceProfile(request.profile,client.origin(),credential)||
                   !client.setDeviceCredential(credential)){
                    result.status={false,0,"Keine Geräteanmeldung – Kopplung in den Plugin-Einstellungen prüfen"};
                }else if(request.detail){
                    result.status=client.publicRecordingDetail(request.recording,result.detail,true);
                    if(result.status.ok)result.nativeId=suiteosd::matchNativeRecording(
                        request.recording,result.detail.nativeMatchProof,request.nativeNames);
                }else if(request.people){
                    result.status=client.publicPeopleSearch("default",request.query,
                        suiteosd::SearchNavigation::pageSize,request.offset,result.results);
                }else{
                    result.status=client.publicSearch("default",request.query,request.epg,
                        suiteosd::SearchNavigation::pageSize,request.offset,result.results);
                }
                return result;
            });
        }catch(const std::exception &){
            searchNotice="Suchauftrag konnte nicht gestartet werden";
        }
    }
    void queueSearchRead(SearchRead request)
    {
        request.origin=gCoreUrl;
        request.profile=deviceProfilePath();
        request.generation=++searchGeneration;
        if(request.detail){
            request.nativeNames.reserve(data.recordings.size());
            for(const auto &rec:data.recordings)request.nativeNames.push_back(rec.fileName);
        }
        pendingSearch=std::move(request);
        searchQueued=true;
        searchNotice=pendingSearch.detail?"Aufnahmedetails werden geladen …":"Suche läuft …";
        launchSearchRead();
    }
    bool finishSearchRead()
    {
        if(!searchJob.valid()||searchJob.wait_for(std::chrono::seconds(0))!=std::future_status::ready)
            return false;
        bool redraw=false;
        try {
            auto result=searchJob.get();
            if(result.request.home&&homeDetailPending&&result.request.generation==searchGeneration){
                homeDetailPending=false;
                if(result.status.ok&&!result.nativeId.empty()){
                    suiteosd::CoreHomeCard card;
                    card.recording=result.request.recording;card.recording.nativeId=result.nativeId;
                    card.position=result.request.position;
                    openHomeRecording(card,result.request.resume);
                }else notice=result.status.ok?"Aufnahme konnte nicht exakt dem lokalen VDR zugeordnet werden":
                    "HTTP "+std::to_string(result.status.http)+" · "+result.status.message;
                redraw=true;
            }else if(searchActive&&result.request.generation==searchGeneration&&
               searchNav.page==suiteosd::SearchPage::Results){
                if(result.request.detail&&result.request.selected!=searchNav.selected){
                    searchNotice.clear();
                }else if(result.status.ok){
                    searchNotice.clear();
                    if(result.request.detail){
                        auto row=result.request.recording;row.nativeId=result.nativeId;
                        if(resolveSearchRecording(row,searchRecording)){
                            detailCastNav.Close();detailScroll=0;searchNav.page=suiteosd::SearchPage::Recording;
                        }else{
                            searchEvent=std::move(result.detail);
                            searchNav.page=suiteosd::SearchPage::PublicRecording;
                        }
                    }else{
                        globalSearch=std::move(result.results);
                        searchNotice=result.status.message;
                    }
                }else{
                    // Preserve the server reason code rather than guessing missing grants.
                    searchNotice=result.status.http>0?
                        "HTTP "+std::to_string(result.status.http)+" · "+result.status.message:
                        result.status.message;
                    if(searchNotice.empty())searchNotice="Suche fehlgeschlagen";
                }
                redraw=true;
            }
        }catch(const std::exception &){
            if(searchActive&&!searchQueued&&searchNav.page==suiteosd::SearchPage::Results){
                searchNotice="Suchantwort konnte nicht verarbeitet werden";
                redraw=true;
            }
        }
        launchSearchRead();
        return redraw;
    }
    void fetchSearch()
    {
        invalidateSearchRead();
        searchNotice.clear();
        searchNav.page=suiteosd::SearchPage::Results;
        personSearch={};
        globalSearch={};
        SearchRead request;
        request.query=searchNav.query;
        request.offset=searchNav.offset;
        request.epg=searchNav.tab==suiteosd::SearchTab::Epg;
        request.people=searchNav.tab==suiteosd::SearchTab::People;
        queueSearchRead(std::move(request));
    }

    void openSearch(const std::string &person = "")
    {
        if (!searchActive) {
            searchOriginCast = detailCastNav;
            searchOriginScroll = detailScroll;
            searchHistory.clear();
        } else {
            searchHistory.push_back({searchNav, personSearch, globalSearch,
                 searchRecording, searchEvent, detailCastNav, detailScroll, searchNotice});
        }
        invalidateSearchRead();
        searchActive = true;
        detailCastNav.Close();
        searchNav.Open(person);
        personSearch = {};
        globalSearch = {};
        searchNotice.clear();
        if (!person.empty()) fetchSearch();
    }

    void closeSearch()
    {
        invalidateSearchRead();
        if (!searchHistory.empty()) {
            auto frame = std::move(searchHistory.back());
            searchHistory.pop_back();
            searchNav = std::move(frame.nav);
            personSearch = std::move(frame.person);
            globalSearch = std::move(frame.global);
            searchRecording = std::move(frame.recording);
            searchEvent = std::move(frame.event);
            detailCastNav = frame.cast;
            detailScroll = frame.scroll;
            searchNotice = std::move(frame.notice);
        } else {
            searchActive = false;
            if(nav.section>=3)loadHomeRail(nav.section);
            nav.KeepVisible(suiteosd::Navigation::kHomeVisibleCards,count(nav.section));
            detailCastNav = searchOriginCast;
            detailScroll = searchOriginScroll;
        }
    }

    const std::vector<suiteosd::CoreSearchRecording> &searchRecordings() const
    {
        return globalSearch.recordings;
    }
    int searchCount() const
    {
        switch (searchNav.tab) {
        case suiteosd::SearchTab::Recordings: return int(searchRecordings().size());
        case suiteosd::SearchTab::Epg: return int(globalSearch.epg.size());
        case suiteosd::SearchTab::People: return int(globalSearch.people.size());
        }
        return 0;
    }
    bool searchHasMore() const
    {
        switch (searchNav.tab) {
        case suiteosd::SearchTab::Recordings:
            return globalSearch.recordingHasMore;
        case suiteosd::SearchTab::Epg: return globalSearch.epgHasMore;
        case suiteosd::SearchTab::People:
            return globalSearch.recordingHasMore || globalSearch.epgHasMore;
        }
        return false;
    }

    void drawSearch()
    {
        using suiteosd::SearchPage;
        using suiteosd::SearchTab;
        if (searchNav.page == SearchPage::Recording) {
            drawRecordingDetailPage(searchRecording);
            if (!searchNotice.empty()) print(58, 784, searchNotice, fontSmall, kFocus, 1480);
            return;
        }
        fill(0, 0, 1599, 899, kBackground);
        fill(0, 0, 1599, 76, kSurface);
        print(34, 23, "VDR Suite", fontLarge, kText, 226);
        print(284, 22, searchNav.person ? "Aufnahmen & EPG zur Person" : "Suche", fontLarge);
        if (searchNav.page == SearchPage::Query) {
            print(154,110,"Aufnahmen, EPG und Personen",fontTitle,kText,1400);
            panel(158,189,1377,63,true,kCard);
            print(180,205,searchNav.query.empty()?"Suchbegriff eingeben":searchNav.query,fontBody,kText,1300);
            for(int i=0;i<int(suiteosd::SearchNavigation::Keys().size());++i){
                int x=158+(i%10)*138,y=286+(i/10)*84;
                panel(x,y,118,70,i==searchNav.key,kCardAlt);
                const auto value=searchNav.KeyLabel(i);
                print(x+40,y+18,value==" "?"␣":value,fontLarge,kText,76);
            }
            print(158,744,"Rot: Suchen · Grün: Leeren",fontBody,kFocus,1360);
            print(158,787,"OK: Zeichen · Gelb: Löschen · Blau: Groß/Klein · Zurück: Abbrechen",
                  fontSmall,kText,1360);
        } else if (searchNav.page == SearchPage::Event||searchNav.page==SearchPage::PublicRecording) {
            print(90, 120, searchEvent.title, fontTitle, kText, 1410);
            print(90, 198, (searchEvent.channelName.empty()?"":searchEvent.channelName+" · ")+searchEvent.subtitle, fontBody, kMuted, 1410);
            print(90,230,searchEvent.startTime,fontSmall,kMuted,1410);
            std::istringstream words(searchEvent.description);
            std::string word, line;
            int y = 267;
            while (words >> word && y < 730) {
                const auto candidate = line.empty() ? word : line + " " + word;
                if (!line.empty() && fontBody && fontBody->Width(candidate.c_str()) > X(1390)) {
                    print(90, y, line, fontBody, kText, 1390);
                    y += 36; line = word;
                } else line = candidate;
            }
            if (y < 730) print(90, y, line, fontBody, kText, 1390);
            print(90, 768, searchNav.page==SearchPage::PublicRecording?
                  "Zurück: Suchergebnisse · Native Wiedergabezuordnung noch nicht verfügbar":"Zurück: Suchergebnisse", fontBody, kFocus,1410);
        } else {
            print(157, 108, searchNav.query, fontTitle, kText, 1350);
            const char *labels[] = {"Aufnahmen", "EPG", "Personen"};
            for (int i = 0; i < (searchNav.person ? 2 : 3); ++i) {
                const int x = 157 + i * 230;
                panel(x, 181, 210, 51, i == int(searchNav.tab), kCardAlt);
                print(x + 16, 193, labels[i], fontBody, kText, 180);
            }
            const int count = searchCount();
            if (searchNav.tab != SearchTab::People) {
                const auto total = searchNav.tab == SearchTab::Epg ? globalSearch.epgTotal :
                    globalSearch.recordingTotal;
                print(1080, 192, globalSearch.status=="ready"?std::to_string(total)+" Treffer":"Keine Ergebnisdaten", fontBody, kMuted, 420);
            }
            const int first = searchNav.First();
            for (int n = 0; n < suiteosd::SearchNavigation::visibleRows && first+n < count; ++n) {
                const int i = first+n, y = 253+n*83;
                panel(157, y, 1380, 73, i == searchNav.selected, kCardAlt);
                std::string title, info;
                if (searchNav.tab == SearchTab::Recordings) {
                    const auto &row = searchRecordings()[i];
                    title = row.title;
                    if(row.backendId=="default")if(const auto *local=findRecording(row.nativeId)){
                        const auto &meta=metaFor(*local);
                        if(!meta.title.empty())title=meta.title;
                        if(!meta.episode.empty()&&meta.episode!=title)title+=" – "+meta.episode;
                    }
                    info = row.character.empty() ? row.subtitle : row.person + " · " + row.character;
                } else if (searchNav.tab == SearchTab::Epg) {
                    const auto &row = globalSearch.epg[i];
                    title = row.title;
                    info = row.channelName + " · " + row.subtitle;
                } else {
                    const auto &row = globalSearch.people[i];
                    title = row.name;
                    info = row.role + " · " + std::to_string(row.recordingCount) + " Aufnahmen · " +
                        std::to_string(row.epgCount) + " EPG (diese Seite)";
                }
                print(180, y+7, title, fontBody, kText, 1320);
                print(180, y+38, info, fontSmall, kMuted, 1320);
            }
            if (!count && searchNotice.empty())
                print(180, 335, "Keine Treffer in dieser Ansicht", fontBody, kMuted);
            print(157, 768, "OK: Öffnen · Grün: Treffergruppe · Rot: Neue Suche",
                  fontSmall, kFocus, 1370);
            print(157, 796, "Gelb/Blau: Vorherige/Nächste Seite · Pfeile: Auswahl · Zurück",
                  fontSmall, kText, 1370);
        }
        if (!searchNotice.empty()) {
            fill(155, 837, 1538, 891, kSurface);
            print(175, 850, searchNotice, fontSmall, kFocus, 1340);
        }
        if (searchNav.page != SearchPage::Event&&searchNav.page!=SearchPage::PublicRecording) drawSidebar();
    }

    eOSState searchKey(eKeys base)
    {
        using suiteosd::SearchPage;
        using suiteosd::SearchTab;
        if (searchNav.page == SearchPage::Query) {
            if (base == kBack) closeSearch();
            else if (base == kRed) {
                searchNav.offset = searchNav.selected = 0;
                searchNav.person = false;
                searchNav.tab = SearchTab::Recordings;
                fetchSearch();
            } else if (base == kGreen) searchNav.query.clear();
            else if (base == kYellow) searchNav.Erase();
            else if (base == kBlue) searchNav.uppercase = !searchNav.uppercase;
            else if (base == kOk) searchNav.Type();
            else if (base == kUp) searchNav.MoveKey(0,-1);
            else if (base == kDown) searchNav.MoveKey(0,1);
            else if (base == kLeft) searchNav.MoveKey(-1,0);
            else if (base == kRight) searchNav.MoveKey(1,0);
            else if (base >= k0 && base <= k9) searchNav.query += char('0' + int(base-k0));
        } else if (searchNav.page == SearchPage::Recording) {
            if (handleDetailCast(base, &searchRecording)) return osContinue;
            if (base == kBack || base == kInfo) searchNav.page = SearchPage::Results;
            else if (base == kOk || base == kRed || base == kPlay) {
                bool present = false;
                {
                    LOCK_RECORDINGS_READ;
                    present = Recordings && Recordings->GetByName(searchRecording.fileName.c_str());
                }
                if (present) {
                    restoreLivePreview();
                    cReplayControl::SetRecording(searchRecording.fileName.c_str());
                    cControl::Launch(new cSuiteReplayControl());
                    return osEnd;
                }
                searchNotice = "Aufnahme nicht mehr im lokalen VDR vorhanden";
            }
        } else if (searchNav.page == SearchPage::Event||searchNav.page==SearchPage::PublicRecording) {
            if (base == kBack || base == kInfo || base == kOk)
                searchNav.page = SearchPage::Results;
        } else {
            if (base == kBack) closeSearch();
            else if (base == kRed) {
                invalidateSearchRead();
                searchNav.Open();
                searchNotice.clear();
            } else if (base == kGreen) {
                searchNav.CycleTab(); fetchSearch();
            } else if (base == kYellow && searchNav.offset > 0) {
                searchNav.PreviousPage(); fetchSearch();
            } else if (base == kBlue && searchHasMore()) {
                searchNav.NextPage(); fetchSearch();
            } else if (base == kUp) searchNav.Move(-1, searchCount());
            else if (base == kDown) searchNav.Move(1, searchCount());
            else if (base == kLeft) searchNav.Move(-suiteosd::SearchNavigation::visibleRows, searchCount());
            else if (base == kRight) searchNav.Move(suiteosd::SearchNavigation::visibleRows, searchCount());
            else if (base == kOk && searchNav.selected < searchCount()) {
                searchNotice.clear();
                const int i = searchNav.selected;
                if (searchNav.tab == SearchTab::Recordings) {
                    if (resolveSearchRecording(searchRecordings()[i], searchRecording)) {
                        detailCastNav.Close(); detailScroll=0; searchNav.page = SearchPage::Recording;
                    } else {
                        SearchRead request;
                        request.detail=true;
                        request.recording=searchRecordings()[i];
                        request.selected=i;
                        queueSearchRead(std::move(request));
                    }
                } else if (searchNav.tab == SearchTab::Epg) {
                    searchEvent = globalSearch.epg[i]; searchNav.page = SearchPage::Event;
                } else {
                    const std::string name = globalSearch.people[i].name;
                    openSearch(name);
                }
            }
        }
        if (searchFromReplay && !searchActive) return osEnd;
        draw();
        return osContinue;
    }

    std::string homeRailTitle(int section) const {
        switch(section){
        case suiteosd::Navigation::kContinue:return "Weiterschauen";
        case suiteosd::Navigation::kNew:return "Neu aufgenommen";
        case suiteosd::Navigation::kGenres:return "Genres";
        case suiteosd::Navigation::kFolders:return "Aufnahmeordner";
        case suiteosd::Navigation::kRecommendation:return recommendedGenreTitle.empty()?"Genre-Empfehlungen":recommendedGenreTitle;
        case suiteosd::Navigation::kMovies:return "Filme der letzten 5 Jahre";
        case suiteosd::Navigation::kRecent:return "Zuletzt angesehen";
        default:return "";
        }
    }
    static suiteosd::HomeKind homeRailKind(int section) {
        using suiteosd::HomeKind;
        switch(section){
        case suiteosd::Navigation::kContinue:return HomeKind::Continue;
        case suiteosd::Navigation::kGenres:return HomeKind::Genres;
        case suiteosd::Navigation::kFolders:return HomeKind::Folders;
        case suiteosd::Navigation::kRecommendation:return HomeKind::Genre;
        case suiteosd::Navigation::kMovies:return HomeKind::Movies;
        case suiteosd::Navigation::kRecent:return HomeKind::Recent;
        default:return HomeKind::New;
        }
    }
    int currentYear() const {
        const auto now=time(nullptr);tm t{};return localtime_r(&now,&t)?1900+t.tm_year:2026;
    }
    void startHomeEvents(){
        const std::string origin=gCoreUrl,path=deviceProfilePath();
        suiteosd::DeviceCredential saved;
        if(!suiteosd::loadDeviceProfile(path,origin,saved))return;
        auto subscribe=[this,origin,path](const std::string &domain,std::atomic<bool> &changed){
            while(!homeEventStop.load()){
                suiteosd::DeviceCredential credential;
                suiteosd::CoreAdminClient client(origin);
                if(!suiteosd::loadDeviceProfile(path,origin,credential)||!client.setDeviceCredential(credential))break;
                client.recordingEvents("default",homeEventStop,changed,domain);
                // Reconnect transport only; no snapshot/status polling.
                for(int i=0;i<30&&!homeEventStop.load();++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        };
        try{
            homeEventJob=std::async(std::launch::async,[this,subscribe]{subscribe("recordings",homeEventChanged);});
            homeProgressJob=std::async(std::launch::async,[this,subscribe]{subscribe("continueWatching",homeProgressChanged);});
        }catch(const std::exception &){}
    }
    bool nativeHomePage(suiteosd::HomeKind kind,const std::string &key,int offset,
                        suiteosd::CoreHomePage &page) {
        using suiteosd::HomeKind;
        using Card=suiteosd::CoreHomeCard;
        std::vector<Card> cards;
        auto recordingCard=[](const Recording &rec){
            Card card;
            card.key=rec.fileName;card.title=rec.baseName;
            card.recording.backendId="default";card.recording.nativeId=rec.fileName;
            card.subtitle=recordingDate(rec.start);return card;
        };
        if(kind==HomeKind::Continue){
            LOCK_RECORDINGS_READ;
            if(Recordings)for(const auto &rec:data.recordings){
                const auto *r=Recordings->GetByName(rec.fileName.c_str());
                if(!r)continue;
                const int resume=r->GetResume();const double fps=r->FramesPerSecond();
                if(resume<=0||fps<=0)continue;
                const int duration=r->LengthInSeconds();
                const auto position=static_cast<std::int64_t>(resume/fps);
                if(position<=0||(duration>0&&position>=duration))continue;
                auto card=recordingCard(rec);
                card.position=position;card.duration=duration>0?duration:-1;
                cards.push_back(std::move(card));
            }
        }else if(kind==HomeKind::New){
            for(const auto &rec:data.recordings)cards.push_back(recordingCard(rec));
        }else if(kind==HomeKind::Folders||kind==HomeKind::Root){
            suiteosd::RecordingBrowser browser;
            browser.view=suiteosd::RecordingBrowser::View::Folders;
            browser.folder=key;
            for(const auto &rec:data.recordings){
                suiteosd::BrowserRecording row;
                row.fileName=rec.fileName;row.name=rec.title;row.title=rec.baseName;row.start=rec.start;
                browser.source.push_back(std::move(row));
            }
            browser.Refresh();
            int direct=0;
            for(const auto &entry:browser.entries){
                if(entry.kind==suiteosd::BrowserEntry::Kind::Recording){
                    if(kind==HomeKind::Folders&&key.empty()){++direct;continue;}
                    if(const auto *rec=findRecording(entry.fileName))cards.push_back(recordingCard(*rec));
                }else if(kind!=HomeKind::Root){
                    Card card;
                    card.kind=entry.kind==suiteosd::BrowserEntry::Kind::Genre?Card::Kind::Genre:Card::Kind::Folder;
                    card.key=entry.path;card.title=entry.title;card.count=entry.count;
                    card.subtitle=std::to_string(entry.count)+" Aufnahmen";cards.push_back(std::move(card));
                }
            }
            if(direct){
                Card card;card.kind=Card::Kind::Root;card.key="root";
                card.title="Hauptverzeichnis";card.count=direct;
                card.subtitle=std::to_string(direct)+" Aufnahmen";cards.insert(cards.begin(),std::move(card));
            }
        }else return false;
        page={};page.total=cards.size();
        const int first=std::clamp(offset,0,int(cards.size()));
        const int end=(kind==HomeKind::Genres||(kind==HomeKind::Folders&&key.empty()))?int(cards.size()):std::min(first+12,int(cards.size()));
        for(int i=first;i<end;++i)page.cards.push_back(std::move(cards[i]));
        page.nextOffset=end;page.hasMore=end<int(cards.size());return true;
    }
    void launchHomeRead(){
        if(homeReadJob.valid()||homeReadQueue.empty())return;
        auto request=std::move(homeReadQueue.front());homeReadQueue.pop_front();
        homeReadJob=std::async(std::launch::async,[request]{
            HomeReadback result;result.request=request;
            suiteosd::CoreAdminClient client(request.origin);
            suiteosd::DeviceCredential saved;
            if(!suiteosd::loadDeviceProfile(request.profile,client.origin(),saved)||!client.setDeviceCredential(saved)){
                result.status={false,0,"Keine Geräteanmeldung"};
                return result;
            }
            result.status=client.publicHomePage("default",request.kind,request.key,12,
                request.offset,request.year,result.page);
            return result;
        });
    }
    void queueHomeRead(HomeRail &rail,int section,suiteosd::HomeKind kind,const std::string &key,int offset){
        rail.attempted=true;rail.loading=true; // keep the existing projection visible during normal readback
        rail.requestId=++homeReadSerial;
        HomeRead request;request.id=rail.requestId;request.section=section;
        request.kind=kind;request.key=key;request.offset=offset;request.year=currentYear();
        request.origin=gCoreUrl;request.profile=deviceProfilePath();
        if(section>=0)homeReadQueue.erase(std::remove_if(homeReadQueue.begin(),homeReadQueue.end(),
            [&](const auto &old){return old.section==section;}),homeReadQueue.end());
        homeReadQueue.push_back(std::move(request));launchHomeRead();
    }
    void loadHomeRail(int section,bool force=false) {
        if(section<3)return;
        auto &rail=homeRails[section];
        if(rail.loading||(rail.attempted&&!force))return;
        std::string key;
        if(section==suiteosd::Navigation::kRecommendation){
            loadHomeRail(suiteosd::Navigation::kGenres);
            const auto &genres=homeRails[suiteosd::Navigation::kGenres];
            if(genres.loading)return;
            if(!genres.status.ok||genres.page.cards.empty()){rail.attempted=true;rail.status=genres.status;return;}
            if(recommendedGenreId.empty()){
                const auto &genre=genres.page.cards[size_t(std::rand())%genres.page.cards.size()];
                recommendedGenreId=genre.key;recommendedGenreTitle=genre.title;
            }
            key=recommendedGenreId;
        }
        queueHomeRead(rail,section,homeRailKind(section),key,0);
    }
    // Queue every visible rail immediately; a pending recommendation
    // dependency must not starve unrelated rows further down.
    bool loadVisibleHomeRails(bool redraw=false){
        if(!homeCollections.empty())return false;
        bool changed=false;
        const int first=nav.firstVisibleSection;
        for(int section=std::max(3,first);section<first+HL::rows;++section){
            const auto it=homeRails.find(section);
            if(it!=homeRails.end()&&it->second.attempted)continue;
            const bool needsPlaceholder=it==homeRails.end()||!it->second.status.ok;
            loadHomeRail(section);
            const auto loaded=homeRails.find(section);
            if(redraw&&needsPlaceholder&&loaded!=homeRails.end()&&loaded->second.attempted){
                if(section==nav.section)nav.KeepVisible(HL::columns,count(section));
                redrawHomeRail(section,first,true);
                changed=true;
            }
        }
        return changed;
    }
    void loadHomeCollection(HomeCollection &collection) {
        collection.rail.page={};collection.rail.status={};
        collection.selected=collection.first=0;
        queueHomeRead(collection.rail,-1,collection.kind,collection.key,collection.offset);
    }
    bool finishHomeRead(){
        if(!homeReadJob.valid()||homeReadJob.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
        auto result=homeReadJob.get();
        HomeRail *rail=nullptr;int *selected=nullptr,*first=nullptr;
        if(result.request.section>=0){
            auto it=homeRails.find(result.request.section);
            if(it!=homeRails.end()&&it->second.requestId==result.request.id){
                rail=&it->second;selected=&nav.card[result.request.section];first=&nav.firstCard[result.request.section];
            }
        }else for(auto &collection:homeCollections)if(collection.rail.requestId==result.request.id){
            rail=&collection.rail;selected=&collection.selected;first=&collection.first;break;
        }
        bool changed=false;
        if(rail){
            rail->loading=false;
            if(!result.status.ok&&result.status.http==0&&result.status.message=="Keine Geräteanmeldung"&&
                nativeHomePage(result.request.kind,result.request.key,result.request.offset,result.page))
                result.status={true,0,"Lokaler VDR"};
            if(result.status.ok){
                changed=!rail->status.ok||rail->stale||!suiteosd::SameHomePage(rail->page,result.page);
                if(!changed){rail->status=result.status;launchHomeRead();return false;}
                auto preserve=[&](int index){
                    return index>=0&&index<int(rail->page.cards.size())?rail->page.cards[index].key:std::string{};
                };
                const auto selection=preserve(*selected),scroll=preserve(*first);
                rail->page=std::move(result.page);rail->status=result.status;rail->stale=false;
                auto locate=[&](const std::string &key,int old){
                    for(int i=0;i<int(rail->page.cards.size());++i)if(rail->page.cards[i].key==key)return i;
                    return std::clamp(old,0,std::max(0,int(rail->page.cards.size())-1));
                };
                *selected=locate(selection,*selected);*first=locate(scroll,*first);
                if(*selected<*first)*first=*selected;
                if(*selected>=*first+HL::columns)*first=*selected-HL::columns+1;
            }else if(rail->status.ok&&result.status.http!=401&&result.status.http!=403){
                changed=!rail->stale;rail->stale=true;
            }else{
                changed=true;rail->page={};rail->status=result.status;rail->stale=false;*selected=*first=0;
            }
            if(changed&&result.request.section==suiteosd::Navigation::kGenres){
                auto it=homeRails.find(suiteosd::Navigation::kRecommendation);
                if(it!=homeRails.end()&&!it->second.loading)it->second.attempted=false;
            }
        }
        launchHomeRead();
        if(!changed||!rail)return false;
        if(!homeCollections.empty())return rail==&homeCollections.back().rail;
        return result.request.section>=nav.firstVisibleSection&&
               result.request.section<nav.firstVisibleSection+HL::rows;
    }
    static std::string playbackClock(std::int64_t seconds) {
        if(seconds<0)return "";
        char buf[64];std::snprintf(buf,sizeof(buf),"%lld:%02lld:%02lld",
          static_cast<long long>(seconds/3600),static_cast<long long>((seconds/60)%60),static_cast<long long>(seconds%60));
        return buf;
    }
    static std::string homePosterKey(const suiteosd::CoreSearchRecording &row){
        return row.backendId+"\n"+row.id;
    }
    void invalidatePublicHomePosters(){
        ++homePosterGeneration;++homePosterRevision;cinematicComposites.clear();publicHomePosters.clear();
    }
    const HomePosterRead *publicHomePoster(const suiteosd::CoreSearchRecording &row) {
        auto it=publicHomePosters.find(homePosterKey(row));
        return it==publicHomePosters.end()?nullptr:&it->second;
    }
    void scheduleVisibleHomePoster(){
        if(searchActive||photosActive||settingsActive||recordingsActive||
           timelineActive||recordingDetails||homeProgrammeDetails)return;
        while(homePosterJobs.size()<suiteosd::kHomePosterWorkers){
        const suiteosd::CoreSearchRecording *candidate=nullptr,*stale=nullptr;
        auto scan=[&](const HomeRail &rail,int first){
            if(!rail.status.ok||candidate)return;
            for(int i=first;i<first+HL::visibleColumns(nav.sideFocused)&&i<int(rail.page.cards.size());++i){
                const auto &card=rail.page.cards[i];
                if(card.kind!=suiteosd::CoreHomeCard::Kind::Recording||
                   card.recording.id.rfind("rec_",0)!=0||
                   homePosterJobs.count(homePosterKey(card.recording)))continue;
                const auto *cached=publicHomePoster(card.recording);
                if(!cached){candidate=&card.recording;return;}
                if(!stale&&cached->revision!=homePosterRevision)stale=&card.recording;
            }
        };
        if(!homeCollections.empty())scan(homeCollections.back().rail,homeCollections.back().first);
        else {
            const int first=nav.firstVisibleSection;
            for(int section=first;section<first+HL::rows;++section){
                auto it=homeRails.find(section);
                if(it!=homeRails.end())scan(it->second,nav.firstCard[section]);
            }
            // Prioritize never-loaded posters over refreshing still-visible
            // cached pixels. Adjacent rails are only considered after visible
            // misses, and one slot remains free when only prefetching.
            if(!candidate&&homePosterJobs.size()<suiteosd::kHomePosterWorkers-1)for(int section:{first+HL::rows,first-1}){
                auto it=homeRails.find(section);
                if(it!=homeRails.end())scan(it->second,nav.firstCard[section]);
                if(candidate)break;
            }
        }
        if(!candidate)candidate=stale;
        if(!candidate)return;
        const auto row=*candidate;
        const std::string origin=gCoreUrl,profile=deviceProfilePath();
        const auto generation=homePosterGeneration,revision=homePosterRevision;
        std::string knownAsset;
        if(const auto *cached=publicHomePoster(row))
            if(cached->poster.valid()&&std::chrono::steady_clock::now()-cached->fetched<std::chrono::seconds(300))
                knownAsset=cached->assetId;
        const int w=X(HL::posterW),h=Y(HL::posterH);
        std::vector<std::string> names;
        if(row.backendId=="default")for(const auto &rec:data.recordings)names.push_back(rec.fileName);
        homePosterJobs.emplace(homePosterKey(row),std::async(std::launch::async,[row,origin,profile,generation,revision,w,h,knownAsset,names=std::move(names)]{
            HomePosterRead result;result.key=homePosterKey(row);result.generation=generation;result.revision=revision;
            suiteosd::CoreAdminClient client(origin);suiteosd::DeviceCredential saved;
            if(!suiteosd::loadDeviceProfile(profile,origin,saved)||!client.setDeviceCredential(saved))return result;
            result.detailStatus=client.publicRecordingDetail(row,result.detail,!names.empty());
            if(result.detailStatus.ok)
                result.nativeId=suiteosd::matchNativeRecording(row,result.detail.nativeMatchProof,names);
            std::string bytes;
            result.artStatus=client.publicRecordingArtwork(row,bytes,&result.assetId,knownAsset);
            result.reused=result.artStatus.ok&&!knownAsset.empty()&&result.assetId==knownAsset&&bytes.empty();
            if(result.artStatus.ok&&!bytes.empty()){
                result.poster=suiteosd::CoverHomePoster(suiteosd::LoadArtworkBytes(bytes,std::min(1024,w*2),std::min(1024,h*2)),w,h,0);
                result.fetched=std::chrono::steady_clock::now();
            }
            return result;
        }));
        }
    }
    std::vector<std::string> finishHomePosters(){
        std::vector<std::string> changed;
        for(auto it=homePosterJobs.begin();it!=homePosterJobs.end();){
            if(it->second.wait_for(std::chrono::seconds(0))!=std::future_status::ready){++it;continue;}
            auto result=it->second.get();
            it=homePosterJobs.erase(it);
            auto key=applyHomePoster(std::move(result));
            if(!key.empty())changed.push_back(std::move(key));
        }
        return changed;
    }
    std::string applyHomePoster(HomePosterRead result){
        if(result.generation!=homePosterGeneration)return {};
        const auto key=result.key;
        auto old=publicHomePosters.find(key);
        const bool denied=result.detailStatus.http==401||result.detailStatus.http==403||
                          result.artStatus.http==401||result.artStatus.http==403;
        if(old==publicHomePosters.end()){
            if(publicHomePosters.size()>=homePosterCapacity())
                publicHomePosters.evictOldest([&](const auto &,const auto &entry){forgetComposite(entry.poster);});
            if(denied){result.poster={};result.detail={};result.nativeId.clear();result.assetId.clear();}
            publicHomePosters.emplace(key,std::move(result));return key;
        }
        auto &cached=old->second;
        cached.revision=result.revision;
        bool changed=false;
        if(denied){
            forgetComposite(cached.poster);cached.poster={};cached.detail={};
            cached.nativeId.clear();cached.assetId.clear();cached.fetched={};
            return key;
        }
        if(result.detailStatus.ok){
            changed=cached.nativeId!=result.nativeId||cached.detail.title!=result.detail.title||
                cached.detail.subtitle!=result.detail.subtitle||cached.detail.description!=result.detail.description||
                cached.detail.startTime!=result.detail.startTime;
            cached.nativeId=std::move(result.nativeId);cached.detail=std::move(result.detail);
        }
        // Keep successful pixels until a valid replacement is decoded.
        // A reconnect or a transient error is not a new empty poster.
        if(result.artStatus.ok&&!result.reused){
            if(result.poster.valid()){
                if(cached.poster.width!=result.poster.width||cached.poster.height!=result.poster.height||
                   cached.poster.argb!=result.poster.argb){
                    forgetComposite(cached.poster);cached.poster=std::move(result.poster);changed=true;
                }
                cached.assetId=std::move(result.assetId);cached.fetched=result.fetched;
            }else if(result.assetId.empty()&&cached.poster.valid()){
                // An authoritative empty manifest means the image was removed.
                forgetComposite(cached.poster);cached.poster={};cached.assetId.clear();cached.fetched={};changed=true;
            }
        }
        return changed?key:std::string{};
    }
    void drawCoreCards(const HomeRail &rail,int first,int selected,int y,bool focus,bool moreCard=false) {
        if(!rail.status.ok){
            print(homeLeft(),y+58,(!rail.attempted||rail.loading)?"Wird geladen …":
                rail.status.message.empty()?"Daten vorübergehend nicht verfügbar":rail.status.message,fontSmall,kMuted,940);
            return;
        }
        if(rail.page.cards.empty()&&!(moreCard&&rail.page.hasMore)){
            print(homeLeft(),y+65,"Keine Einträge vorhanden",fontBody,kMuted);return;
        }
        const int size=int(rail.page.cards.size());
        for(int col=0;col<HL::visibleColumns(nav.sideFocused)&&first+col<size+(moreCard&&rail.page.hasMore?1:0);++col){
            const int index=first+col,x=homeCardX(col),top=y+HL::titleHeight;
            if(index==size){
                portraitCard(x,top,focus&&index==selected);
                print(x+12,top+65,"Weitere",fontBody,kText,HL::cardWidth-24);
                print(x+12,top+95,"Aufnahmen",fontSmall,kText,HL::cardWidth-24);
                print(x+12,top+HL::cardHeight-30,"OK: Öffnen",fontSmall,kFocus,HL::cardWidth-24);
                continue;
            }
            const auto &card=rail.page.cards[index];
            if(card.kind==suiteosd::CoreHomeCard::Kind::Recording){
                const auto *loaded=publicHomePoster(card.recording);
                const auto *local=findRecording(loaded&&!loaded->nativeId.empty()?loaded->nativeId:card.recording.nativeId);
                const bool native=local&&card.recording.backendId=="default";
                const auto *poster=loaded&&loaded->poster.valid()?&loaded->poster:
                    native?&recordingPoster(*local,HL::posterW,HL::posterH):nullptr;
                portraitCard(x,top,focus&&index==selected,poster);
                const std::string subtitle=card.completed?"Angesehen":card.position>=0?playbackClock(card.position):std::string{};
                const std::string title=loaded&&!loaded->detail.title.empty()?loaded->detail.title:
                    native?homeRecordingTitle(*local,card.title):card.title;
                portraitTitle(x,top,title,subtitle,loaded&&!loaded->detail.subtitle.empty()?loaded->detail.subtitle:
                              native?homeRecordingEpisode(*local):std::string{});
                if(card.duration>0&&card.position>=0){
                    cinemaProgress(x+9,top+HL::cardHeight-9,HL::cardWidth-18,int(100*std::clamp(double(card.position)/card.duration,0.0,1.0)));
                }
            }else{
                portraitCard(x,top,focus&&index==selected);
                printFit(x+12,top+60,card.title,fontBody,kText,HL::cardWidth-24);
                printFit(x+12,top+98,card.subtitle,fontSmall,kMuted,HL::cardWidth-24);
            }
        }
    }
    void drawHomeRail(int section,int y) {
        if(section==0){drawHomeHero(y);return;}
        if(section==1){drawPrograms(1,y,"Was läuft jetzt",data.now);return;}
        if(section==2){drawPrograms(2,y,"Was läuft als nächstes",data.next);return;}
        auto it=homeRails.find(section);
        if(section==suiteosd::Navigation::kNew&&(it==homeRails.end()||!it->second.status.ok)){
            drawRecordings(y);return;
        }
        cinematicHeading(y,homeRailTitle(section));
        if(it!=homeRails.end()&&it->second.stale)
            print(homeLeft()+520,y+4,"Wird aktualisiert …",fontCinemaMeta,kMuted,440);
        static const HomeRail empty;
        drawCoreCards(it==homeRails.end()?empty:it->second,nav.firstCard[section],nav.card[section],y,
            !nav.sideFocused&&nav.section==section,true);
    }
    bool openHomeRecording(const suiteosd::CoreHomeCard &card,bool resume) {
        if(card.recording.nativeId.empty()&&card.recording.id.rfind("rec_",0)==0){
            SearchRead request;request.detail=true;request.home=true;request.resume=resume;
            request.position=card.position;request.recording=card.recording;
            homeDetailPending=true;notice="Aufnahmedetails werden geladen …";
            queueSearchRead(std::move(request));return false;
        }
        if(!resolveSearchRecording(card.recording,homeDetailRecording)){
            notice="Öffentliche Aufnahme-ID noch nicht nativ zugeordnet";return false;
        }
        detailCastNav.Close();detailScroll=0;recordingDetails=true;
        homeResumeDetail=resume;homeResumeSeconds=resume?card.position:-1;
        notice.clear();return true;
    }
    eOSState playHomeRecording(bool fromStart=false) {
        const auto path=homeDetailRecording.fileName;
        bool exists=false,pes=false;double fps=0;int frames=-1;
        {
            LOCK_RECORDINGS_READ;
            const auto *r=Recordings?Recordings->GetByName(path.c_str()):nullptr;
            if(r){exists=true;pes=r->IsPesRecording();fps=r->FramesPerSecond();frames=r->NumFrames();}
        }
        if(!exists){notice="Aufnahme nicht mehr vorhanden";return osContinue;}
        if(fromStart||homeResumeSeconds>=0){
            cResumeFile resume(path.c_str(),pes);
            // VDR's own resume file, written only on an explicit playback action.
            if(fromStart)resume.Delete();
            else if(fps>0){
                const double position=std::max(0.0,double(homeResumeSeconds)*fps);
                const int target=int(std::min(position,frames>0?double(frames-1):double(2147483647)));
                if(!resume.Save(target)){notice="Wiedergabeposition konnte nicht übernommen werden";return osContinue;}
            }
        }
        restoreLivePreview();
        cReplayControl::SetRecording(path.c_str());
        cControl::Launch(new cSuiteReplayControl());
        return osEnd;
    }
    void openHomeCard(const suiteosd::CoreHomeCard &card,bool resume=false){
        using Kind=suiteosd::CoreHomeCard::Kind;using suiteosd::HomeKind;
        if(card.kind==Kind::Recording){openHomeRecording(card,resume);return;}
        HomeCollection collection;
        collection.kind=card.kind==Kind::Genre?HomeKind::Genre:
             card.kind==Kind::Root?HomeKind::Root:HomeKind::Folders;
        collection.key=card.kind==Kind::Root?"":card.key;collection.title=card.title;
        loadHomeCollection(collection);homeCollections.push_back(std::move(collection));
    }
    eOSState activateHomeRail(){
        const int s=nav.section;
        loadHomeRail(s);
        auto &rail=homeRails[s];
        if(!rail.status.ok){
            return osContinue;
        }else if(nav.card[s]<int(rail.page.cards.size())){
            const auto card=rail.page.cards[nav.card[s]];
            openHomeCard(card,s==suiteosd::Navigation::kContinue);
            if(rail.status.http==0&&s==suiteosd::Navigation::kContinue)homeResumeSeconds=-1;
        }else if(rail.page.hasMore){
            HomeCollection c;
            c.kind=homeRailKind(s);c.title=homeRailTitle(s);
            if(s==suiteosd::Navigation::kRecommendation)c.key=recommendedGenreId;
            loadHomeCollection(c);homeCollections.push_back(std::move(c));
        }
        return osContinue;
    }
    void drawHomeCollection(){
        const auto &c=homeCollections.back();
        print(homeLeft(),96,c.title,fontTitle,kText,985);
        drawCoreCards(c.rail,c.first,c.selected,190,true);
        print(155,600,"Links/Rechts: Auswahl · OK: Öffnen · Zurück",fontBody,kFocus,985);
        print(155,650,"Gelb: Vorherige Seite · Blau: Nächste Seite",fontBody,kText,985);
        if(!notice.empty())print(155,725,notice,fontSmall,kFocus,1390);
        drawCinematicSidebar();
    }
    eOSState homeCollectionKey(eKeys key){
        auto &c=homeCollections.back();notice.clear();
        if(key==kBack)homeCollections.pop_back();
        else if(key==kOk||key==kInfo){
            if(!c.rail.status.ok){return osContinue;}
            else if(c.selected<int(c.rail.page.cards.size())){
                const auto card=c.rail.page.cards[c.selected];openHomeCard(card);
            }
        }else if(key==kBlue&&c.rail.page.hasMore){
            c.previous.push_back(c.offset);c.offset=c.rail.page.nextOffset;loadHomeCollection(c);
        }else if(key==kYellow&&!c.previous.empty()){
            c.offset=c.previous.back();c.previous.pop_back();loadHomeCollection(c);
        }else if(key==kLeft||key==kRight){
            c.selected=std::clamp(c.selected+(key==kLeft?-1:1),0,std::max(0,int(c.rail.page.cards.size())-1));
            if(c.selected<c.first)c.first=c.selected;
            if(c.selected>=c.first+suiteosd::Navigation::kHomeVisibleCards)c.first=c.selected-suiteosd::Navigation::kHomeVisibleCards+1;
        }
        draw();return osContinue;
    }

    void drawSidebar()
    {
        // Expanded sidebar overlays the content; it never permanently
        // consumes additional space. All descriptions appear together.
        const int right = nav.sideFocused ? 270 : 99;
        panel(18, 90, right - 18, 726, false, kSurface);
        for (int i = 0; i < int(kSidebar.size()); ++i) {
            const int y = 98 + i * 54;
            const bool selected = nav.sideFocused && nav.sideIndex == i;
            if (selected)
                panel(31, y, right - 51, 48, true, kHighlight);
            else if (!nav.sideFocused && i ==
                     (searchActive ? 11 : photosActive ? 8 : settingsActive ? 10 : timelineActive ? 2 : recordingsActive ? 3 : 0))
                panel(31, y, 52, 48, false, kHighlight);
            suiteosd::DrawSidebarIcon(i,[&](int px,int py){
                fill(42+px,y+12+py,43+px,y+13+py,kText);
            });
            if (nav.sideFocused)
                print(102, y + 11, kSidebar[i], fontBody, kText, 145);
        }
    }

    void drawCinematicSidebar()
    {
        const int right=cinematicSidebarWidth();
        for(int i=0;i<int(kSidebar.size());++i){
            const int y=112+i*55;
            const bool selected=nav.sideFocused&&nav.sideIndex==i;
            const bool active=selected||(!nav.sideFocused&&i==0);
            const suiteosd::CinemaPatch *frame=nullptr;
            if(active){
                frame=&cinemaFrame(19,y,right-38,48,13,suiteosd::CinemaStyle::Sidebar);
                drawCinemaImage(frame->image,frame->x,frame->y);
            }
            const std::array<int,4> key{{i,int(nav.sideFocused),int(active),width}};
            auto glyph=cinematicGlyphs.find(key);
            if(glyph==cinematicGlyphs.end())
                glyph=cinematicGlyphs.emplace(key,suiteosd::CinematicGlyphPatch(
                    cinematicBackground(),frame,X(43),Y(y+9),std::max(1,X(30)),i)).first;
            drawCinemaImage(glyph->second.image,glyph->second.x,glyph->second.y);
            if(nav.sideFocused)print(96,y+11,kSidebar[i],fontBody,kText,right-104);
        }
    }
    void redrawHomeRail(int section,int first,bool clearRow)
    {
        const int row=section-first;
        if(row<0||row>=HL::rows)return;
        if(clearRow){
            const auto &bg=cinematicBackground();
            const int x0=X(homeLeft()-12),x1=X(HL::right-9);
            const int y0=Y(HL::headingY(row)),y1=Y(HL::headingY(row+1));
            suiteosd::Artwork strip{x1-x0,y1-y0,std::vector<uint32_t>(size_t(x1-x0)*(y1-y0))};
            for(int y=y0;y<y1;++y)
                std::copy_n(bg.argb.begin()+size_t(y)*width+x0,strip.width,
                            strip.argb.begin()+size_t(y-y0)*strip.width);
            drawCinemaImage(strip,x0,y0);
        }
        drawHomeRail(section,HL::headingY(row));
    }
    // Slide only the hero rail; the decoder stays on the tuned channel.
    // During the transition both compositions are opaque, then the true
    // video cutout is restored only for the tuned selection.
    bool animateHeroChannels(int previous,bool forward){
        if(!osd||previous==nav.card[0])return false;
        const int x0=X(homeLeft()-12),x1=X(HL::right-9);
        const int y0=Y(HL::top),y1=Y(HL::headingY(1));
        const int w=x1-x0,h=y1-y0;
        if(w<=0||h<=0)return false;
        auto *strip=osd->CreatePixmap(1,cRect(x0,y0,w,h),cRect(0,0,2*w,h));
        if(!strip)return false;
        const auto &bg=cinematicBackground();
        suiteosd::Artwork patch{w,h,std::vector<uint32_t>(size_t(w)*h)};
        for(int y=0;y<h;++y)
            std::copy_n(bg.argb.begin()+size_t(y0+y)*width+x0,w,patch.argb.begin()+size_t(y)*w);
        const int selected=nav.card[0],from=forward?0:w,to=forward?w:0;
        railDrawPixmap=strip;railDrawY=-y0;
        for(int i=0;i<2;++i){
            railDrawX=i*w-x0;
            nav.card[0]=i*w==from?previous:selected;
            drawCinemaImage(patch,x0,y0);
            drawHomeHero(HL::top);
        }
        nav.card[0]=selected;railDrawPixmap=nullptr;
        strip->SetDrawPortPoint(cPoint(-from,0));flushHomeOsd();
        const auto began=std::chrono::steady_clock::now();
        for(;;){
            const auto now=std::chrono::steady_clock::now();
            const int ms=int(std::chrono::duration_cast<std::chrono::milliseconds>(now-began).count());
            if(ms>=HL::heroScrollDurationMs)break;
            strip->SetDrawPortPoint(cPoint(-HL::scrollOffset(from,to,ms*HL::scrollDurationMs/HL::heroScrollDurationMs),0));
            flushHomeOsd();
            std::this_thread::sleep_until(now+std::chrono::milliseconds(24));
        }
        redrawHomeRail(0,0,true);
        osd->DestroyPixmap(strip);
        drawHomeFocusPanel();flushHomeOsd();
        return true;
    }
    // Render four adjacent rails once, then move their native draw port inside
    // the fixed three-rail viewport. No image decoding or API work per frame.
    bool animateHomeRows(int previousFirst,int first)
    {
        if(!osd||std::abs(first-previousFirst)!=1)return false;
        if(previousFirst==0||first==0)restoreLivePreview();
        const int x0=X(homeLeft()-12),x1=X(HL::right-9);
        const int y0=Y(HL::top),y1=Y(HL::viewportBottom);
        const int step=Y(HL::headingY(1))-y0;
        const int w=x1-x0,h=y1-y0;
        if(w<=0||h<=0||step<=0)return false;
        cPixmap *strip=osd->CreatePixmap(1,cRect(x0,y0,w,h),cRect(0,0,w,h+step+2));
        if(!strip)return false; // providers without a large pixmap keep normal navigation
        strip->Fill(kBackground);
        const int start=std::min(previousFirst,first);
        const auto &bg=cinematicBackground();
        for(int i=0;i<=HL::rows;++i){
            // Use a real on-screen row for composition, including the entering
            // rail; translate only its finished drawing into the strip.
            const int row=std::clamp(start+i-first,0,HL::rows-1);
            const int sourceY=Y(HL::headingY(row));
            const int destY=Y(HL::headingY(i))-y0;
            const int sourceH=Y(HL::headingY(row+1))-sourceY;
            suiteosd::Artwork patch{w,sourceH,std::vector<uint32_t>(size_t(w)*sourceH)};
            for(int y=0;y<sourceH;++y)
                std::copy_n(bg.argb.begin()+size_t(sourceY+y)*width+x0,w,
                            patch.argb.begin()+size_t(y)*w);
            railDrawPixmap=strip;
            railDrawX=-x0;
            railDrawY=destY-sourceY;
            drawCinemaImage(patch,x0,sourceY);
            drawHomeRail(start+i,HL::headingY(row));
            railDrawPixmap=nullptr;
        }
        const int from=(previousFirst-start)*step,to=(first-start)*step;
        strip->SetDrawPortPoint(cPoint(0,-from));
        flushHomeOsd();
        // VDR's normal idle key cadence is not a video frame clock. Keep
        // this short transition on the existing OSD owner, using elapsed
        // time so a slow provider skips frames rather than extending it.
        const auto began=std::chrono::steady_clock::now();
        for(;;){
            const auto now=std::chrono::steady_clock::now();
            const int elapsed=int(std::chrono::duration_cast<std::chrono::milliseconds>(now-began).count());
            if(elapsed>=HL::scrollDurationMs)break;
            strip->SetDrawPortPoint(cPoint(0,-HL::scrollOffset(from,to,elapsed)));
            flushHomeOsd();
            std::this_thread::sleep_until(now+std::chrono::milliseconds(24));
        }
        strip->SetDrawPortPoint(cPoint(0,-to));
        flushHomeOsd();
        // Restore the normal static rendering at the exact destination before
        // removing the temporary layer. Subsequent focus redraws stay local.
        for(int row=0;row<HL::rows;++row)
            redrawHomeRail(first+row,first,true);
        osd->DestroyPixmap(strip);
        if(first==0){startLivePreview(true);drawHomeHero(HL::top);}
        drawHomeFocusPanel();
        flushHomeOsd();
        return true;
    }
    void drawHomeNotice()
    {
        // No permanent instructional footer: keys and actions are unchanged.
        if(!notice.empty()){
            fill(305,850,1550,888,0xFF061626u);
            print(323,856,notice,fontSmall,kFocus,1200);
        }
    }

    void draw()
    {
        if (!osd)
            return;
        ++fullDraws;
        if (searchActive) {
            restoreLivePreview();
            drawSearch();
            flushHomeOsd();
            return;
        }
        const bool home = !photosActive && !settingsActive && !recordingsActive && !timelineActive;
        if(home)nav.firstVisibleSection=HL::firstRow(nav.section,suiteosd::Navigation::kSections,nav.firstVisibleSection);
        if ((home && homeHeroVisible() && !recordingDetails && !homeProgrammeDetails) ||
            (recordingsActive && !browserDetails && !browserDeleteConfirm))
            startLivePreview(home);
        else
            restoreLivePreview();
        if (photosActive) {
            drawPhotoGallery();
            flushHomeOsd();
            return;
        }
        if (settingsActive) {
            drawSettings();
            flushHomeOsd();
            return;
        }
        if (recordingsActive) {
            drawRecordingBrowser();
            flushHomeOsd();
            return;
        }
        if (timelineActive) {
            drawTimeline();
            flushHomeOsd();
            return;
        }
        paintHomeBackground();
        drawHeader();
        if(!homeCollections.empty()){
            drawHomeCollection();
            if(!recordingDetails)drawHomeSidePanel();
            drawRecordingDetails();scheduleVisibleHomePoster();flushHomeOsd();return;
        }
        nav.firstVisibleSection=HL::firstRow(nav.section,suiteosd::Navigation::kSections,nav.firstVisibleSection);
        const int first=nav.firstVisibleSection;
        loadVisibleHomeRails();
        for(int row=0;row<HL::rows;++row)
            drawHomeRail(first+row,HL::headingY(row));
        drawHomeSidePanel();
        drawHomeNotice();
        drawCinematicSidebar();
        drawRecordingDetails();
        drawHomeProgrammeInfo();
        scheduleVisibleHomePoster();
        flushHomeOsd();
    }

    eOSState action(suiteosd::Activation a)
    {
        switch (a) {
        case suiteosd::Activation::Hero: {
            const auto *p=heroProgramme(0);
            if(p){
                if(p->channelNumber==cDevice::CurrentChannel())return osEnd;
                restoreLivePreview();
                bool switched=false;
                {LOCK_CHANNELS_READ; switched=Channels&&Channels->SwitchTo(p->channelNumber);}
                if(switched)return osEnd;
            }
            notice="Senderwechsel nicht verfügbar";break;
        }
        case suiteosd::Activation::ChannelNow: {
            if (nav.card[1] >= 0 && nav.card[1] < int(data.now.size()) &&
                cDevice::PrimaryDevice()) {
                const int number = data.now[nav.card[1]].channelNumber;
                LOCK_CHANNELS_READ;
                if (Channels && Channels->SwitchTo(number))
                    return osEnd;
            }
            notice = "Senderwechsel nicht verfügbar";
            break;
        }
        case suiteosd::Activation::ChannelNext:
            homeProgrammeDetails=true; homeTimerConfirm=false; homeTimerNotice.clear();
            break;
        case suiteosd::Activation::Recording: {
            auto it=homeRails.find(nav.section);
            if(it!=homeRails.end()&&it->second.status.ok)return activateHomeRail();
            const int index=nav.card[nav.section];
            if(index>=0&&index<int(data.recordings.size())){
                homeDetailRecording=data.recordings[index];homeResumeDetail=false;homeResumeSeconds=-1;
                detailCastNav.Close();detailScroll=0;recordingDetails=true;
            }
            break;
        }
        case suiteosd::Activation::HomeCard:return activateHomeRail();
        case suiteosd::Activation::Module: {
            // VDR handles direct-menu keys in its main loop and creates
            // its own cMenuMain submenus. Returning osSchedule/osTimers
            // from a plain cOsdObject would NOT open them.
            eKeys nativeKey = kNone;
            switch (nav.sideIndex) {
            case 1: return osEnd;       // Live-TV: return to current broadcast
            case 2: openTimeline(); break; // Native VDR schedule, Suite Web timeline design
            case 3: openRecordingBrowser(); return osContinue; // Native Suite recording grid
            case 8: openPhotoGallery(); return osContinue; // image_next VDR service
            case 4: nativeKey = kTimers; break;     // Timers
            case 11: openSearch(); return osContinue;
            case 12: nativeKey = kMenu; break; // classic VDR menu remains accessible
            case 10: openSettings(); return osContinue; // Unified VDR+Suite settings
            default: break;
            }
            if (nativeKey != kNone) {
                if (cRemote::Put(nativeKey, true))
                    return osEnd;
                notice = "VDR-Menü konnte nicht geöffnet werden";
            } else if (nav.sideIndex == 5) {
                // Search timers belong to the local epgsearch VDR plugin,
                // NOT the VDR core or VDR-Suite Public API v1.
                // Prefer epgsearchonly's direct native search menu when
                // installed; epgsearch itself is a usable local fallback.
                const char *plugin = cPluginManager::GetPlugin("epgsearchonly")
                    ? "epgsearchonly"
                    : (cPluginManager::GetPlugin("epgsearch") ? "epgsearch" : nullptr);
                if (plugin && cRemote::CallPlugin(plugin))
                    return osEnd;
                notice = plugin ? "Epgsearch-Menü konnte nicht geöffnet werden"
                                : "Epgsearch-Plugin nicht installiert";
            } else if (nav.sideIndex == 6 || nav.sideIndex == 7) {
                // Keep HbbTV and teletext owned by their installed VDR plugins.
                const char *plugin = nav.sideIndex == 6 ? "web" : "osdteletext";
                if (cPluginManager::GetPlugin(plugin) && cRemote::CallPlugin(plugin))
                    return osEnd;
                notice = nav.sideIndex == 6
                    ? "HbbTV-Plugin nicht verfügbar"
                    : "Teletext-Plugin nicht verfügbar";
            } else {
                notice = "Dieses Suite-Modul folgt in einem weiteren Integrationsschritt.";
            }
            break;
        }
        default: break;
        }
        return osContinue;
    }

public:
    explicit cSuiteHome(NativeHomeCache &cache):nativeHome(cache){nativeHome.opened();}
    ~cSuiteHome() override
    {
        isyslog("suiteosd: HOME_PERF warm=%d first_flush_us=%lld full_draws=%llu flushes=%llu flush_us=%llu dpad_samples=%llu dpad_total_us=%llu dpad_max_us=%llu native_hits=%llu native_misses=%llu scraper_calls=%llu scraper_us=%llu native_completed=%llu native_pixel_bytes=%zu",
            int(warmOpen),firstFrameUs,fullDraws,flushes,flushUs,keySamples,keyUs,keyMaxUs,
            nativeHome.hits-startHits,nativeHome.misses-startMisses,nativeHome.scraperCalls-startScraper,
            nativeHome.scraperUs-startScraperUs,nativeHome.completed-startCompleted,nativeHome.pixelBytes());
        nativeHome.closed();
        homeEventStop.store(true);
        for(auto &entry:homePosterJobs)if(entry.second.valid())entry.second.wait();
        if(homeEventJob.valid())homeEventJob.wait();
        if(homeProgressJob.valid())homeProgressJob.wait();
        restoreLivePreview();
        delete osd;
        delete fontTitle;
        delete fontLarge;
        delete fontBody;
        delete fontSmall;
        delete fontCinemaBrand;
        delete fontCinemaCard;
        delete fontCinemaMeta;
    }

    void Show() override
    {
        refreshHomeSnapshot(true);
        int outputW=0,outputH=0;
        double pixelAspect=1.0;
        if(auto *device=cDevice::PrimaryDevice())
            device->GetOsdSize(outputW,outputH,pixelAspect);
        const auto area=suiteosd::FullscreenOsdRect(outputW,outputH,
            {cOsd::OsdLeft(),cOsd::OsdTop(),cOsd::OsdWidth(),cOsd::OsdHeight()});
        width=area.w;height=area.h;osdOriginX=area.x;osdOriginY=area.y;
        nativeHome.setSize(width,height);
        osd = cOsdProvider::NewOsd(osdOriginX,osdOriginY);
        if (!osd)
            return;
        const tArea fullArea = {0, 0, width - 1, height - 1, 32};
        if (osd->SetAreas(&fullArea, 1) != oeOk) {
            esyslog("suiteosd: TrueColor OSD area unavailable; cannot render Home");
            delete osd;osd=nullptr;
            return;
        }
        isyslog("suiteosd: dashboard OSD %dx%d at (%d,%d), output %dx%d",
            width,height,osdOriginX,osdOriginY,outputW,outputH);
        // Cover the entire output before exposing any intentional video slot.
        osd->DrawRectangle(0,0,width-1,height-1,kBackground);
        fontTitle = cFont::CreateFont("Sans:Bold", std::max(20, Y(50)));
        fontLarge = cFont::CreateFont("Sans:Bold", std::max(16, Y(29)));
        fontBody = cFont::CreateFont("Sans", std::max(14, Y(23)));
        fontSmall = cFont::CreateFont("Sans", std::max(12, Y(19)));
        fontCinemaBrand = cFont::CreateFont("Sans:Bold",std::max(20,Y(34)));
        fontCinemaCard = cFont::CreateFont("Sans:Bold",std::max(12,Y(18)));
        fontCinemaMeta = cFont::CreateFont("Sans",std::max(11,Y(16)));
        nav.section=0;
        startHomeEvents();
        if (!gReplayPersonSearch.empty()) {
            searchFromReplay=true;
            const std::string name=std::exchange(gReplayPersonSearch,"");
            openSearch(name);
        }
        draw();
    }

    eOSState ProcessKey(eKeys key) override
    {
        const eKeys base = static_cast<eKeys>(key & ~k_Repeat);
        if(base!=kNone){
            keyPending=base==kUp||base==kDown||base==kLeft||base==kRight;
            if(keyPending)keyAt=PerfClock::now();
        }
        if(base==kNone&&osd&&gLivePreviewEnabled&&!searchActive&&!photosActive&&!settingsActive&&!timelineActive){
            const bool home=!recordingsActive;
            const bool wantsPreview=home?(homeHeroVisible()&&!recordingDetails&&!homeProgrammeDetails):
                (!browserDetails&&!browserDeleteConfirm);
            if(wantsPreview&&std::chrono::steady_clock::now()>=nextPreviewAttempt){
                nextPreviewAttempt=std::chrono::steady_clock::now()+std::chrono::milliseconds(750);
                const auto *previous=scaledDevice;
                startLivePreview(home);
                if(previous!=scaledDevice){
                    if(home){drawHomeLivePanel();flushHomeOsd();}
                    else draw();
                }
            }
        }
        if(homeDetailPending&&base!=kNone){homeDetailPending=false;invalidateSearchRead();notice.clear();}
        const bool homeChanged=finishHomeRead();
        const auto posterChanged=finishHomePosters();
        const bool searchChanged=finishSearchRead();
        const bool homeDashboard=osd&&!searchActive&&!photosActive&&!settingsActive&&!recordingsActive&&!timelineActive&&!recordingDetails&&!homeProgrammeDetails;
        std::vector<std::string> localKeys;std::vector<std::uintptr_t> localEvictions;
        nativeHome.takeChanges(localKeys,localEvictions);
        if(!localEvictions.empty())cinematicComposites.eraseIf([&](const auto &key,const auto &){
            return std::find(localEvictions.begin(),localEvictions.end(),key.second)!=localEvictions.end();
        });
        if(!localKeys.empty()&&homeDashboard&&!homeChanged){
            if(!homeCollections.empty())draw();
            else {
                const bool events=std::any_of(localKeys.begin(),localKeys.end(),[](const auto &k){return k.rfind("epg:",0)==0;});
                const bool recordings=std::any_of(localKeys.begin(),localKeys.end(),[](const auto &k){return k.rfind("rec:",0)==0;});
                bool changed=false;const int first=nav.firstVisibleSection;
                for(int section=first;section<first+HL::rows;++section){
                    if((events&&(section==1||section==2))||(recordings&&section>=3)){
                        redrawHomeRail(section,first,true);changed=true;
                    }
                }
                if(changed){drawHomeFocusPanel();flushHomeOsd();}
            }
        }
        if(homeChanged&&homeDashboard)draw(); // also when a remote key arrives with the readback
        if(!posterChanged.empty()&&osd&&!searchActive&&!photosActive&&!settingsActive&&!recordingsActive&&!timelineActive&&!recordingDetails&&!homeProgrammeDetails){
            if(!homeCollections.empty())draw();
            else {
                bool changed=false;
                const int first=nav.firstVisibleSection;
                for(int section=first;section<first+HL::rows;++section){
                    const auto it=homeRails.find(section);
                    if(it==homeRails.end())continue;
                    const auto &cards=it->second.page.cards;
                    const int start=nav.firstCard[section];
                    for(int i=start;i<start+HL::visibleColumns(nav.sideFocused)&&i<int(cards.size());++i)
                        if(std::find(posterChanged.begin(),posterChanged.end(),homePosterKey(cards[i].recording))!=posterChanged.end()){
                            redrawHomeRail(section,first,true);changed=true;break;
                        }
                }
                if(changed){drawHomeFocusPanel();flushHomeOsd();}
                scheduleVisibleHomePoster();
            }
        }
        if(searchChanged&&!searchActive&&base==kNone&&osd){draw();return osContinue;}
        if (searchActive) {
            if(base==kNone){if(searchChanged)draw();return osContinue;}
            return searchKey(base);
        }
        if (base == kSetup) {
            openSettings();
            draw();
            return osContinue;
        }
        if (base == kRecordings) {
            if (!recordingsActive) openRecordingBrowser();
            draw();
            return osContinue;
        }
        if (photosActive)
            return photoKey(base);
        if (recordingsActive)
            return browserKey(base);
        if (timelineActive)
            return timelineKey(base);
        if (settingsActive)
            return settingsKey(base);
        if (homeProgrammeDetails) {
            if(homeTimerConfirm){
                if(base==kOk){
                    const auto *p=selectedHomeProgramme();
                    homeTimerNotice=p?createNativeTimer(p->channelNumber,p->eventId,p->start):"Keine Sendung ausgewählt";
                    homeTimerConfirm=false;
                }else if(base==kBack||base==kInfo)homeTimerConfirm=false;
            }else if(base==kRed||base==kRecord){
                homeTimerConfirm=true;homeTimerNotice.clear();
            }else if(base==kYellow||base==kInfo||base==kBack||base==kOk){
                homeProgrammeDetails=false;homeTimerNotice.clear();
            }
            draw();return osContinue;
        }
        if (recordingDetails) {
            if(handleDetailCast(base,&homeDetailRecording))return osContinue;
            if(base==kOk||base==kPlay||base==kRed||(base==kGreen&&homeResumeDetail)){
                const auto result=playHomeRecording(base==kGreen);
                if(result!=osContinue)return result;
            }else if(base==kInfo||base==kBack){recordingDetails=false;notice.clear();}
            draw();return osContinue;
        }
        if(base==kNone){
            if(homeChanged&&osd&&!homeDashboard){draw();return osContinue;}
            if(osd&&homeCollections.empty()){
                if(homeProgressChanged.exchange(false)){
                    for(const int section:{suiteosd::Navigation::kContinue,suiteosd::Navigation::kRecent}){
                        auto it=homeRails.find(section);
                        if(it!=homeRails.end()){it->second.attempted=false;it->second.loading=false;it->second.requestId=0;}
                    }
                }
                if(homeEventChanged.exchange(false)){
                    refreshHomeSnapshot();
                    // Invalidate once on hello/change; keep visible last data until refreshed.
                    for(auto &entry:homeRails){entry.second.attempted=false;entry.second.loading=false;entry.second.requestId=0;}
                    recordingGenres.clear();
                    ++homePosterRevision; // resync in place; never discard decoded pixels on SSE hello
                }
                if(loadVisibleHomeRails(true)){drawHomeFocusPanel();flushHomeOsd();}
                // Warm the next/previous rail using the existing read queue.
                // Only do this after current visible reads have completed.
                if(!homeReadJob.valid()&&homeReadQueue.empty()){
                    const int first=nav.firstVisibleSection;
                    for(int section:{first+HL::rows,first-1})
                        if(section>=3&&section<suiteosd::Navigation::kSections){
                            loadHomeRail(section);if(homeReadJob.valid())break;
                        }
                }
                scheduleVisibleHomePoster();
            }
            if(osd&&focusPending&&std::chrono::steady_clock::now()-focusSince>=std::chrono::milliseconds(220)){
                drawHomeFocusPanel();flushHomeOsd();
            }
            else if(osd&&notice.empty()&&focusClockMinute!=time(nullptr)/60){
                drawHomeLivePanel();drawHomeSidePanel();flushHomeOsd(); // local clock only, no status polling
            }
            return osContinue;
        }
        if(!homeCollections.empty())return homeCollectionKey(base);
        if ((base == kInfo || base == kYellow) && !nav.sideFocused) {
            if(nav.section>=3) {
                const auto result=action(nav.section==suiteosd::Navigation::kNew?
                    suiteosd::Activation::Recording:suiteosd::Activation::HomeCard);
                if(result!=osContinue)return result;
            }else if(selectedHomeProgramme())homeProgrammeDetails=true;
            draw();return osContinue;
        }
        if (base == kRed) {
            openRecordingBrowser();
            draw();
            return osContinue;
        }
        if (base == kGreen) {
            openTimeline();
            draw();
            return osContinue;
        }
        if (base == kBlue) {
            if (cRemote::Put(kTimers, true))
                return osEnd;
            notice = "VDR-Timerverwaltung nicht verfügbar";
            draw();
            return osContinue;
        }
        if (base == k0) {
            refreshHomeSnapshot();
            recordingMeta.clear(); // explicit refresh checks for updated scraper results
            recordingDetailImages.clear();
            focusPreviews.clear();focusKey.clear();focusPending=false;
            cinematicComposites.clear();nativeHome.reset();
            homeRails.clear();recommendedGenreId.clear();recommendedGenreTitle.clear();invalidatePublicHomePosters();
            homeReadQueue.clear();loadHomeRail(nav.section);
            nav.KeepVisible(suiteosd::Navigation::kHomeVisibleCards, count(nav.section));
            notice = "EPG und Aufnahmen aktualisiert";
            draw();
            return osContinue;
        }
        suiteosd::Direction direction;
        switch (base) {
        case kUp: direction = suiteosd::Direction::Up; break;
        case kDown: direction = suiteosd::Direction::Down; break;
        case kLeft: direction = suiteosd::Direction::Left; break;
        case kRight: direction = suiteosd::Direction::Right; break;
        case kOk: direction = suiteosd::Direction::Select; break;
        case kBack: return nav.Move(suiteosd::Direction::Back, count(nav.section), suiteosd::Navigation::kHomeVisibleCards) ==
                           suiteosd::Activation::Close ? osEnd : (draw(), osContinue);
        default: return osContinue;
        }
        const bool hadNotice=!notice.empty();
        notice.clear();
        // Navigation changes the section before clamping the card focus.
        // Use the destination section's actual item count, not the old row's.
        int target = nav.section;
        if (!nav.sideFocused && direction == suiteosd::Direction::Down)
            target = std::min(suiteosd::Navigation::kSections-1, target + 1);
        if (!nav.sideFocused && direction == suiteosd::Direction::Up)
            target = std::max(0, target - 1);
        if (nav.sideFocused && direction == suiteosd::Direction::Select &&
            nav.sideIndex == 0)
            target = 0;
        const auto previous=nav;
        const bool railLoaded=target>=3&&(homeRails.find(target)==homeRails.end()||!homeRails.at(target).attempted);
        if(target>=3)loadHomeRail(target);
        const eOSState result = action(nav.Move(direction, count(target), suiteosd::Navigation::kHomeVisibleCards));
        if (result != osContinue)
            return result;
        loadVisibleHomeRails();
        scheduleVisibleHomePoster();
        const bool changed=previous.section!=nav.section||previous.sideFocused!=nav.sideFocused||
            previous.sideIndex!=nav.sideIndex||previous.card!=nav.card||previous.firstCard!=nav.firstCard;
        if(direction==suiteosd::Direction::Select||hadNotice){draw();return osContinue;}
        if(!changed&&!railLoaded)return osContinue;
        if(previous.section==0&&nav.section==0&&!previous.sideFocused&&!nav.sideFocused&&
           (direction==suiteosd::Direction::Left||direction==suiteosd::Direction::Right)&&
           !(key&k_Repeat)&&animateHeroChannels(previous.card[0],direction==suiteosd::Direction::Right))return osContinue;
        const int first=nav.firstVisibleSection;
        if(osd&&!previous.sideFocused&&!nav.sideFocused&&first!=previous.firstVisibleSection&&
            (direction==suiteosd::Direction::Up||direction==suiteosd::Direction::Down)&&
            animateHomeRows(previous.firstVisibleSection,first))return osContinue;
        if(osd&&!previous.sideFocused&&!nav.sideFocused&&
            first==previous.firstVisibleSection){
            redrawHomeRail(previous.section,first,previous.firstCard[previous.section]!=nav.firstCard[previous.section]||
                          (railLoaded&&previous.section==target));
            if(nav.section!=previous.section)
                redrawHomeRail(nav.section,first,previous.firstCard[nav.section]!=nav.firstCard[nav.section]||railLoaded);
            if(previous.section==0||nav.section==0)drawHomeLivePanel();
            drawHomeFocusPanel();
            flushHomeOsd();
        }else draw();
        return osContinue;
    }
};

// Also expose the same preference through VDR's original plugin setup menu.
// Both entry points persist the exact same setting under the suiteosd plugin
// namespace; no parallel settings file or database is created.
class cMenuSuiteOsdSetup final : public cMenuSetupPage {
private:
    int livePreview;
    int photoView;
    char coreUrl[256] = {};
    char deviceName[129] = {};
    suiteosd::CoreAdminClient pairingClient{gCoreUrl};
    suiteosd::DevicePairing pairing;
    // Issued credential is retained only in memory until the private vault
    // confirms storage. A storage failure must not repeat issuance.
    suiteosd::DeviceCredential issued;
    cOsdItem *stateItem = nullptr;
    cOsdItem *codeItem = nullptr;
    cOsdItem *startItem = nullptr;
    cOsdItem *finishItem = nullptr;
    cOsdItem *testItem = nullptr;
    cOsdItem *forgetItem = nullptr;
    enum class PairAction {Start,Complete,Test};
    struct PairResult {
        std::string origin, message;
        suiteosd::DevicePairing pairing;
        suiteosd::DeviceCredential issued;
    };
    // Only the setup menu owns this bounded worker. It never uses VDR UI
    // objects, and all profile writes finish before the worker is destroyed.
    std::future<PairResult> pairingJob;
    std::chrono::steady_clock::time_point nextPairingCheck{};
    std::string visiblePairingStatus;
    bool verifyOnOpen=false;

    bool pairingConnected() const { return visiblePairingStatus=="Erfolgreich gekoppelt"; }
    std::string pairingCodeText() const
    {
        if(pairingConnected())return "Verbunden";
        return pairing.userCode.empty()?"Kein offener Kopplungscode":suiteosd::kPairingCodePrefix+pairing.userCode;
    }
    void setStatus(const std::string &message)
    {
        const bool changed=visiblePairingStatus!=message;
        visiblePairingStatus=message;
        const std::string code=pairingCodeText();
        if(!changed&&code==codeItem->Text())return;
        stateItem->SetText(message.c_str());
        codeItem->SetText(code.c_str());
        startItem->SetText(pairingConnected()?"Gerätestatus anzeigen":"Gerät koppeln");
        if(!HasSubMenu()){
            Display();
            SetStatus(message.c_str());
        }
    }
    void selectServer()
    {
        if (pairingClient.origin() != coreUrl) {
            pairing = {};
            pairingClient.setOrigin(coreUrl);
        }
    }
    void runPairing(PairAction action,bool manual=true)
    {
        if(pairingJob.valid())return;
        selectServer();
        const std::string origin=pairingClient.origin(), name=deviceName, path=deviceProfilePath();
        if(action==PairAction::Start&&!issued.secret.empty()){
            setStatus("Zuerst ausgestellte Berechtigung speichern");return;
        }
        if(manual||action!=PairAction::Complete)
            setStatus(action==PairAction::Start?"Kopplungsanfrage wird erstellt ...":
                      action==PairAction::Test||pairing.requestId.empty()?"Geräteverbindung wird geprüft ...":"Freigabe wird geprüft ...");
        try {
            pairingJob=std::async(std::launch::async,[origin,name,path,action,p=pairing,c=issued]() mutable {
                PairResult out{origin,{},std::move(p),std::move(c)};
                suiteosd::CoreAdminClient client(origin);
                suiteosd::CoreStatus status;
                if(action==PairAction::Start){
                    status=client.beginPairing(name,VERSION,out.pairing);
                    out.message=status.ok?"Code im Webfrontend genehmigen – Abschluss automatisch":status.message;
                }else if(action==PairAction::Complete){
                    status=client.completePairing(out.pairing,out.issued,path);
                    out.message=status.message;
                }else{
                    suiteosd::DeviceCredential saved;
                    if(!suiteosd::loadDeviceProfile(path,origin,saved)||!client.setDeviceCredential(saved))
                        out.message="Keine gespeicherte Kopplung für diesen Server";
                    else {
                        status=client.testDevice();
                        out.message=status.ok?"Erfolgreich gekoppelt":status.message;
                    }
                }
                return out;
            });
        }catch(const std::exception &){setStatus("Gerätevorgang konnte nicht gestartet werden");}
    }
    void updatePairing()
    {
        if(pairingJob.valid()){
            if(pairingJob.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
            try {
                auto result=pairingJob.get();
                if(result.origin==coreUrl){
                    pairing=std::move(result.pairing);issued=std::move(result.issued);
                    setStatus(result.message);
                    nextPairingCheck=std::chrono::steady_clock::now()+std::chrono::seconds(pairing.pollIntervalSeconds);
                }else{
                    // Credential storage (if issued) used the original exact origin.
                    // Never project its state onto a newly edited server URL.
                    pairing={};issued=std::move(result.issued);selectServer();
                    setStatus("Server geändert – Kopplungsstatus für neues Ziel prüfen");
                }
            }catch(const std::exception &){pairing={};setStatus("Gerätevorgang fehlgeschlagen – Status prüfen");}
        }
        if(!pairingJob.valid()&&pairing.state=="pending"&&pairing.origin==coreUrl){
            if(pairing.expiresAtEpoch>0&&time(nullptr)>=pairing.expiresAtEpoch){
                // Server remains authoritative; the manual check can still read its status.
                pairing.state="expired";
                setStatus("Kopplung abgelaufen – neue Anfrage starten oder Status prüfen");
            }else if(std::chrono::steady_clock::now()>=nextPairingCheck)
                runPairing(PairAction::Complete,false);
        }
    }
    class PairingDialog final : public cOsdMenu {
        cMenuSuiteOsdSetup &owner;
        cOsdItem *code=nullptr,*status=nullptr,*help1=nullptr,*help2=nullptr,*check=nullptr,*back=nullptr;
        void UpdateLabels()
        {
            const bool pending=!owner.pairing.userCode.empty();
            help1->SetText(pending?"Im Webfrontend: Einstellungen > Wartende Geräte":"");
            help2->SetText(pending?"Dieses Gerät genehmigen. Der Abschluss erfolgt automatisch.":"");
            check->SetText(pending?"Status prüfen":"Verbindung prüfen");
        }
    public:
        explicit PairingDialog(cMenuSuiteOsdSetup &parent)
            :cOsdMenu(suiteosd::kPairingDialogTitle),owner(parent)
        {
            SetMenuCategory(mcPluginSetup);
            Add(code=new cOsdItem(owner.pairingCodeText().c_str(),osUnknown,false));
            Add(status=new cOsdItem(owner.visiblePairingStatus.c_str(),osUnknown,false));
            Add(help1=new cOsdItem("",osUnknown,false));
            Add(help2=new cOsdItem("",osUnknown,false));
            Add(check=new cOsdItem("Status prüfen"),true);
            Add(back=new cOsdItem("Zurück zu den Einstellungen"));
            UpdateLabels();
        }
        void Refresh()
        {
            const auto nextCode=owner.pairingCodeText();
            if(nextCode==code->Text()&&owner.visiblePairingStatus==status->Text())return;
            code->SetText(nextCode.c_str());
            status->SetText(owner.visiblePairingStatus.c_str());
            UpdateLabels();
            Display();
        }
        eOSState ProcessKey(eKeys key) override
        {
            if(key!=kOk)return cOsdMenu::ProcessKey(key);
            const auto *item=Get(Current());
            if(item==back)return osBack;
            if(owner.pairingJob.valid())owner.setStatus("Gerätevorgang läuft ...");
            else if(item==check)owner.runPairing(PairAction::Complete);
            Refresh();
            return osContinue;
        }
    };
protected:
    void Store(void) override
    {
        gLivePreviewEnabled = livePreview;
        SetupStore("LivePreview", gLivePreviewEnabled);
        gPhotoViewMode = photoView;
        SetupStore("PhotoView", gPhotoViewMode);
        std::snprintf(gCoreUrl, sizeof(gCoreUrl), "%s", coreUrl);
        SetupStore("CoreURL", gCoreUrl);
        std::snprintf(gDeviceName, sizeof(gDeviceName), "%s", deviceName);
        SetupStore("DeviceName", gDeviceName);
    }
public:
    cMenuSuiteOsdSetup(void)
        : livePreview(gLivePreviewEnabled), photoView(gPhotoViewMode)
    {
        std::snprintf(coreUrl, sizeof(coreUrl), "%s", gCoreUrl);
        std::snprintf(deviceName, sizeof(deviceName), "%s", gDeviceName);
        SetSection("VDR Suite-OSD");
        Add(new cMenuEditBoolItem("Live-TV-Vorschau in Home und Aufnahmen", &livePreview));
        static const char * const views[] = {"Raster", "Karussell"};
        Add(new cMenuEditStraItem("Bildergalerie - Anzeigeart",
                                  &photoView, 2, views));
        Add(new cMenuEditStrItem("VDR-Suite Core URL", coreUrl, sizeof(coreUrl)));
        Add(new cMenuEditStrItem("Gerätename", deviceName, sizeof(deviceName)));
        verifyOnOpen=restoreDevice(pairingClient);
        visiblePairingStatus=verifyOnOpen?"Geräteberechtigung gespeichert":"Nicht gekoppelt";
        Add(stateItem = new cOsdItem(visiblePairingStatus.c_str(),osUnknown,false));
        Add(codeItem = new cOsdItem("Kein offener Kopplungscode",osUnknown,false));
        Add(new cOsdItem("Für entfernte Server HTTPS verwenden",osUnknown,false));
        Add(startItem = new cOsdItem("Gerät koppeln"));
        Add(finishItem = new cOsdItem("Status prüfen"));
        Add(testItem = new cOsdItem("Verbindung testen"));
        Add(forgetItem = new cOsdItem("Lokale Kopplung aufheben"));
        Add(new cOsdItem("Einstellungen speichern"));
    }
    eOSState ProcessKey(eKeys Key) override
    {
        if(verifyOnOpen){
            verifyOnOpen=false;
            if(pairingClient.origin()==coreUrl)runPairing(PairAction::Test);
        }
        updatePairing();
        if(HasSubMenu()){
            static_cast<PairingDialog*>(SubMenu())->Refresh();
            const auto state=cMenuSetupPage::ProcessKey(Key);
            if(!HasSubMenu()&&!visiblePairingStatus.empty())SetStatus(visiblePairingStatus.c_str());
            return state;
        }
        if (Key != kOk) return cMenuSetupPage::ProcessKey(Key);
        cOsdItem *item = Get(Current());
        if (item != startItem && item != finishItem &&
            item != testItem && item != forgetItem)
            return cMenuSetupPage::ProcessKey(Key);
        if(item==startItem){
            if(!pairingJob.valid()){
                const bool alreadyConnected=pairingConnected()&&pairingClient.origin()==coreUrl;
                selectServer();
                if(alreadyConnected)runPairing(PairAction::Test);
                else if(pairing.state!="pending"||pairing.origin!=coreUrl)runPairing(PairAction::Start);
            }
            return AddSubMenu(new PairingDialog(*this));
        }
        if(pairingJob.valid()){
            setStatus("Gerätevorgang läuft ...");return osContinue;
        }
        selectServer();
        if(item==finishItem)runPairing(PairAction::Complete);
        else if(item==testItem)runPairing(PairAction::Test);
        else {
            if (!suiteosd::removeDeviceProfile(deviceProfilePath(),pairingClient.origin())) {
                setStatus("Lokale Kopplung konnte nicht entfernt werden");
                return osContinue;
            }
            pairingClient.forgetDevice();
            pairing = {};
            if (issued.origin == pairingClient.origin()) issued = {};
            setStatus("Lokal entkoppelt – Servergerät im Webfrontend widerrufen");
        }
        return osContinue;
    }
};

class cPluginSuiteOsd final : public cPlugin {
    NativeHomeCache nativeHome;
public:
    void MainThreadHook() override {nativeHome.tick();}
    void Stop() override {nativeHome.stop();}
    bool Start() override { suiteosd::RegisterSuiteSkin(); return true; }
    const char *Version() override { return VERSION; }
    const char *Description() override { return tr(DESCRIPTION); }
    const char *MainMenuEntry() override { return tr("VDR Suite"); }
    cOsdObject *MainMenuAction() override { return new cSuiteHome(nativeHome); }
    cMenuSetupPage *SetupMenu() override { return new cMenuSuiteOsdSetup(); }
    bool SetupParse(const char *Name, const char *Value) override
    {
        if (std::strcmp(Name, "LivePreview") == 0) {
            gLivePreviewEnabled = std::atoi(Value) != 0 ? 1 : 0;
            return true;
        }
        if (std::strcmp(Name, "PhotoView") == 0) {
            gPhotoViewMode = std::atoi(Value) == 0 ? 0 : 1;
            return true;
        }
        if (std::strcmp(Name, "CoreURL") == 0) {
            std::snprintf(gCoreUrl, sizeof(gCoreUrl), "%s", Value);
            return true;
        }
        if (std::strcmp(Name, "DeviceName") == 0) {
            std::snprintf(gDeviceName, sizeof(gDeviceName), "%s", Value);
            return true;
        }
        return false;
    }
};
} // namespace

VDRPLUGINCREATOR(cPluginSuiteOsd);
