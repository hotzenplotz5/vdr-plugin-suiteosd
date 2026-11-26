/*
 * VDR Suite Skin: TV and replay OSD.
 * The VDR core retains remote-key handling and playback ownership.
 * TVScraper is an optional, local VDR service; no external API is involved.
 */
#include "artwork_decode.h"
#include "logo_locator.h"
#include "skin_registration.h"
#include "vendor/tvscraper/services.h"

#include <vdr/epg.h>
#include <vdr/osd.h>
#include <vdr/plugin.h>
#include <vdr/recording.h>
#include <vdr/skins.h>
#include <vdr/font.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace suiteosd {
cSkinDisplayMenu *CreateSuiteMenu();
namespace {
constexpr tColor bg = 0xFF091729;
constexpr tColor dark = 0xF6071426;
constexpr tColor accent = 0xFF08D2F6;
constexpr tColor white = 0xFFF7FAFF;
constexpr tColor muted = 0xFFA9BBD0;
constexpr tColor line = 0xFF2A4961;

std::string clockTime(time_t t)
{
    char buffer[12] = {};
    tm local = {};
    if (localtime_r(&t, &local))
        std::strftime(buffer, sizeof(buffer), "%H:%M", &local);
    return buffer;
}

std::string joinGenres(const std::vector<std::string> &genres)
{
    std::string result;
    for (const std::string &name : genres) {
        if (!result.empty()) result += "  ·  ";
        result += name;
        if (result.size() > 90) break;
    }
    return result;
}

struct Media {
    std::string title;
    std::string subtitle;
    std::string description;
    std::string release;
    std::string genres;
    std::string fanartPath;
    int season = 0;
    int episode = 0;
    int runtime = 0;
    float rating = 0.0f;

    void fromScraper(cScraperVideo *video)
    {
        if (!video) return;
        video->getOverview(&title, &subtitle, &release, &runtime,
                           nullptr, nullptr, nullptr);
        std::vector<std::string> genreValues;
        video->getMovieOrTv(nullptr, nullptr, nullptr, &description,
                            &genreValues, nullptr, nullptr, nullptr,
                            nullptr, nullptr, &rating, nullptr, nullptr,
                            nullptr, nullptr, nullptr, nullptr, nullptr,
                            nullptr, nullptr, nullptr);
        genres = joinGenres(genreValues);
        season = video->getSeasonNumber();
        episode = video->getEpisodeNumber();
        if (video->getVideoType() == tSeries) {
            std::string episodeOverview;
            int episodeDuration = 0;
            if (video->getEpisode(nullptr, &episodeOverview, nullptr, nullptr,
                                  &episodeDuration, nullptr, nullptr, nullptr)) {
                if (!episodeOverview.empty()) description = std::move(episodeOverview);
                if (episodeDuration > 0) runtime = episodeDuration;
            }
        }
        fanartPath = video->getImage(
            cImageLevels(eImageLevel::episodeMovie,
                         eImageLevel::seasonMovie,
                         eImageLevel::tvShowCollection),
            cOrientations(eOrientation::landscape, eOrientation::banner), true).path;
    }

