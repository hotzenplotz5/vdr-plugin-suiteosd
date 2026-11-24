/*
 * VDR Suite OSD – first native Home screen.
 *
 * Draws real VDR EPG/recording data with a remote-only scrollable home.
 * This is not a cSkin replacement or an embedded HTML/video player.
 */
#include "navigation.h"
#include "settings_navigation.h"
#include "epg_timeline.h"
#include "recording_browser.h"
#include "logo_locator.h"
#include "artwork_decode.h"
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
#include <ctime>
#include <cstring>
#include <map>
#include <sstream>
#include <utility>
#include <string>
#include <vector>

static const char *VERSION = "0.7.0";
static const char *DESCRIPTION = trNOOP("VDR Suite TV OSD");

namespace {
constexpr tColor kBackground = 0xF9040A19;
constexpr tColor kSurface = 0xFC071729;
constexpr tColor kCard = 0xFF11243B;
constexpr tColor kCardAlt = 0xFF132A44;
constexpr tColor kFocus = 0xFF09D2FA;
constexpr tColor kHighlight = 0xFF056BA0;
constexpr tColor kText = 0xFFF5F9FF;
constexpr tColor kMuted = 0xFF9DB2C9;
constexpr tColor kLine = 0xFF20506D;

// Suite-OSD configuration is stored under the plugin owner in VDR setup.conf.
// This variable is loaded by SetupParse and written only through SetupStore.
int gLivePreviewEnabled = 1;

struct Programme {
    int channelNumber = 0;
    std::string channel;
    std::string title;
    std::string time;
    std::string description;
    int progress = 0;
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
    suiteosd::Artwork artwork;
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
    p.title = event ? event->Title() : "Keine Programminformation";
    p.time = eventTime(event);
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

const std::array<const char *, 11> kSidebar = {{
    "Home", "Live-TV", "EPG", "Aufnahmen", "Timer",
    "Suchtimer", "HbbTV", "Teletext", "Bilder", "Media Tools",
    "Einstellungen"
}};
const std::array<const char *, 11> kSymbols = {{
    "H", "TV", "E", "REC", "T", "S", "Hb", "TXT", "IMG", "M", "SET"
}};

class cSuiteHome final : public cOsdObject {
private:
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
    bool timelineActive = false;
    bool timelineDetails = false;
    bool timelineTimerConfirm = false;
    std::string timelineTimerNotice;
    std::map<std::string, suiteosd::Artwork> channelLogos;
    std::map<std::string, suiteosd::Artwork> timelineArtwork;
    Snapshot data;
    std::map<std::string, RecordingMeta> recordingMeta;
    std::map<std::string, std::vector<std::string>> recordingGenres;
    suiteosd::RecordingBrowser recordingBrowser;
    bool recordingsActive = false;
    bool browserDetails = false;
    bool browserDeleteConfirm = false;
    std::string browserNotice;
    cDevice *scaledDevice = nullptr;
    cRect scaledRect;
    std::string notice;
    bool recordingDetails = false;
    bool homeProgrammeDetails = false;
    int width = 1600;
    int height = 900;

    int X(int px) const { return px * width / 1600; }
    int Y(int px) const { return px * height / 900; }

    void fill(int left, int top, int right, int bottom, tColor color)
    {
        int x0 = std::max(0, X(left));
        int y0 = std::max(0, Y(top));
        int x1 = std::min(width - 1, X(right));
        int y1 = std::min(height - 1, Y(bottom));
        if (osd && x1 >= x0 && y1 >= y0)
            osd->DrawRectangle(x0, y0, x1, y1, color);
    }

    void print(int x, int y, const std::string &s, const cFont *f,
               tColor fg = kText, int maxWidth = 0)
    {
        if (osd && f && !s.empty() && y >= 0 && y < 885)
            osd->DrawText(X(x), Y(y), s.c_str(), fg, clrTransparent, f,
                          maxWidth > 0 ? X(maxWidth) : 0);
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
        case 0: return 1;
        case 1: return int(data.now.size());
        case 2: return int(data.next.size());
        case 3: return int(data.recordings.size());
        }
        return 0;
    }

