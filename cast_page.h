#ifndef SUITEOSD_CAST_PAGE_H
#define SUITEOSD_CAST_PAGE_H
#include "detail_model.h"
#include "artwork_decode.h"
#include <vdr/osd.h>
#include <vdr/font.h>
#include <map>

namespace suiteosd {
// Used only by interactive Suite cOsdObjects; the passive cSkin renderer
// cannot own remote keys. All image decoding happens after scraper locks.
inline void DrawCastPage(cOsd *osd, int width, int height, const cFont *titleFont,
                         const cFont *bodyFont, const cFont *smallFont,
                         const DetailMetadata &detail, const CreditNavigation &nav,
                         std::map<std::string, Artwork> &cache, bool canSearch = false)
{
    if (!osd) return;
    auto X = [width](int x) { return x * width / 1600; };
    auto Y = [height](int y) { return y * height / 900; };
    auto rect = [&](int x, int y, int w, int h, tColor c) {
        osd->DrawRectangle(X(x), Y(y), X(x+w)-1, Y(y+h)-1, c);
    };
    auto text = [&](int x, int y, const std::string &s, const cFont *font,
                    tColor c, int w) {
        if (font && !s.empty())
            osd->DrawText(X(x), Y(y), s.c_str(), c, clrTransparent, font, X(w));
    };
    constexpr tColor bg = 0xFF050F20, surface = 0xFF081A30,
        card = 0xFF102940, accent = 0xFF08D2F6, white = 0xFFF6FAFF, muted = 0xFFA8BBD2;
    rect(0, 0, 1600, 900, bg);
    rect(0, 0, 1600, 78, surface);
    text(36, 24, "VDR Suite", bodyFont, white, 220);
    text(284, 24, "Schauspieler & Mitwirkende", bodyFont, white, 1100);
    text(48, 110, detail.title, titleFont, white, 1490);
    text(48, 184, std::to_string(detail.people.size()) + " Personen", bodyFont, muted, 1400);
    for (int col = 0; col < CreditNavigation::visible &&
         nav.first + col < int(detail.people.size()); ++col) {
        const int index = nav.first + col;
        const auto &credit = detail.people[index];
        const int x = 48 + col * 252;
        if (index == nav.selected) rect(x-3, 247, 242, 456, accent);
        rect(x, 250, 236, 450, card);
        if (!credit.portraitPath.empty()) {
            auto it = cache.find(credit.portraitPath);
            if (it == cache.end()) {
                if (cache.size() >= 24) cache.clear();
                it = cache.emplace(credit.portraitPath,
                      LoadArtwork(credit.portraitPath, X(212), Y(306))).first;
            }
            const Artwork &image = it->second;
            if (image.valid()) {
                cImage picture(cSize(image.width, image.height), image.argb.data());
                osd->DrawImage(cPoint(X(x+12) + (X(212)-image.width)/2,
                                     Y(262) + (Y(306)-image.height)/2), picture);
            }
        }
        text(x+12, 584, credit.name, bodyFont, white, 212);
        text(x+12, 625, credit.role, smallFont, accent, 212);
        text(x+12, 657, credit.character, smallFont, muted, 212);
    }
    if (detail.people.empty())
        text(48, 320, "Keine Mitwirkenden verfügbar", bodyFont, muted, 1400);
    if (!detail.people.empty()) {
        const auto &selected = detail.people[std::clamp(nav.selected, 0, int(detail.people.size())-1)];
        text(48, 733, selected.name + " · " + selected.role +
             (selected.character.empty() ? "" : " · " + selected.character),
             bodyFont, white, 1490);
    }
    rect(18, 817, 1564, 72, surface);
    text(52, 839, canSearch ? "Links/Rechts: Personen · OK: Suche" :
         "Links/Rechts: Personen", bodyFont, white, 780);
    text(855, 839, "Zurück / Hoch: Aufnahmedetails", bodyFont, white, 700);
}
} // namespace suiteosd
#endif
