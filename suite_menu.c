/*
 * VDR Suite OSD – native VDR menu renderer (OSD.2.1).
 *
 * Only cSkinDisplayMenu drawing is replaced. VDR, epgsearch and other
 * plugins keep their own menu items, input handling, sorting and actions.
 * Optional TVScraper data is copied under VDR's caller-held list locks;
 * images are decoded only after those calls, in Flush().
 */
#include "artwork_decode.h"
#include "vendor/tvscraper/services.h"

#include <vdr/channels.h>
#include <vdr/epg.h>
#include <vdr/font.h>
#include <vdr/osd.h>
#include <vdr/plugin.h>
#include <vdr/recording.h>
#include <vdr/skins.h>
#include <vdr/timers.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace suiteosd {
namespace {
constexpr tColor kBase = 0xFF050F20;
constexpr tColor kSurface = 0xFF081A30;
constexpr tColor kCard = 0xFF102940;
constexpr tColor kFocusBg = 0xFF075E8B;
constexpr tColor kCyan = 0xFF08D2F6;
constexpr tColor kWhite = 0xFFF6FAFF;
constexpr tColor kMuted = 0xFFA8BBD2;
constexpr tColor kLine = 0xFF29465F;
constexpr tColor kRed = 0xFFF6656B;
constexpr tColor kGreen = 0xFF67DEA1;
constexpr tColor kYellow = 0xFFF7CE68;
constexpr tColor kBlue = 0xFF60BDFF;

std::string timeText(time_t time)
{
    char buf[64] = {};
    tm value = {};
    if (localtime_r(&time, &value))
        std::strftime(buf, sizeof(buf), "%d.%m. %H:%M", &value);
    return buf;
}

std::string shortTime(time_t time)
{
    char buf[16] = {};
    tm value = {};
    if (localtime_r(&time, &value))
        std::strftime(buf, sizeof(buf), "%H:%M", &value);
    return buf;
}

std::string genresText(const std::vector<std::string> &genres)
{
    std::string result;
    for (const auto &genre : genres) {
        if (!result.empty()) result += " · ";
        result += genre;
    }
    return result;
}

struct Preview {
    std::string title;
    std::string subtitle;
    std::string description;
    std::string meta;
    std::string picture;
    bool hasDetail = false;

    void ReadScraper(cScraperVideo *video)
    {
        if (!video) return;
        std::string movieTitle, episodeTitle, release, overview;
        std::vector<std::string> genres;
        int runtime = 0;
        float rating = 0.0f;
        video->getOverview(&movieTitle, &episodeTitle, &release,
                           &runtime, nullptr, nullptr, nullptr);
        video->getMovieOrTv(nullptr, nullptr, nullptr, &overview, &genres,
                            nullptr, nullptr, nullptr, nullptr, nullptr,
                            &rating, nullptr, nullptr, nullptr, nullptr,
                            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (video->getVideoType() == tSeries) {
            std::string episodeDescription;
            int episodeDuration = 0;
            if (video->getEpisode(nullptr, &episodeDescription, nullptr, nullptr,
                                  &episodeDuration, nullptr, nullptr, nullptr)) {
                if (!episodeDescription.empty()) overview = episodeDescription;
                if (episodeDuration > 0) runtime = episodeDuration;
            }
            const int season = video->getSeasonNumber();
            const int episode = video->getEpisodeNumber();
            if (season > 0) {
                meta += "  ·  S" + std::to_string(season);
                if (episode > 0) meta += " E" + std::to_string(episode);
            }
        }
        if (subtitle.empty() && !episodeTitle.empty()) subtitle = episodeTitle;
        if (description.empty() && !overview.empty()) description = overview;
        const std::string genreLine = genresText(genres);
        if (!genreLine.empty()) meta += "  ·  " + genreLine;
        if (!release.empty()) meta += "  ·  " + release;
        if (runtime > 0) meta += "  ·  " + std::to_string(runtime) + " Min";
        if (rating > 0.0f) {
            char buf[32] = {};
            std::snprintf(buf, sizeof(buf), "  ·  %.1f/10", rating);
            meta += buf;
        }
        picture = video->getImage(
            cImageLevels(eImageLevel::episodeMovie,
                         eImageLevel::seasonMovie,
                         eImageLevel::tvShowCollection),
            cOrientations(eOrientation::landscape,
                          eOrientation::banner, eOrientation::portrait),
            true).path;
    }
};

struct Row {
    std::string text;
    bool selectable = false;
    bool current = false;
};

enum class Body { Items, Event, Recording, Text };

class SuiteMenu final : public cSkinDisplayMenu {
    cOsd *osd = nullptr;
    cFont *heading = nullptr;
    cFont *bodyFont = nullptr;
    cFont *small = nullptr;
    cFont *fixedFont = nullptr;
    int width = 1600;
    int height = 900;
    int maxRows = 10;
    std::vector<Row> rows;
    std::string title;
    std::array<std::string, 4> buttons;
    std::string message;
    eMessageType messageType = mtStatus;
    eMenuCategory category = mcUnknown;
    eMenuSortMode sorting = msmUnknown;
    int totalItems = 0, firstItem = 0, currentIndex = -1;
    Body mode = Body::Items;
    Preview preview;
    // The canonical VDR menu item text is generated by the original
    // cOsdItem if SetItemEvent/Timer/Recording/Channel returns false.
    // Carry only its selected item's optional details to SetItem().
    Preview pendingPreview;
    int pendingPreviewIndex = -1;
    std::string rawText;
    bool fixedText = false;
    int firstTextLine = 0;
    std::string lastPicture;
    Artwork cachedPicture;

