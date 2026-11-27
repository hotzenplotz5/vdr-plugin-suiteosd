/*
 * SuiteOSD photo navigation. Image Next remains the source of truth.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Pure state: no filesystem, decoder, VDR API or duplicated image loader.
 */
#ifndef VDR_SUITEOSD_PHOTO_GALLERY_H
#define VDR_SUITEOSD_PHOTO_GALLERY_H

#include "image-next-service-v1.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace suiteosd {

class PhotoGallery {
public:
    static constexpr int kColumns = 4;
    static constexpr int kRows = 2;
    static constexpr int kVisible = kColumns * kRows;

    // Fill a short final row without inventing or duplicating photos.
    // The first tile spans two columns when three remain; two remaining
    // tiles each span two. A lone tile is centred across two columns.
    static int GridSpan(int cell, int visible) {
        const int rowStart = (cell / kColumns) * kColumns;
        const int remaining = std::min(kColumns, visible - rowStart);
        const int col = cell % kColumns;
        return remaining <= 2 || (remaining == 3 && col == 0) ? 2 : 1;
    }
    static int GridStart(int cell, int visible) {
        const int rowStart = (cell / kColumns) * kColumns;
        const int remaining = std::min(kColumns, visible - rowStart);
        const int col = cell % kColumns;
        if (remaining == 1) return 1;
        if (remaining == 2) return col * 2;
        if (remaining == 3) return col ? col + 1 : 0;
        return col;
    }
    static int GridCentre(int cell, int visible) {
        return 2 * GridStart(cell, visible) + GridSpan(cell, visible);
    }

    int source = -1; // select a configured image_next source first
    std::string directory;
    std::vector<imagenext::Source> sources;
    std::vector<imagenext::Entry> entries;
    int selected = 0;
    int firstVisible = 0;

    // Choice is persisted in suiteosd.PhotoView (0=grid, 1=carousel).
    // The carousel uses the same directory and image_next thumbnail data.
    enum class View { Grid = 0, Carousel = 1 };
    View view = View::Carousel;

    void SetView(View next)
    {
        view = next;
        selected = std::max(0, std::min(selected, Count() - 1));
        firstVisible = (selected / kVisible) * kVisible;
    }

    void MoveCarousel(int delta)
    {
        if (!Count()) {
            selected = firstVisible = 0;
            return;
        }
        selected = std::max(0, std::min(Count() - 1, selected + delta));
        firstVisible = (selected / kVisible) * kVisible;
    }

    bool InSources() const { return source < 0; }
    int Count() const { return InSources() ? int(sources.size()) : int(entries.size()); }

    const imagenext::Source *SelectedSource() const
    {
        return InSources() && selected >= 0 && selected < int(sources.size()) ?
               &sources[selected] : nullptr;
    }

    const imagenext::Entry *SelectedEntry() const
    {
        return !InSources() && selected >= 0 && selected < int(entries.size()) ?
               &entries[selected] : nullptr;
    }

    void SetSources(std::vector<imagenext::Source> next)
    {
        source = -1;
        directory.clear();
        sources = std::move(next);
        entries.clear();
        selected = firstVisible = 0;
    }

    void SetEntries(std::vector<imagenext::Entry> next)
    {
        entries = std::move(next);
        selected = firstVisible = 0;
    }

    void OpenSource(int index)
    {
        source = index;
        directory.clear();
        entries.clear();
        selected = firstVisible = 0;
    }

    void OpenDirectory(const std::string &relative)
    {
        directory = relative;
        entries.clear();
        selected = firstVisible = 0;
    }

    // Returns false only when already at top-level configured source list.
    bool Back()
    {
        if (InSources())
            return false;
        if (directory.empty()) {
            source = -1;
            entries.clear();
            selected = firstVisible = 0;
            return true;
        }
        const auto slash = directory.rfind('/');
        directory = slash == std::string::npos ? "" : directory.substr(0, slash);
        entries.clear();
        selected = firstVisible = 0;
        return true;
    }

    void Move(int dx, int dy)
    {
        const int count = Count();
        if (!count) {
            selected = firstVisible = 0;
            return;
        }
        const int col = selected % kColumns;
        if (dx < 0 && col > 0)
            --selected;
        else if (dx > 0 && col < kColumns - 1 && selected + 1 < count)
            ++selected;
        else if (dy != 0) {
            const int row = selected / kColumns + (dy < 0 ? -1 : 1);
            const int first = row * kColumns;
            if (first >= 0 && first < count) {
                const int currentPage = (selected / kVisible) * kVisible;
                const int centre = GridCentre(selected - currentPage,
                    std::min(kVisible, count - currentPage));
                const int targetPage = (first / kVisible) * kVisible;
                const int visible = std::min(kVisible, count - targetPage);
                int best = first, distance = 100;
                for (int i = first; i < std::min(first + kColumns, count); ++i) {
                    const int d = std::abs(GridCentre(i - targetPage, visible) - centre);
                    if (d < distance) { best = i; distance = d; }
                }
                selected = best;
            }
        }
        firstVisible = (selected / kVisible) * kVisible;
    }

    void Page(int delta)
    {
        const int count = Count();
        if (!count) return;
        selected = std::max(0, std::min(count - 1, selected + delta * kVisible));
        firstVisible = (selected / kVisible) * kVisible;
    }
};

} // namespace suiteosd

#endif // VDR_SUITEOSD_PHOTO_GALLERY_H
