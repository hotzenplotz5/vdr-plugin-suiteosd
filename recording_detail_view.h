#ifndef SUITEOSD_RECORDING_DETAIL_VIEW_H
#define SUITEOSD_RECORDING_DETAIL_VIEW_H
#include "detail_model.h"
#include "artwork_decode.h"
#include <vdr/osd.h>
#include <vdr/font.h>
#include <map>
#include <sstream>

namespace suiteosd {
constexpr int kHeroTextWidth = 850;
constexpr int kHeroTextRows = 6;
constexpr uint32_t kHeroBackground = 0xFF090B0Fu;

// Same bounded decoder, larger display fit and opaque software fade so the
// background artwork is never replaced by the underlying live picture.
inline Artwork LoadDetailArtwork(const std::string &path, int w, int h, bool backdrop = false)
{
    w = std::clamp(w, 1, 1024); h = std::clamp(h, 1, 1024);
    auto image = LoadArtwork(path, w, h);
    if (image.valid()) {
        ExpandArtwork(image, w, h);
        OpaqueArtwork(image, kHeroBackground);
        if (backdrop) FadeArtwork(image, kHeroBackground);
    }
    return image;
}

inline std::vector<std::string> HeroLines(const std::string &source,
                                          const cFont *font, int pixels)
{
    std::vector<std::string> lines;
    if (!font) return lines;
    std::istringstream paragraphs(source.empty() ? "Keine Beschreibung vorhanden." : source);
    std::string paragraph;
    while (std::getline(paragraphs, paragraph)) {
        std::istringstream words(paragraph);
        std::string line, word;
        while (words >> word) {
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && font->Width(candidate.c_str()) > pixels) {
                lines.push_back(line);
                line = word;
            } else line = candidate;
        }
        lines.push_back(line);
    }
    return lines;
}

// Full-page hero, shared by the Suite browser, replay Info and passive VDR
// recording Info. Key ownership stays with the caller; the Skin draws only.
inline void DrawRecordingHero(cOsd *osd, int width, int height,
    const cFont *titleFont, const cFont *bodyFont, const cFont *smallFont,
    const DetailMetadata &detail, const std::string &fallbackTitle,
    const std::string &recorded, const std::string &description,
    const Artwork &fanart, const Artwork &poster, int scroll,
    const CreditNavigation *nav, std::map<std::string, Artwork> &portraits)
{
    if (!osd) return;
    auto X = [width](int x) { return x * width / 1600; };
    auto Y = [height](int y) { return y * height / 900; };
    auto rect = [&](int x,int y,int w,int h,tColor color) {
        osd->DrawRectangle(X(x),Y(y),X(x+w)-1,Y(y+h)-1,color);
    };
    auto text = [&](int x,int y,const std::string &s,const cFont *font,
                    tColor color,int w) {
        if (font && !s.empty())
            osd->DrawText(X(x),Y(y),s.c_str(),color,clrTransparent,font,X(w));
    };
    auto picture = [&](const Artwork &art,int x,int y,int w,int h) {
        if (!art.valid()) return;
        cImage img(cSize(art.width,art.height),art.argb.data());
        osd->DrawImage(cPoint(X(x)+(X(w)-art.width)/2,Y(y)+(Y(h)-art.height)/2),img);
    };
    constexpr tColor dark = kHeroBackground, card = 0xFF15181E,
        white = 0xFFF8FAFC, muted = 0xFFB8BEC8, accent = 0xFFB9D8FF,
        gold = 0xFFFDE68A;
    rect(0,84,1600,736,dark);
    if (fanart.valid()) {
        cImage img(cSize(fanart.width,fanart.height),fanart.argb.data());
        osd->DrawImage(cPoint(X(1590)-fanart.width,Y(86)),img);
    }
    if (poster.valid()) {
        rect(44,116,274,414,card);
        picture(poster,50,122,262,402);
    }
    const int tx = poster.valid() ? 350 : 54;
    text(tx,112,"AUFNAHME",smallFont,accent,990);
    text(tx,149,detail.title.empty() ? fallbackTitle : detail.title,titleFont,white,1190);
    text(tx,216,detail.episode,bodyFont,muted,1050);
    text(tx,254,HeroFacts(detail),bodyFont,white,1120);
    if (detail.rating > 0) {
        std::string rating = std::to_string(int(detail.rating * 10 + 0.5f));
        const int tenths = int(detail.rating * 10 + 0.5f);
        rating = "★ " + std::to_string(tenths / 10) + "," + std::to_string(tenths % 10) + "/10";
        text(tx,290,rating,bodyFont,gold,220);
    }
    text(tx+(detail.rating>0?228:0),296,recorded,smallFont,muted,850);
    const auto lines = HeroLines(description,bodyFont,X(kHeroTextWidth));
    const int first = std::clamp(scroll,0,std::max(0,int(lines.size())-kHeroTextRows));
    for (int i=0;i<kHeroTextRows && first+i<int(lines.size());++i)
        text(tx,338+i*34,lines[first+i],bodyFont,white,kHeroTextWidth+8);
    if (int(lines.size())>kHeroTextRows)
        text(tx,548,"Text " + std::to_string(first+1) + "–" +
            std::to_string(std::min(first+kHeroTextRows,int(lines.size()))) +
            " / " + std::to_string(lines.size()),smallFont,muted,800);

    rect(0,581,1600,239,dark);
    text(48,590,nav ? (nav->active ? "Schauspieler & Mitwirkende  ·  OK: Person suchen" : "Schauspieler & Mitwirkende  ·  Runter / Blau: auswählen") : "Schauspieler & Mitwirkende",bodyFont,white,1250);
    if (detail.people.empty()) {
        text(48,650,"Keine Mitwirkenden verfügbar",bodyFont,muted,1450);
        return;
    }
    const int start = nav ? nav->first : 0;
    const int count = int(detail.people.size());
    text(1340,596,std::to_string(std::min(start+1,count))+"–"+
        std::to_string(std::min(start+CreditNavigation::visible,count))+" / "+
        std::to_string(count),smallFont,muted,210);
    for(int col=0;col<CreditNavigation::visible && start+col<count;++col) {
        const int index = start+col, x = 48+col*252;
        const auto &credit = detail.people[index];
        const bool focused = nav && nav->active && nav->selected==index;
        if (focused) rect(x-3,632,242,168,accent);
        rect(x,635,236,162,card);
        if (!credit.portraitPath.empty()) {
            auto it = portraits.find(credit.portraitPath);
            if(it==portraits.end()) {
                if(portraits.size()>=24) portraits.clear();
                it=portraits.emplace(credit.portraitPath,
                    LoadDetailArtwork(credit.portraitPath,X(88),Y(126))).first;
            }
            picture(it->second,x+8,648,88,126);
        }
        const auto names = HeroLines(credit.name,smallFont,X(128));
        for(int line=0;line<2 && line<int(names.size());++line)
            text(x+103,647+line*25,names[line],smallFont,white,126);
        text(x+103,705,credit.role,smallFont,accent,126);
        const auto roles = credit.character.empty() ? std::vector<std::string>{} :
            HeroLines(credit.character,smallFont,X(128));
        for(int line=0;line<2 && line<int(roles.size());++line)
            text(x+103,732+line*25,roles[line],smallFont,muted,126);
    }
}
} // namespace suiteosd
#endif