    int X(int v) const { return v * width / 1600; }
    int Y(int v) const { return v * height / 900; }

    void rect(int x, int y, int w, int h, tColor color)
    {
        if (osd && w > 0 && h > 0)
            osd->DrawRectangle(X(x), Y(y), X(x + w) - 1, Y(y + h) - 1, color);
    }

    void text(int x, int y, const std::string &value, const cFont *font,
              tColor color = kWhite, int maxWidth = 0)
    {
        if (osd && font && !value.empty())
            osd->DrawText(X(x), Y(y), value.c_str(), color, clrTransparent,
                          font, maxWidth > 0 ? X(maxWidth) : 0);
    }

    std::vector<std::string> wrap(const std::string &value, int maxPixels,
                                  const cFont *font) const
    {
        std::vector<std::string> result;
        if (!font || value.empty()) return result;
        std::istringstream input(value);
        std::string paragraph;
        while (std::getline(input, paragraph)) {
            if (paragraph.empty()) {
                result.emplace_back();
                continue;
            }
            std::istringstream words(paragraph);
            std::string line, word;
            while (words >> word) {
                std::string candidate = line.empty() ? word : line + " " + word;
                if (!line.empty() && font->Width(candidate.c_str()) > maxPixels) {
                    result.push_back(line);
                    line = std::move(word);
                } else line = std::move(candidate);
            }
            if (!line.empty()) result.push_back(line);
        }
        return result;
    }

    std::vector<std::string> contentLines() const
    {
        if (!fixedText) {
            return wrap(rawText, X(preview.picture.empty() ? 1406 : 921), bodyFont);
        }
        // Fixed-font views (e.g. technical plugin output) must preserve
        // indentation and whitespace rather than word-wrapping it.
        std::vector<std::string> lines;
        std::istringstream stream(rawText);
        std::string line;
        while (std::getline(stream, line)) lines.push_back(line);
        return lines;
    }

    void drawParagraph(const std::string &value, int x, int y, int w,
                       int bottom, const cFont *font, tColor fg,
                       int lineHeight, int offset = 0)
    {
        const auto lines = wrap(value, X(w), font);
        for (int i = std::max(0, offset); i < int(lines.size()) && y < bottom;
             ++i, y += lineHeight)
            text(x, y, lines[i], font, fg, w);
    }

    void drawPicture(int x, int y, int w, int h)
    {
        if (!osd || preview.picture.empty()) return;
        if (lastPicture != preview.picture) {
            lastPicture = preview.picture;
            cachedPicture = LoadArtwork(preview.picture, X(w), Y(h));
        }
        if (!cachedPicture.valid()) return;
        cImage image(cSize(cachedPicture.width, cachedPicture.height),
                     cachedPicture.argb.data());
        osd->DrawImage(cPoint(X(x) + (X(w) - cachedPicture.width) / 2,
                              Y(y) + (Y(h) - cachedPicture.height) / 2), image);
    }

