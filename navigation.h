#ifndef VDR_SUITEOSD_NAVIGATION_H
#define VDR_SUITEOSD_NAVIGATION_H

#include <algorithm>
#include <array>
#include <cstddef>
#include "home_layout.h"

namespace suiteosd {

// Headless model: the VDR adapter translates remote keys into Direction.
enum class Direction { Up, Down, Left, Right, Select, Back };
enum class Activation { None, Close, Hero, ChannelNow, ChannelNext, Recording, Module, HomeCard };

struct Navigation {
    static constexpr int kHomeVisibleCards = HomeLayout::columns;
    static constexpr int kSections = 10; // hero, now, next, new, continue, genres, folders, recommendation, movies, recent
    static constexpr int kContinue = 4, kNew = 3, kGenres = 5, kFolders = 6, kRecommendation=7, kMovies = 8, kRecent = 9;
    static constexpr int kSidebarItems = 13;
    static constexpr int kClassicMenuItem = 12;
    static constexpr int kSearchItem = 11;
    bool sideFocused = false;
    int sideIndex = 0;
    int section = 0;
    std::array<int, kSections> card = {{0, 0, 0, 0}};
    std::array<int, kSections> firstCard = {{0, 0, 0, 0}};
    int firstVisibleSection = 1; // hero is fixed at the top initially

    void KeepVisible(int visibleCards, int sectionCount)
    {
        const int count = std::max(0, sectionCount);
        if (count == 0) {
            card[section] = 0;
            firstCard[section] = 0;
            return;
        }
        card[section] = std::clamp(card[section], 0, count - 1);
        const int columns = std::max(1, visibleCards);
        if (card[section] < firstCard[section])
            firstCard[section] = card[section];
        if (card[section] >= firstCard[section] + columns)
            firstCard[section] = card[section] - columns + 1;
    }

    Activation Move(Direction key, int sectionCount, int visibleCards)
    {
        if (key == Direction::Back)
            return sideFocused ? (sideFocused = false, Activation::None) : Activation::Close;
        if (sideFocused) {
            if (key == Direction::Up)
                sideIndex = std::max(0, sideIndex - 1);
            else if (key == Direction::Down)
                sideIndex = std::min(kSidebarItems - 1, sideIndex + 1);
            else if (key == Direction::Right)
                sideFocused = false;
            else if (key == Direction::Select) {
                sideFocused = false;
                switch (sideIndex) {
                case 0: section = 0; break; // Home stays in the approved Suite dashboard
                default: return Activation::Module; // VDR menus or installed plugins
                }
                KeepVisible(visibleCards, sectionCount);
                firstVisibleSection = HomeLayout::firstRow(section,kSections);
            }
            return Activation::None;
        }
        if (key == Direction::Left) {
            if (section == 0 || card[section] == 0)
                sideFocused = true;
            else
                --card[section];
        } else if (key == Direction::Right) {
            card[section] = std::min(std::max(0, sectionCount - 1), card[section] + 1);
        } else if (key == Direction::Up) {
            section = std::max(0, section - 1);
        } else if (key == Direction::Down) {
            section = std::min(kSections - 1, section + 1);
        } else if (key == Direction::Select) {
            switch (section) {
            case 0: return Activation::Hero;
            case 1: return Activation::ChannelNow;
            case 2: return Activation::ChannelNext;
            case kNew: return Activation::Recording;
            default: return Activation::HomeCard;
            }
        }
        KeepVisible(visibleCards, sectionCount);
        // The first three rails stay visible together; subsequent rails scroll
        // vertically. Section zero focuses the separate live panel.
        firstVisibleSection = HomeLayout::firstRow(section,kSections);
        return Activation::None;
    }
};

} // namespace suiteosd
#endif
