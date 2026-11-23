#ifndef VDR_SUITEOSD_NAVIGATION_H
#define VDR_SUITEOSD_NAVIGATION_H

#include <algorithm>
#include <array>
#include <cstddef>

namespace suiteosd {

// Headless model: the VDR adapter translates remote keys into Direction.
enum class Direction { Up, Down, Left, Right, Select, Back };
enum class Activation { None, Close, Hero, ChannelNow, ChannelNext, Recording, Module };

struct Navigation {
    static constexpr int kSections = 4; // hero, now, next, recordings
    static constexpr int kSidebarItems = 11;
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
                case 0: section = 0; break; // Home
                case 1: section = 0; break; // Live-TV
                case 2: section = 1; break; // EPG / now
                case 3: section = 3; break; // recordings
                default: return Activation::Module; // clearly pending module integration
                }
                KeepVisible(visibleCards, sectionCount);
                firstVisibleSection = section >= 3 ? 2 : 1;
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
            case 3: return Activation::Recording;
            }
        }
        KeepVisible(visibleCards, sectionCount);
        // The hero occupies a full section and stays in the viewport until
        // moving to lower rails. No visible scroll bar is rendered.
        firstVisibleSection = section >= 3 ? 2 : 1;
        return Activation::None;
    }
};

} // namespace suiteosd
#endif