    const suiteosd::Artwork &channelLogo(const suiteosd::TimelineChannel &channel)
    {
        const std::string key = channel.name + "\n" + channel.shortName;
        auto it = channelLogos.find(key);
        if (it != channelLogos.end()) return it->second;
        if (channelLogos.size() >= 48) channelLogos.clear();
        const std::string path = suiteosd::FindChannelLogo(channel.name, channel.shortName);
        return channelLogos.emplace(key,
            suiteosd::LoadArtwork(path, X(64), Y(42))).first->second;
    }

    bool drawChannelLogo(const suiteosd::TimelineChannel &channel,
                         int x, int y, int w, int h)
    {
        const suiteosd::Artwork &art = channelLogo(channel);
        if (!art.valid() || !osd) return false;
        cImage image(cSize(art.width, art.height), art.argb.data());
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
        print(33, 19, "VDR", fontTitle);
        print(116, 19, "Suite", fontTitle, kFocus);
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
        return osReplay;
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
            recordingGenres.erase(fileName);
            data = readVdrSnapshot();
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
            if (base == kBack || base == kInfo || base == kBlue || base == kOk) {
                browserDetails = false;
                draw();
            }
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
            if (browserSelectedRecording()) browserDetails = true;
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
                    const eOSState result = playBrowserRecording();
                    if (result != osContinue) return result;
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
        fill(0, 0, 1600, 72, kSurface);
        print(32, 18, "VDR", fontTitle);
        print(115, 18, "Suite", fontTitle, kFocus);
        print(244, 26, "TV-OSD", fontBody, kMuted);
        print(424, 26, "Live-TV  |  " + data.live.channel, fontBody);
        time_t now = time(nullptr);
        print(1460, 22, formatTime(now), fontTitle);
    }

    void drawHero(int scroll)
    {
        int y = 89 - scroll;
        if (y + 275 < 80 || y > 830)
            return;
        panel(154, y, 1415, 270, nav.section == 0 && !nav.sideFocused, kSurface);
        print(189, y + 23, "LIVE-TV  ·  KANAL " + std::to_string(data.live.channelNumber),
              fontSmall, kFocus);
        print(189, y + 61, data.live.channel.empty() ? "Kein aktiver Sender" : data.live.channel,
              fontTitle, kText, 760);
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
        panel(875, y + 18, 652, 224, false, kCardAlt);
        print(928, y + 70, "Live-TV", fontLarge, kText);
        print(928, y + 118, "Bildausgabe durch VDR / Ausgabeplugin", fontBody, kMuted, 560);
        print(928, y + 157, "Kein separater Streaming-Player", fontSmall, kMuted);
    }