    void drawChrome()
    {
        rect(0, 0, 1600, 900, kBase);
        rect(0, 0, 1600, 80, kSurface);
        rect(25, 18, 5, 44, kCyan);
        text(47, 15, "VDR", heading, kWhite, 115);
        text(146, 15, "Suite", heading, kCyan, 180);
        text(356, 24, title, bodyFont, kWhite, 960);
        text(1420, 26, shortTime(time(nullptr)), bodyFont, kMuted, 145);
        rect(24, 81, 1551, 2, kLine);

        rect(18, 828, 1564, 62, kSurface);
        constexpr tColor colors[] = {kRed, kGreen, kYellow, kBlue};
        constexpr int x[] = {45, 427, 811, 1195};
        for (int i = 0; i < 4; ++i) {
            if (buttons[i].empty()) continue;
            rect(x[i], 848, 13, 13, colors[i]);
            text(x[i] + 24, 838, buttons[i], small, kWhite, 326);
        }
        if (!message.empty()) {
            const tColor color = messageType == mtError ? kRed :
                                 messageType == mtWarning ? kYellow : kCyan;
            rect(330, 780, 1216, 42, kSurface);
            rect(330, 780, 5, 42, color);
            text(347, 788, message, small, color, 1180);
        }
    }

    void drawTabbedRow(const std::string &value, int x, int y, int w,
                       tColor color)
    {
        size_t start = 0;
        int column = 0;
        const int nominal = X(w);
        while (start <= value.size() && column < 6) {
            const size_t pos = value.find('\t', start);
            const std::string field = value.substr(start, pos == std::string::npos ?
                                                     std::string::npos : pos - start);
            int offset = 0;
            if (column > 0) {
                // Tab() contains VDR's cumulative pixel position, including
                // negative absolute-pixel tab stops and repeated offsets.
                offset = Tab(column);
                if (offset <= 0) offset = column * nominal / 3;
                offset = std::min(offset, nominal - X(48));
            }
            text(x + offset * 1600 / width, y, field, bodyFont, color,
                 std::max(24, w - offset * 1600 / width));
            if (pos == std::string::npos) break;
            start = pos + 1;
            ++column;
        }
    }

    void drawItems()
    {
        rect(25, 106, 1035, 697, kSurface);
        rect(1077, 106, 501, 697, kSurface);
        text(48, 126, title.empty() ? "MENÜ" : title, bodyFont, kCyan, 955);
        rect(49, 177, 985, 2, kLine);

        const int rowTop = 190, rowHeight = 55;
        for (int i = 0; i < maxRows; ++i) {
            const Row &row = rows[i];
            if (row.text.empty()) continue;
            const int y = rowTop + i * rowHeight;
            if (row.current) {
                rect(45, y, 989, rowHeight - 6, kCyan);
                rect(49, y + 4, 981, rowHeight - 14, kFocusBg);
            } else {
                rect(49, y + rowHeight - 4, 979, 1, kLine);
            }
            drawTabbedRow(row.text, 73, y + 10, 935,
                          !row.selectable ? kMuted : kWhite);
        }
        const int shown = std::min(maxRows, std::max(0, totalItems - firstItem));
        if (totalItems > maxRows) {
            text(52, 768, std::to_string(firstItem + 1) + "–" +
                 std::to_string(firstItem + shown) + " / " +
                 std::to_string(totalItems), small, kMuted, 365);
        }
        drawPreview(1100, 126, 443, 645);
    }