    std::string facts() const
    {
        std::string s = genres;
        if (!release.empty()) s += (s.empty() ? "" : "  ·  ") + release;
        if (runtime > 0) s += "  ·  " + std::to_string(runtime) + " Min";
        if (season > 0) {
            s += "  ·  S" + std::to_string(season);
            if (episode > 0) s += " E" + std::to_string(episode);
        }
        if (rating > 0.0f) {
            char ratingText[28];
            std::snprintf(ratingText, sizeof(ratingText), "  ·  %.1f/10", rating);
            s += ratingText;
        }
        return s;
    }
};

// Owns a transparent OSD rectangle at the bottom of the live TV picture.
// No video surface or player is created or rescaled.
class Canvas {
    cOsd *osd = nullptr;
    cFont *large = nullptr;
    cFont *medium = nullptr;
    cFont *small = nullptr;
    int width = 0;
    int height = 0;
    std::string lastImagePath;
    Artwork cachedImage;
    Artwork cachedLogo;

public:
    explicit Canvas(int nominalHeight)
    {
        width = std::max(400, cOsd::OsdWidth());
        const int fullHeight = std::max(300, cOsd::OsdHeight());
        height = std::clamp(fullHeight * nominalHeight / 900, 115, fullHeight);
        osd = cOsdProvider::NewOsd(cOsd::OsdLeft(),
                                   cOsd::OsdTop() + fullHeight - height);
        if (!osd) return;
        const tArea area = {0, 0, width - 1, height - 1, 32};
        if (osd->SetAreas(&area, 1) != oeOk) {
            delete osd;
            osd = nullptr;
            esyslog("suiteosd skin: TrueColor OSD unavailable");
            return;
        }
        large = cFont::CreateFont("Sans:Bold", std::max(20, height / 7));
        medium = cFont::CreateFont("Sans", std::max(15, height / 9));
        small = cFont::CreateFont("Sans", std::max(13, height / 11));
    }

    ~Canvas()
    {
        delete osd;
        delete large;
        delete medium;
        delete small;
    }

