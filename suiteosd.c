/*
 * VDR Suite OSD – first native Home screen.
 *
 * Draws real VDR EPG/recording data with a remote-only scrollable home.
 * This is not a cSkin replacement or an embedded HTML/video player.
 */
#include "navigation.h"

#include <vdr/channels.h>
#include <vdr/device.h>
#include <vdr/epg.h>
#include <vdr/font.h>
#include <vdr/osd.h>
#include <vdr/osdbase.h>
#include <vdr/plugin.h>
#include <vdr/recording.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

static const char *VERSION = "0.2.0";
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

struct Programme {
    int channelNumber = 0;
    std::string channel;
    std::string title;
    std::string time;
    int progress = 0;
};
struct Recording {
    std::string title;
    std::string time;
};
struct Snapshot {
    Programme live;
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
                if (snapshot.live.channel.empty() || ch->Number() == liveNumber)
                    snapshot.live = current;
                // Keep the initial snapshot bounded; the model still scrolls
                // through all items in this bounded set.
                if (snapshot.now.size() < 100)
                    snapshot.now.push_back(current);
                if (snapshot.next.size() < 100)
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
                r.time = formatTime(rec->Start());
                snapshot.recordings.push_back(std::move(r));
                if (snapshot.recordings.size() >= 100)
                    break;
            }
        }
    }
    return snapshot;
}

const std::array<const char *, 10> kSidebar = {{
    "Home", "Live-TV", "EPG", "Aufnahmen", "Timer",
    "Suchtimer", "HbbTV", "Teletext", "Bilder", "Media Tools"
}};
const std::array<const char *, 10> kSymbols = {{
    "H", "TV", "E", "REC", "T", "S", "Hb", "TXT", "IMG", "M"
}};

class cSuiteHome final : public cOsdObject {
private:
    cOsd *osd = nullptr;
    cFont *fontTitle = nullptr;
    cFont *fontLarge = nullptr;
    cFont *fontBody = nullptr;
    cFont *fontSmall = nullptr;
    suiteosd::Navigation nav;
    Snapshot data;
    std::string notice;
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
        print(189, y + 134, "JETZT", fontSmall, kFocus);
        print(189, y + 160, data.live.title.empty() ? "EPG nicht verfügbar" : data.live.title,
              fontLarge, kText, 660);
        print(189, y + 205, data.live.time, fontSmall, kMuted);
        fill(189, y + 242, 805, y + 250, kLine);
        fill(189, y + 242, 189 + (616 * data.live.progress / 100), y + 250, kFocus);
        panel(875, y + 18, 652, 224, false, kCardAlt);
        print(928, y + 70, "Live-TV", fontLarge, kText);
        print(928, y + 118, "Bildausgabe durch VDR / Ausgabeplugin", fontBody, kMuted, 560);
        print(928, y + 157, "Kein separater Streaming-Player", fontSmall, kMuted);
    }

    void drawPrograms(int section, int y, const char *heading, const std::vector<Programme> &items)
    {
        if (y < -170 || y > 815)
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

    void drawRecordings(int y)
    {
        if (y < -170 || y > 815)
            return;
        print(155, y, "Aufnahmen (" + std::to_string(data.recordings.size()) +
              (data.recordings.size() >= 100 ? "+" : "") + ")", fontLarge);
        const int cw = 188;
        const int first = nav.firstCard[3];
        for (int col = 0; col < 7 && first + col < int(data.recordings.size()); ++col) {
            const Recording &item = data.recordings[first + col];
            const int x = 157 + col * 200;
            const bool focus = !nav.sideFocused && nav.section == 3 &&
                               nav.card[3] == first + col;
            panel(x, y + 48, cw, 160, focus, kCardAlt);
            fill(x + 12, y + 59, x + cw - 12, y + 112, kLine);
            print(x + 25, y + 70, "AUFNAHME", fontSmall, kMuted);
            print(x + 13, y + 122, item.title, fontBody, kText, cw - 24);
            print(x + 13, y + 161, item.time, fontSmall, kMuted);
        }
        if (data.recordings.empty())
            print(159, y + 94, "Keine Aufnahmen vorhanden", fontBody, kMuted);
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
            else if (!nav.sideFocused && i == 0)
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
        print(850, 852, "Blau: Aktualisieren", fontSmall);
        if (!notice.empty()) {
            fill(305, 790, 1570, 832, kSurface);
            print(329, 801, notice, fontSmall, kFocus, 1210);
        }
    }

    void draw()
    {
        if (!osd)
            return;
        fill(0, 0, 1599, 899, kBackground);
        drawHeader();
        const int scroll = nav.firstVisibleSection == 2 ? 402 : 0;
        drawHero(scroll);
        drawPrograms(1, 394 - scroll, "Was läuft jetzt", data.now);
        drawPrograms(2, 614 - scroll, "Was läuft als nächstes", data.next);
        drawRecordings(834 - scroll);
        drawFooter();
        drawSidebar();
        osd->Flush();
    }

    void action(suiteosd::Activation a)
    {
        switch (a) {
        case suiteosd::Activation::Hero:
            notice = "Live-TV läuft über das VDR-Ausgabeplugin. Zurück schließt das OSD.";
            break;
        case suiteosd::Activation::ChannelNow:
        case suiteosd::Activation::ChannelNext:
            notice = "Senderauswahl: Umschalten folgt mit der VDR-Anbindung.";
            break;
        case suiteosd::Activation::Recording:
            notice = "Aufnahmewiedergabe folgt in der Player-Integration.";
            break;
        case suiteosd::Activation::Module:
            notice = "Dieses Suite-Modul folgt in einem weiteren Integrationsschritt.";
            break;
        default: break;
        }
    }

public:
    ~cSuiteHome() override
    {
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
        if (base == kBlue) {
            data = readVdrSnapshot();
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
        if (nav.sideFocused && direction == suiteosd::Direction::Select) {
            if (nav.sideIndex <= 1) target = 0;
            else if (nav.sideIndex == 2) target = 1;
            else if (nav.sideIndex == 3) target = 3;
        }
        action(nav.Move(direction, count(target), 7));
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