    void drawPreview(int x, int y, int w, int h)
    {
        text(x, y, mode == Body::Event ? "PROGRAMMINFORMATION" :
             mode == Body::Recording ? "AUFNAHMEDETAILS" :
             "AUSWAHL", small, kCyan, w);
        rect(x, y + 39, w, 2, kLine);
        if (!preview.title.empty())
            drawParagraph(preview.title, x, y + 55, w, y + 147,
                          bodyFont, kWhite, 37);
        if (!preview.subtitle.empty())
            text(x, y + 139, preview.subtitle, small, kMuted, w);
        if (!preview.meta.empty())
            drawParagraph(preview.meta, x, y + 179, w, y + 275,
                          small, kCyan, 27);
        if (!preview.picture.empty()) {
            rect(x, y + 284, w, 198, kCard);
            drawPicture(x + 8, y + 291, w - 16, 183);
        }
        const int descY = preview.picture.empty() ? y + 299 : y + 498;
        if (!preview.description.empty())
            drawParagraph(preview.description, x, descY, w, y + h - 11,
                          small, kWhite, 26);
        else if (!preview.hasDetail)
            text(x, y + 314, "Mit OK auswählen", small, kMuted, w);
    }

    void drawDetails()
    {
        rect(29, 103, 1538, 701, kSurface);
        text(64, 121, mode == Body::Event ? "PROGRAMMINFORMATIONEN" :
             mode == Body::Recording ? "AUFNAHMEDETAILS" : title,
             small, kCyan, 1400);
        if (!preview.title.empty())
            text(65, 170, preview.title, heading, kWhite, 1390);
        if (!preview.subtitle.empty())
            text(67, 235, preview.subtitle, bodyFont, kMuted, 1050);
        if (!preview.meta.empty())
            text(67, 293, preview.meta, small, kCyan, 1385);
        const bool hasPicture = !preview.picture.empty();
        const int descWidth = hasPicture ? 921 : 1406;
        if (hasPicture) {
            rect(1028, 356, 493, 317, kCard);
            drawPicture(1041, 369, 468, 290);
        }
        const auto lines = contentLines();
        int y = 365;
        for (int i = firstTextLine; i < int(lines.size()) && y < 747; ++i, y += 37)
            text(67, y, lines[i], fixedText ? fixedFont : bodyFont, kWhite, descWidth);
        if (firstTextLine > 0 || int(lines.size()) > 10 + firstTextLine)
            text(69, 758, "Pfeile: Text scrollen", small, kCyan, 700);
    }

public:
    SuiteMenu()
    {
        width = std::max(400, cOsd::OsdWidth());
        height = std::max(300, cOsd::OsdHeight());
        // Keep rows inside the menu panel on small video modes.
        maxRows = 10;
        rows.resize(maxRows);
        osd = cOsdProvider::NewOsd(cOsd::OsdLeft(), cOsd::OsdTop());
        if (!osd) return;
        const tArea area = {0, 0, width - 1, height - 1, 32};
        if (osd->SetAreas(&area, 1) != oeOk) {
            delete osd;
            osd = nullptr;
            return;
        }
        heading = cFont::CreateFont("Sans:Bold", std::max(20, Y(44)));
        bodyFont = cFont::CreateFont("Sans", std::max(15, Y(29)));
        small = cFont::CreateFont("Sans", std::max(12, Y(22)));
        fixedFont = cFont::CreateFont("Monospace", std::max(12, Y(22)));
    }

    ~SuiteMenu() override
    {
        delete osd;
        delete heading;
        delete bodyFont;
        delete small;
        delete fixedFont;
    }

    bool valid() const { return osd != nullptr && bodyFont != nullptr; }
    int MaxItems() override { return maxRows; }
    int GetTextAreaWidth() const override { return X(1400); }
    const cFont *GetTextAreaFont(bool FixedFont) const override
    { return FixedFont ? fixedFont : bodyFont; }