    bool valid() const { return osd != nullptr; }
    int W(int x) const { return x * width / 1600; }
    int H(int y) const { return y * height / 230; }
    void rect(int x0, int y0, int x1, int y1, tColor color)
    {
        if (osd) osd->DrawRectangle(W(x0), H(y0), W(x1), H(y1), color);
    }
    void label(int x, int y, const std::string &s, int level = 1,
               tColor fg = white, int maxWidth = 0)
    {
        if (!osd || s.empty()) return;
        const cFont *font = level == 0 ? large : (level == 2 ? small : medium);
        if (font) osd->DrawText(W(x), H(y), s.c_str(), fg, clrTransparent,
                                font, maxWidth ? W(maxWidth) : 0);
    }
    void begin()
    {
        if (!osd) return;
        osd->DrawRectangle(0, 0, width - 1, height - 1, clrTransparent);
        rect(18, 12, 1580, 214, bg);
        rect(18, 12, 1580, 15, accent);
    }
    void picture(const std::string &path)
    {
        if (!osd || path.empty()) return;
        // Flush() is called frequently during replay. Reuse the small decoded
        // image until TVScraper returns a different local image path.
        if (lastImagePath != path) {
            lastImagePath = path;
            cachedImage = LoadArtwork(path, W(270), H(156));
        }
        if (!cachedImage.valid()) return;
        cImage img(cSize(cachedImage.width, cachedImage.height), cachedImage.argb.data());
        osd->DrawImage(cPoint(W(1275) + (W(270) - cachedImage.width) / 2,
                              H(31) + (H(156) - cachedImage.height) / 2), img);
    }
    void setLogo(const std::string &path)
    {
        cachedLogo = path.empty() ? Artwork{} : LoadArtwork(path, W(245), H(100));
        OpaqueArtwork(cachedLogo, bg);
    }
    bool logo()
    {
        if (!osd || !cachedLogo.valid()) return false;
        cImage img(cSize(cachedLogo.width, cachedLogo.height), cachedLogo.argb.data());
        osd->DrawImage(cPoint(W(52) + (W(245) - cachedLogo.width) / 2,
                              H(30) + (H(100) - cachedLogo.height) / 2), img);
        return true;
    }
    void progress(int x0, int y, int x1, int value)
    {
        const int pos = std::clamp(value, 0, 100);
        rect(x0, y, x1, y + 8, line);
        rect(x0, y, x0 + (x1 - x0) * pos / 100, y + 8, accent);
    }
    void clear()
    {
        if (!osd) return;
        osd->DrawRectangle(0, 0, width - 1, height - 1, clrTransparent);
        osd->Flush();
    }
    void flush() { if (osd) osd->Flush(); }
};

class DisplayChannel final : public cSkinDisplayChannel {
    Canvas canvas;
    bool withInfo;
    std::string channel, channelNumber;
    std::string present, following, times, nextTimes;
    std::string message;
    Media media;
    int progressValue = 0;
public:
    explicit DisplayChannel(bool info) : canvas(230), withInfo(info) {}
    void SetChannel(const cChannel *ch, int number) override
    {
        channel = ch && ch->Name() ? ch->Name() : "Kein Sender";
        channelNumber = std::to_string(number > 0 ? number : (ch ? ch->Number() : 0));
        canvas.setLogo(ch ? FindChannelLogo(channel, ch->ShortName() ? ch->ShortName() : "") : "");
        // Invalidate preceding artwork when the user switches channels.
        media = Media{};
        present.clear();
        following.clear();
        times.clear();
        nextTimes.clear();
        progressValue = 0;
    }
    void SetEvents(const cEvent *now, const cEvent *next) override
    {
        media = Media{};
        present = now && now->Title() ? now->Title() : "Keine Programminformation";
        following = next && next->Title() ? next->Title() : "Keine Folgesendung";
        times = now ? clockTime(now->StartTime()) + "–" + clockTime(now->EndTime()) : "";
        nextTimes = next ? clockTime(next->StartTime()) + "–" + clockTime(next->EndTime()) : "";
        progressValue = now && now->Duration() > 0 ?
            std::clamp(int(100 * (time(nullptr) - now->StartTime()) / now->Duration()), 0, 100) : 0;
        if (now && now->ShortText()) media.subtitle = now->ShortText();
        if (now && now->Description()) media.description = now->Description();
        cPlugin *scraper = cPluginManager::GetPlugin("tvscraper");
        if (scraper && now) {
            cGetScraperVideo request(now, nullptr);
            if (scraper->Service("GetScraperVideo", &request) && request.m_scraperVideo)
                media.fromScraper(request.m_scraperVideo.get());
        }
    }
    void SetMessage(eMessageType, const char *text) override
    {
        message = text ? text : "";
    }
    void Flush() override
    {
        if (!canvas.valid()) return;
        canvas.begin();
        if (!canvas.logo())
            canvas.label(52, 55, channel, 0, white, 268);
        canvas.label(52, 140, channelNumber + "  " + channel, 1, white, 268);
        canvas.rect(338, 29, 341, 197, accent);
        canvas.label(371, 26, present, 0, white, 848);
        if (withInfo) {
            canvas.label(372, 86, times +
                (media.subtitle.empty() ? "" : "  ·  " + media.subtitle), 1, muted, 840);
            canvas.progress(372, 128, 1121, progressValue);
            canvas.label(372, 155, "ALS NÄCHSTES  " + following + "  " + nextTimes,
                         2, white, 848);
            if (!media.facts().empty())
                canvas.label(372, 193, media.facts(), 2, muted, 848);
        }
        canvas.picture(media.fanartPath);
        if (!message.empty()) canvas.label(48, 193, message, 2, accent, 285);
        canvas.flush();
    }
};

class DisplayReplay final : public cSkinDisplayReplay {
    Canvas canvas;
    std::string title;
    std::string currentTime, totalTime, jump, message;
    Media media;
    int current = 0, total = 0;
    bool play = true, forward = true;
    int speed = -1;
public:
    explicit DisplayReplay(bool modeOnly) : canvas(modeOnly ? 135 : 230) {}
    void SetRecording(const cRecording *recording) override
    {
        media = Media{};
        title = recording && recording->Name() ? recording->Name() : "Wiedergabe";
        if (recording && recording->Info()) {
            if (recording->Info()->Title()) title = recording->Info()->Title();
            if (recording->Info()->ShortText())
                media.subtitle = recording->Info()->ShortText();
            if (recording->Info()->Description())
                media.description = recording->Info()->Description();
        }
        cPlugin *scraper = cPluginManager::GetPlugin("tvscraper");
        if (scraper && recording) {
            cGetScraperVideo request(nullptr, recording);
            if (scraper->Service("GetScraperVideo", &request) && request.m_scraperVideo)
                media.fromScraper(request.m_scraperVideo.get());
        }
    }
    void SetTitle(const char *value) override { if (value) title = value; }
    void SetMode(bool Play, bool Forward, int Speed) override
    {
        play = Play; forward = Forward; speed = Speed;
    }
    void SetProgress(int Current, int Total) override { current = Current; total = Total; }
    void SetCurrent(const char *value) override { currentTime = value ? value : ""; }
    void SetTotal(const char *value) override { totalTime = value ? value : ""; }
    void SetJump(const char *value) override { jump = value ? value : ""; }
    void SetMessage(eMessageType, const char *value) override { message = value ? value : ""; }
    void Flush() override
    {
        if (!canvas.valid()) return;
        canvas.begin();
        canvas.label(49, 41, "VDR Suite", 0, accent, 269);
        canvas.label(49, 97, !play ? "PAUSE" : speed >= 0 ?
                     (forward ? "VORLAUF" : "RÜCKLAUF") : "WIEDERGABE",
                     2, white, 269);
        canvas.rect(339, 29, 342, 199, accent);
        canvas.label(367, 28, media.title.empty() ? title : media.title,
                     0, white, 869);
        canvas.label(369, 84, media.subtitle, 1, muted, 860);
        canvas.label(369, 120, currentTime + " / " + totalTime, 1, white, 741);
        canvas.progress(369, 165, 1129, total > 0 ? int(100LL * current / total) : 0);
        canvas.label(369, 192, !jump.empty() ? jump :
                     (!message.empty() ? message : media.facts()), 2, muted, 830);
        canvas.picture(media.fanartPath);
        canvas.flush();
    }
};


// Native OSD.2.2 overlays. VDR remains the owner of volume, audio track
// selection, timeout policy and notifications; these classes only draw them.
class DisplayVolume final : public cSkinDisplayVolume {
    Canvas canvas;
    int current = 0;
    int total = 0;
    bool mute = false;
public:
    DisplayVolume() : canvas(145) {}
    bool valid() const { return canvas.valid(); }
    void SetVolume(int Current, int Total, bool Mute) override
    {
        current = Current;
        total = Total;
        mute = Mute;
    }
    void Flush() override
    {
        if (!canvas.valid()) return;
        canvas.begin();
        canvas.label(53, 34, "VDR Suite", 0, accent, 278);
        canvas.rect(333, 32, 337, 196, accent);
        canvas.label(365, 30, mute ? "TON AUS" : "LAUTSTÄRKE", 0, white, 420);
        const int percent = total > 0 ?
            std::clamp(int(100LL * current / total), 0, 100) : 0;
        canvas.progress(368, 135, 1190, mute ? 0 : percent);
        canvas.label(1220, 115, mute ? "MUTE" : std::to_string(percent) + " %",
                     1, mute ? 0xFFFF8585 : accent, 300);
        canvas.flush();
    }
};

class DisplayTracks final : public cSkinDisplayTracks {
    Canvas canvas;
    std::string heading;
    std::vector<std::string> tracks;
    int selected = 0;
    int audioChannel = -1;
public:
    DisplayTracks(const char *Title, int NumTracks, const char *const *Tracks)
      : canvas(330), heading(Title ? Title : "Audio / Sprache")
    {
        const int count = std::max(0, NumTracks);
        tracks.reserve(count);
        for (int i = 0; i < count; ++i)
            tracks.emplace_back(Tracks && Tracks[i] ? Tracks[i] : "");
    }
    bool valid() const { return canvas.valid(); }
    void SetTrack(int Index, const char *const *Tracks) override
    {
        selected = std::clamp(Index, 0, std::max(0, int(tracks.size()) - 1));
        if (Tracks) {
            for (size_t i = 0; i < tracks.size(); ++i)
                tracks[i] = Tracks[i] ? Tracks[i] : "";
        }
    }
    void SetAudioChannel(int AudioChannel) override
    {
        audioChannel = AudioChannel;
    }
    void Flush() override
    {
        if (!canvas.valid()) return;
        canvas.begin();
        canvas.label(48, 29, "VDR Suite", 0, accent, 281);
        canvas.label(48, 112, heading, 1, white, 285);
        canvas.rect(330, 31, 334, 201, accent);
        const int total = int(tracks.size());
        const int first = std::clamp(selected - 2, 0, std::max(0, total - 5));
        for (int row = 0; row < 5 && first + row < total; ++row) {
            const int index = first + row;
            const int y = 27 + row * 35;
            if (index == selected)
                canvas.rect(362, y - 3, 1205, y + 28, 0xFF075E8B);
            else
                canvas.rect(362, y + 28, 1205, y + 29, 0xFF29465F);
            canvas.label(385, y, std::to_string(index + 1) + ".  " + tracks[index],
                         1, index == selected ? white : muted, 830);
        }
        std::string detail = total ? std::to_string(selected + 1) + " / " +
                                      std::to_string(total) + " Spuren" : "Keine Tonspuren";
        if (audioChannel == 0) detail += " · Stereo";
        if (audioChannel == 1) detail += " · Links";
        if (audioChannel == 2) detail += " · Rechts";
        canvas.label(49, 182, detail, 2, accent, 290);
        canvas.flush();
    }
};

class DisplayMessage final : public cSkinDisplayMessage {
    Canvas canvas;
    eMessageType type = mtInfo;
    std::string message;
public:
    DisplayMessage() : canvas(140) {}
    bool valid() const { return canvas.valid(); }
    void SetMessage(eMessageType Type, const char *Text) override
    {
        type = Type;
        message = Text ? Text : "";
    }
    void Flush() override
    {
        if (!canvas.valid()) return;
        if (message.empty()) {
            canvas.clear();
            return;
        }
        canvas.begin();
        const tColor indicator = type == mtError ? 0xFFFF8585 :
                                 type == mtWarning ? 0xFFF7CE68 :
                                 type == mtInfo ? accent : muted;
        const char *kind = type == mtError ? "FEHLER" :
                           type == mtWarning ? "WARNUNG" :
                           type == mtInfo ? "HINWEIS" : "STATUS";
        canvas.label(49, 44, "VDR Suite", 0, accent, 280);
        canvas.rect(330, 33, 335, 192, indicator);
        canvas.label(366, 28, kind, 2, indicator, 220);
        canvas.label(365, 94, message, 1, white, 1130);
        canvas.flush();
    }
};

class SuiteSkin final : public cSkin {
    // A provider without TrueColor keeps the original VDR skin as fallback.
    cSkin *fallback()
    {
        for (cSkin *skin = Skins.First(); skin; skin = Skins.Next(skin))
            if (skin != this) return skin;
        return nullptr;
    }
public:
    SuiteSkin() : cSkin("suiteosd") {}
    const char *Description() override { return "VDR Suite"; }
    cSkinDisplayChannel *DisplayChannel(bool withInfo) override
    { return new class DisplayChannel(withInfo); }
    cSkinDisplayMenu *DisplayMenu() override
    {
        if (cSkinDisplayMenu *menu = CreateSuiteMenu()) return menu;
        return fallback() ? fallback()->DisplayMenu() : nullptr;
    }
    cSkinDisplayReplay *DisplayReplay(bool modeOnly) override
    { return new class DisplayReplay(modeOnly); }
    cSkinDisplayVolume *DisplayVolume() override
    {
        auto *display = new class DisplayVolume();
        if (display->valid()) return display;
        delete display;
        return fallback() ? fallback()->DisplayVolume() : nullptr;
    }
    cSkinDisplayTracks *DisplayTracks(const char *title, int count,
                                     const char *const *tracks) override
    {
        auto *display = new class DisplayTracks(title, count, tracks);
        if (display->valid()) return display;
        delete display;
        return fallback() ? fallback()->DisplayTracks(title, count, tracks) : nullptr;
    }
    cSkinDisplayMessage *DisplayMessage() override
    {
        auto *display = new class DisplayMessage();
        if (display->valid()) return display;
        delete display;
        return fallback() ? fallback()->DisplayMessage() : nullptr;
    }
};
} // namespace

void RegisterSuiteSkin()
{
    RegisterSkinOnce<SuiteSkin>(Skins, "suiteosd");
}
} // namespace suiteosd