    void drawPrograms(int section, int y, const char *heading, const std::vector<Programme> &items)
    {
        if (y < 75 || y > 815)
            return;
        print(155, y, heading, fontLarge);
        const int columns = 7;
        const int gap = 12;
        const int cw = 188;
        const int first = nav.firstCard[section];
        for (int col = 0; col < columns && first + col < int(items.size()); ++col) {
            const Programme &item = items[first + col];
            const int x = 157 + col * (cw + gap);
            const bool focus = !nav.sideFocused && nav.section == section &&
                               nav.card[section] == first + col;
            panel(x, y + 48, cw, 160, focus, kCard);
            print(x + 13, y + 61, item.channel, fontBody, kText, cw - 25);
            print(x + 13, y + 104, item.title, fontSmall, kText, cw - 24);
            print(x + 13, y + 132, item.time, fontSmall, kMuted, cw - 24);
            fill(x + 13, y + 190, x + cw - 13, y + 198, kLine);
            if (section == 1)
                fill(x + 13, y + 190,
                     x + 13 + (cw - 26) * item.progress / 100, y + 198, kFocus);
        }
        if (items.empty())
            print(159, y + 94, "Keine EPG-Daten verfügbar", fontBody, kMuted);
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
                               nullptr, nullptr, nullptr, nullptr);
            video->getMovieOrTv(
                nullptr, nullptr, nullptr, &meta.overview,
                &meta.genres, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, nullptr, nullptr, nullptr, nullptr);
            if (video->getVideoType() == tSeries) {
                std::string episodeOverview;
                if (video->getEpisode(nullptr, &episodeOverview, nullptr,
                                      nullptr, nullptr, nullptr, nullptr, nullptr) &&
                    !episodeOverview.empty())
                    meta.overview = std::move(episodeOverview);
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
            meta.artwork = suiteosd::LoadArtwork(imagePath, X(164), Y(55));
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

    void startLivePreview()
    {
        if (!gLivePreviewEnabled || scaledDevice) return;
        cDevice *device = cDevice::PrimaryDevice();
        if (!device || device->Replaying())
            return;
        const cRect requested(X(1197), Y(151), X(338), Y(190));
        const cRect available = device->CanScaleVideo(requested);
        if (available.IsEmpty())
            return; // provider does not support video scaling
        scaledDevice = device;
        scaledRect = available;
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
        settingsActive = false;
        if (timelineActive) timelineActive = false;
        recordingBrowser.ChangeView(suiteosd::RecordingBrowser::View::Folders);
        recordingBrowser.tabsFocused = true;
        browserDetails = browserDeleteConfirm = false;
        browserNotice.clear();
        data = readVdrSnapshot();
        refreshBrowser();
        recordingsActive = true;
        startLivePreview();
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

    void drawRecordings(int y)
    {
        if (y < 75 || y > 815)
            return;
        print(155, y, "Aufnahmen (" + std::to_string(data.recordings.size()) + ")", fontLarge);
        const int cw = 188;
        const int first = nav.firstCard[3];
        for (int col = 0; col < 7 && first + col < int(data.recordings.size()); ++col) {
            const Recording &item = data.recordings[first + col];
            const int x = 157 + col * 200;
            const bool focus = !nav.sideFocused && nav.section == 3 &&
                               nav.card[3] == first + col;
            panel(x, y + 48, cw, 160, focus, kCardAlt);
            const RecordingMeta &meta = metaFor(item);
            fill(x + 12, y + 59, x + cw - 12, y + 114, kLine);
            if (meta.artwork.valid()) {
                cImage picture(cSize(meta.artwork.width, meta.artwork.height),
                               meta.artwork.argb.data());
                const int left = X(x + 12) +
                    (X(cw - 24) - meta.artwork.width) / 2;
                const int top = Y(y + 59) +
                    (Y(55) - meta.artwork.height) / 2;
                osd->DrawImage(cPoint(left, top), picture);
            } else
                print(x + 25, y + 70, "AUFNAHME", fontSmall, kMuted);

            const std::string &title = meta.title.empty() ? item.title : meta.title;
            print(x + 13, y + 122, title, fontBody, kText, cw - 24);
            const std::string detail = !meta.episode.empty() ? meta.episode :
                                       !meta.release.empty() ? meta.release : item.time;
            print(x + 13, y + 161, detail, fontSmall, kMuted, cw - 24);
        }
        if (data.recordings.empty())
            print(159, y + 94, "Keine Aufnahmen vorhanden", fontBody, kMuted);
    }

    void drawRecordingDetails()
    {
        if (!recordingDetails || nav.section != 3 ||
            nav.card[3] < 0 || nav.card[3] >= int(data.recordings.size()))
            return;
        const Recording &item = data.recordings[nav.card[3]];
        const RecordingMeta &meta = metaFor(item);
        panel(268, 164, 1082, 540, true, kSurface);
        const std::string title = meta.title.empty() ? item.title : meta.title;
        print(299, 188, title, fontTitle, kText, 1008);
        print(299, 253, "Aufnahme  ·  " + item.time, fontSmall, kFocus);
        if (!meta.release.empty())
            print(299, 292, "Erscheinungsdatum: " + meta.release, fontBody, kMuted, 984);
        if (!meta.episode.empty())
            print(299, 332, "Episode: " + meta.episode, fontBody, kMuted, 984);
        std::string overview = meta.overview.empty() ? item.description : meta.overview;
        if (overview.empty())
            overview = "Keine Beschreibung vorhanden";
        std::replace(overview.begin(), overview.end(), '\n', ' ');
        std::istringstream words(overview);
        std::string line, word;
        int y = 388;
        int lineCount = 0;
        while (words >> word && lineCount < 8) {
            std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && fontBody && fontBody->Width(candidate.c_str()) > X(950)) {
                print(299, y, line, fontBody, kText, 961);
                y += 33;
                ++lineCount;
                line = word;
            } else {
                line = std::move(candidate);
            }
        }
        if (!line.empty() && lineCount < 8)
            print(299, y, line, fontBody, kText, 961);
        print(299, 662, "Info oder Zurück: Schließen", fontSmall, kFocus);
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
        if (!rec || !browserDetails) return;
        const RecordingMeta &meta = metaFor(*rec);
        panel(267, 157, 1086, 555, true, kSurface);
        const std::string title = meta.title.empty() ? rec->baseName : meta.title;
        print(299, 186, title, fontTitle, kText, 1000);
        print(299, 255, "Aufnahme · " + rec->time, fontSmall, kFocus);
        if (!meta.episode.empty())
            print(299, 301, meta.episode, fontBody, kText, 1000);
        if (!meta.release.empty())
            print(299, 334, "Erscheinung: " + meta.release, fontSmall, kMuted);
        if (!meta.genres.empty()) {
            std::string genres;
            for (const auto &genre : meta.genres)
                genres += (genres.empty() ? "" : " · ") + genre;
            print(299, 366, genres, fontSmall, kFocus, 950);
        }
        std::string overview = meta.overview.empty() ? rec->description : meta.overview;
        if (overview.empty()) overview = "Keine Beschreibung vorhanden";
        std::replace(overview.begin(), overview.end(), '\n', ' ');
        std::istringstream words(overview);
        std::string line, word;
        int y = 417;
        while (words >> word && y < 642) {
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && fontBody && fontBody->Width(candidate.c_str()) > X(975)) {
                print(299, y, line, fontBody, kText, 987);
                line = word;
                y += 31;
            } else line = candidate;
        }
        if (!line.empty() && y <= 646)
            print(299, y, line, fontBody, kText, 987);
        print(299, 680, "Blau / Info / Zurück: Schließen", fontSmall, kFocus);
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

    void drawRecordingBrowser()
    {
        fill(0, 0, 1599, 899, kBackground);
        fill(0, 0, 1599, 76, kSurface);
        print(33, 18, "VDR", fontTitle);
        print(117, 18, "Suite", fontTitle, kFocus);
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
            const int y = 236 + (i / columns) * 187;
            const bool focus = !nav.sideFocused && !recordingBrowser.tabsFocused &&
                               index == recordingBrowser.selected;
            panel(x, y, 224, 167, focus, kCardAlt);
            if (entry.kind != suiteosd::BrowserEntry::Kind::Recording) {
                fill(x + 12, y + 12, x + 211, y + 93, kLine);
                print(x + 27, y + 26,
                      entry.kind == suiteosd::BrowserEntry::Kind::Folder ?
                      "ORDNER" : "GENRE", fontBody, kFocus);
                print(x + 16, y + 105, entry.title, fontBody, kText, 197);
                print(x + 16, y + 143, std::to_string(entry.count) + " Aufnahmen",
                      fontSmall, kMuted, 194);
                continue;
            }
            const Recording *rec = findRecording(entry.fileName);
            if (!rec) continue;
            const RecordingMeta &meta = metaFor(*rec);
            fill(x + 13, y + 10, x + 211, y + 106, kLine);
            if (meta.artwork.valid()) {
                const suiteosd::Artwork &art = meta.artwork;
                cImage image(cSize(art.width, art.height), art.argb.data());
                const int centerX = X(x + 112) - art.width / 2;
                const int centerY = Y(y + 58) - art.height / 2;
                osd->DrawImage(cPoint(centerX, centerY), image);
            } else
                print(x + 27, y + 37, "AUFNAHME", fontBody, kMuted);
            print(x + 15, y + 113, meta.title.empty() ? entry.title : meta.title,
                  fontBody, kText, 192);
            print(x + 15, y + 144,
                  meta.episode.empty() ? rec->time : meta.episode,
                  fontSmall, kMuted, 194);
        }
        if (recordingBrowser.entries.empty())
            print(178, 345, "Keine Aufnahmen in dieser Ansicht", fontBody, kMuted, 922);

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
            print(1217, 211, "TV-Vorschau nicht verfügbar", fontBody, kMuted, 300);
            print(1217, 254, "Sender läuft unverändert weiter", fontSmall, kMuted, 298);
        }
        print(1202, 365, data.live.channel.empty() ? "Live-TV" : data.live.channel,
              fontBody, kText, 322);
        print(1202, 403, data.live.title, fontSmall, kMuted, 322);
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
        print(885, 848, "Blau: Info", fontSmall);
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
        case 0: return &data.live;
        case 1:
            return nav.card[1] >= 0 && nav.card[1] < int(data.now.size()) ?
                &data.now[nav.card[1]] : nullptr;
        case 2:
            return nav.card[2] >= 0 && nav.card[2] < int(data.next.size()) ?
                &data.next[nav.card[2]] : nullptr;
        default: return nullptr;
        }
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
        print(301, 675, "Gelb / Info / Zurück: Schließen", fontSmall, kFocus);
    }

    void drawSidebar()
    {
        // Expanded sidebar overlays the content; it never permanently
        // consumes additional space. All descriptions appear together.
        const int right = nav.sideFocused ? 270 : 99;
        panel(18, 90, right - 18, 726, false, kSurface);
        for (int i = 0; i < int(kSidebar.size()); ++i) {
            const int y = 107 + i * 61;
            const bool selected = nav.sideFocused && nav.sideIndex == i;
            if (selected)
                panel(31, y, right - 51, 48, true, kHighlight);
            else if (!nav.sideFocused && i ==
                     (settingsActive ? 10 : timelineActive ? 2 : recordingsActive ? 3 : 0))
                panel(31, y, 52, 48, false, kHighlight);
            print(41, y + 13, kSymbols[i], fontSmall, kText, 56);
            if (nav.sideFocused)
                print(102, y + 11, kSidebar[i], fontBody, kText, 145);
        }
    }

    void drawFooter()
    {
        fill(17, 836, 1582, 891, kSurface);
        print(42, 852, "Pfeile: Navigation", fontSmall);
        print(365, 852, "OK: Auswählen", fontSmall);
        print(610, 852, "Zurück: TV", fontSmall);
        print(850, 852, "Rot: Aufnahmen · Grün: EPG · Gelb: Info · Blau: Timer", fontSmall);
        if (!notice.empty()) {
            fill(305, 790, 1570, 832, kSurface);
            print(329, 801, notice, fontSmall, kFocus, 1210);
        }
    }

    void draw()
    {
        if (!osd)
            return;
        if (recordingsActive) {
            drawRecordingBrowser();
            osd->Flush();
            return;
        }
        if (timelineActive) {
            drawTimeline();
            osd->Flush();
            return;
        }
        fill(0, 0, 1599, 899, kBackground);
        drawHeader();
        const int scroll = nav.firstVisibleSection == 2 ? 402 : 0;
        drawHero(scroll);
        drawPrograms(1, 394 - scroll, "Was läuft jetzt", data.now);
        drawPrograms(2, 614 - scroll, "Was läuft als nächstes", data.next);
        drawRecordings(834 - scroll);
        drawFooter();
        drawSidebar();
        drawRecordingDetails();
        drawHomeProgrammeInfo();
        osd->Flush();
    }

    eOSState action(suiteosd::Activation a)
    {
        switch (a) {
        case suiteosd::Activation::Hero:
            return osEnd; // reveal the currently tuned broadcast
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
            notice = "Sendungsdetails und Timer folgen mit der Suite-Integration.";
            break;
        case suiteosd::Activation::Recording:
            if (nav.card[3] >= 0 && nav.card[3] < int(data.recordings.size()) &&
                cDevice::PrimaryDevice() &&
                !data.recordings[nav.card[3]].fileName.empty()) {
                cReplayControl::SetRecording(data.recordings[nav.card[3]].fileName.c_str());
                return osReplay; // VDR launches its own canonical replay control
            }
            notice = "Aufnahmewiedergabe aktuell nicht verfügbar";
            break;
        case suiteosd::Activation::Module: {
            // VDR handles direct-menu keys in its main loop and creates
            // its own cMenuMain submenus. Returning osSchedule/osTimers
            // from a plain cOsdObject would NOT open them.
            eKeys nativeKey = kNone;
            switch (nav.sideIndex) {
            case 1: return osEnd;       // Live-TV: return to current broadcast
            case 2: openTimeline(); break; // Native VDR schedule, Suite Web timeline design
            case 3: openRecordingBrowser(); return osContinue; // Native Suite recording grid
            case 4: nativeKey = kTimers; break;     // Timers
            case 10: nativeKey = kSetup; break;     // VDR settings
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
    ~cSuiteHome() override
    {
        restoreLivePreview();
        delete osd;
        delete fontTitle;
        delete fontLarge;
        delete fontBody;
        delete fontSmall;
    }

    void Show() override
    {
        data = readVdrSnapshot();
        width = cOsd::OsdWidth();
        height = cOsd::OsdHeight();
        osd = cOsdProvider::NewOsd(cOsd::OsdLeft(), cOsd::OsdTop());
        if (!osd)
            return;
        const tArea fullArea = {0, 0, width - 1, height - 1, 32};
        if (osd->SetAreas(&fullArea, 1) != oeOk) {
            esyslog("suiteosd: TrueColor OSD area unavailable; cannot render Home");
            return;
        }
        fontTitle = cFont::CreateFont("Sans:Bold", std::max(20, Y(50)));
        fontLarge = cFont::CreateFont("Sans:Bold", std::max(16, Y(29)));
        fontBody = cFont::CreateFont("Sans", std::max(14, Y(23)));
        fontSmall = cFont::CreateFont("Sans", std::max(12, Y(19)));
        draw();
    }

    eOSState ProcessKey(eKeys key) override
    {
        const eKeys base = static_cast<eKeys>(key & ~k_Repeat);
        if (base == kRecordings) {
            if (!recordingsActive) openRecordingBrowser();
            draw();
            return osContinue;
        }
        if (recordingsActive)
            return browserKey(base);
        if (timelineActive)
            return timelineKey(base);
        if (homeProgrammeDetails) {
            if (base == kYellow || base == kInfo || base == kBack || base == kOk) {
                homeProgrammeDetails = false;
                draw();
            }
            return osContinue;
        }
        if (recordingDetails) {
            if (base == kInfo || base == kBack) {
                recordingDetails = false;
                draw();
            }
            return osContinue;
        }
        if ((base == kInfo || base == kYellow) && !nav.sideFocused) {
            if (nav.section == 3 && nav.card[3] >= 0 &&
                nav.card[3] < int(data.recordings.size()))
                recordingDetails = true;
            else if (selectedHomeProgramme())
                homeProgrammeDetails = true;
            draw();
            return osContinue;
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
            data = readVdrSnapshot();
            recordingMeta.clear(); // explicit refresh checks for updated scraper results
            nav.KeepVisible(7, count(nav.section));
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
        case kBack: return nav.Move(suiteosd::Direction::Back, count(nav.section), 7) ==
                           suiteosd::Activation::Close ? osEnd : (draw(), osContinue);
        default: return osContinue;
        }
        notice.clear();
        // Navigation changes the section before clamping the card focus.
        // Use the destination section's actual item count, not the old row's.
        int target = nav.section;
        if (!nav.sideFocused && direction == suiteosd::Direction::Down)
            target = std::min(3, target + 1);
        if (!nav.sideFocused && direction == suiteosd::Direction::Up)
            target = std::max(0, target - 1);
        if (nav.sideFocused && direction == suiteosd::Direction::Select &&
            nav.sideIndex == 0)
            target = 0;
        const eOSState result = action(nav.Move(direction, count(target), 7));
        if (result != osContinue)
            return result;
        draw();
        return osContinue;
    }
};

class cPluginSuiteOsd final : public cPlugin {
public:
    const char *Version() override { return VERSION; }
    const char *Description() override { return tr(DESCRIPTION); }
    const char *MainMenuEntry() override { return tr("VDR Suite"); }
    cOsdObject *MainMenuAction() override { return new cSuiteHome(); }
};
} // namespace

VDRPLUGINCREATOR(cPluginSuiteOsd);