    void SetMenuCategory(eMenuCategory value) override
    {
        cSkinDisplayMenu::SetMenuCategory(value);
        category = value;
    }
    void SetMenuSortMode(eMenuSortMode value) override { sorting = value; }
    void SetTabs(int a, int b = 0, int c = 0, int d = 0, int e = 0) override
    {
        cSkinDisplayMenu::SetTabs(a, b, c, d, e);
        SetEditableWidth(std::max(X(160), X(930) - Tab(1)));
    }
    void Clear() override
    {
        rows.assign(maxRows, Row{});
        preview = Preview{};
        pendingPreview = Preview{};
        pendingPreviewIndex = -1;
        currentIndex = -1;
        mode = Body::Items;
        firstTextLine = 0;
        rawText.clear();
        fixedText = false;
        totalItems = firstItem = 0;
    }
    void SetTitle(const char *value) override { title = value ? value : ""; }
    void SetButtons(const char *red, const char *green = nullptr,
                    const char *yellow = nullptr, const char *blue = nullptr) override
    {
        buttons = {{red ? red : "", green ? green : "",
                    yellow ? yellow : "", blue ? blue : ""}};
    }
    void SetMessage(eMessageType type, const char *value) override
    {
        messageType = type;
        message = value ? value : "";
    }
    void SetScrollbar(int total, int offset) override
    {
        totalItems = total;
        firstItem = offset;
    }
    void SetItem(const char *value, int index, bool current, bool selectable) override
    {
        if (index < 0 || index >= maxRows) return;
        mode = Body::Items;
        rows[index] = Row{value ? value : "", selectable, current};
        if (current) {
            if (currentIndex != index || preview.title != rows[index].text) {
                preview = Preview{};
                preview.title = rows[index].text;
                std::replace(preview.title.begin(), preview.title.end(), '\t', ' ');
            }
            if (pendingPreviewIndex == index) {
                preview = std::move(pendingPreview);
                pendingPreviewIndex = -1;
            }
            currentIndex = index;
        } else if (currentIndex == index) {
            currentIndex = -1;
            preview = Preview{};
        }
    }
    bool SetItemEvent(const cEvent *event, int index, bool current, bool,
                      const cChannel *channel, bool,
                      eTimerMatch, bool timerActive) override
    {
        if (event && current) {
            Preview info;
            info.title = event->Title() ? event->Title() : "";
            info.subtitle = event->ShortText() ? event->ShortText() : "";
            info.description = event->Description() ? event->Description() : "";
            info.meta = timeText(event->StartTime()) + "–" + shortTime(event->EndTime());
            if (channel && channel->Name())
                info.meta += "  ·  " + std::string(channel->Name());
            if (timerActive) info.meta += "  ·  Timer aktiv";
            info.hasDetail = true;
            cPlugin *scraper = cPluginManager::GetPlugin("tvscraper");
            if (scraper) {
                cGetScraperVideo request(event, nullptr);
                if (scraper->Service("GetScraperVideo", &request) && request.m_scraperVideo)
                    info.ReadScraper(request.m_scraperVideo.get());
            }
            pendingPreview = std::move(info);
            pendingPreviewIndex = index;
        }
        // Let cMenuScheduleItem / cMenuWhatsOnItem draw the canonical
        // VDR row, including timer-match flags and all native formatting.
        return false;
    }

    bool SetItemRecording(const cRecording *recording, int index, bool current,
                          bool, int, int total, int) override
    {
        if (recording && current && total <= 0) {
            Preview info;
            const cRecordingInfo *details = recording->Info();
            info.title = details && details->Title() ? details->Title() :
                         (recording->Name() ? recording->Name() : "");
            info.subtitle = details && details->ShortText() ? details->ShortText() : "";
            info.description = details && details->Description() ? details->Description() : "";
            info.meta = timeText(recording->Start());
            info.hasDetail = true;
            cPlugin *scraper = cPluginManager::GetPlugin("tvscraper");
            if (scraper) {
                cGetScraperVideo request(nullptr, recording);
                if (scraper->Service("GetScraperVideo", &request) && request.m_scraperVideo)
                    info.ReadScraper(request.m_scraperVideo.get());
            }
            pendingPreview = std::move(info);
            pendingPreviewIndex = index;
        }
        // VDR keeps its folder grouping, recording/new markers, indentation,
        // dates and any other metadata it formats into the original row.
        return false;
    }

    bool SetItemTimer(const cTimer *timer, int index, bool current,
                      bool) override
    {
        if (timer && current) {
            Preview info;
            info.title = timer->File() ? timer->File() : "";
            const cChannel *channel = timer->Channel();
            info.subtitle = channel && channel->Name() ? channel->Name() : "";
            char range[32] = {};
            std::snprintf(range, sizeof(range), "%02d:%02d–%02d:%02d",
                          timer->Start() / 100, timer->Start() % 100,
                          timer->Stop() / 100, timer->Stop() % 100);
            info.meta = range;
            if (timer->Remote())
                info.meta += "  ·  " + std::string(timer->Remote());
            if (timer->WeekDays())
                info.meta += "  ·  Wiederholend";
            if (timer->Recording()) info.description = "Aufnahme läuft";
            else info.description = timer->HasFlags(tfActive) ?
                "Timer aktiviert" : "Timer deaktiviert";
            info.hasDetail = true;
            pendingPreview = std::move(info);
            pendingPreviewIndex = index;
        }
        // VDR's canonical timer row preserves active/recording flags,
        // repeat days, day/month, channel, remote owner and pattern timers.
        return false;
    }

    bool SetItemChannel(const cChannel *channel, int index, bool current,
                        bool, bool withProvider) override
    {
        if (channel && current) {
            Preview info;
            info.title = channel->Name() ? channel->Name() : "";
            if (withProvider && channel->Provider())
                info.subtitle = channel->Provider();
            info.meta = "Kanal " + std::to_string(channel->Number());
            info.hasDetail = true;
            pendingPreview = std::move(info);
            pendingPreviewIndex = index;
        }
        return false; // Keep VDR's native channel row and provider formatting.
    }

    void SetEvent(const cEvent *event) override
    {
        mode = Body::Event;
        preview = Preview{};
        fixedText = false;
        firstTextLine = 0;
        if (!event) { rawText.clear(); return; }
        preview.title = event->Title() ? event->Title() : "";
        preview.subtitle = event->ShortText() ? event->ShortText() : "";
        preview.description = event->Description() ? event->Description() : "";
        preview.meta = timeText(event->StartTime()) + "–" + shortTime(event->EndTime());
        preview.hasDetail = true;
        rawText = preview.description;
        cPlugin *scraper = cPluginManager::GetPlugin("tvscraper");
        if (scraper) {
            cGetScraperVideo request(event, nullptr);
            if (scraper->Service("GetScraperVideo", &request) && request.m_scraperVideo)
                preview.ReadScraper(request.m_scraperVideo.get());
        }
        if (rawText.empty()) rawText = preview.description;
    }
    void SetRecording(const cRecording *recording) override
    {
        mode = Body::Recording;
        preview = Preview{};
        fixedText = false;
        firstTextLine = 0;
        if (!recording) { rawText.clear(); return; }
        preview.title = recording->Info() && recording->Info()->Title() ?
            recording->Info()->Title() : (recording->Name() ? recording->Name() : "");
        preview.subtitle = recording->Info() && recording->Info()->ShortText() ?
            recording->Info()->ShortText() : "";
        preview.description = recording->Info() && recording->Info()->Description() ?
            recording->Info()->Description() : "";
        preview.meta = timeText(recording->Start());
        preview.hasDetail = true;
        cPlugin *scraper = cPluginManager::GetPlugin("tvscraper");
        if (scraper) {
            cGetScraperVideo request(nullptr, recording);
            if (scraper->Service("GetScraperVideo", &request) && request.m_scraperVideo)
                preview.ReadScraper(request.m_scraperVideo.get());
        }
        rawText = preview.description;
    }
    void SetText(const char *value, bool fixedFont) override
    {
        mode = Body::Text;
        preview = Preview{};
        rawText = value ? value : "";
        fixedText = fixedFont;
        firstTextLine = 0;
    }
    void Scroll(bool up, bool page) override
    {
        if (mode == Body::Items) return;
        const int maxLines = int(contentLines().size());
        firstTextLine = std::clamp(firstTextLine + (up ? -1 : 1) * (page ? 9 : 1),
                                   0, std::max(0, maxLines - 10));
    }
    void Flush() override
    {
        if (!osd) return;
        drawChrome();
        if (mode == Body::Items) drawItems();
        else drawDetails();
        osd->Flush();
    }
};
} // namespace

cSkinDisplayMenu *CreateSuiteMenu()
{
    auto *menu = new SuiteMenu();
    if (menu->valid()) return menu;
    delete menu;
    return nullptr;
}
} // namespace suiteosd
