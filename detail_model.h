#ifndef SUITEOSD_DETAIL_MODEL_H
#define SUITEOSD_DETAIL_MODEL_H

#include <algorithm>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace suiteosd {

struct PersonCredit {
    std::string name, role, character, portraitPath;
};

// Own values only. No borrowed VDR or TVScraper pointers survive a service call.
struct DetailMetadata {
    std::string title, episode, overview, release, fanartPath, posterPath;
    std::vector<std::string> genres;
    std::vector<PersonCredit> people;
    int runtime = 0, season = 0, episodeNumber = 0;
    float rating = 0;
};

inline void AddCredit(DetailMetadata &data, PersonCredit credit)
{
    if (credit.name.empty()) return;
    for (auto &existing : data.people) {
        if (std::tie(existing.name, existing.role, existing.character) !=
            std::tie(credit.name, credit.role, credit.character)) continue;
        if (existing.portraitPath.empty()) existing.portraitPath = credit.portraitPath;
        return;
    }
    data.people.push_back(std::move(credit));
}

inline std::string CreditText(const DetailMetadata &data)
{
    std::string result;
    for (const auto &credit : data.people) {
        if (!result.empty()) result += "\n";
        result += credit.name + " · " + credit.role;
        if (!credit.character.empty()) result += " · " + credit.character;
    }
    return result;
}

// Bounded, horizontally scrollable cast rail; no fixed total cast limit.
struct CreditNavigation {
    bool active = false;
    int selected = 0;
    int first = 0;
    static constexpr int visible = 6;
    void Open() { active = true; selected = first = 0; }
    void Close() { active = false; selected = first = 0; }
    void Move(int delta, int count)
    {
        selected = std::clamp(selected + delta, 0, std::max(0, count - 1));
        if (selected < first) first = selected;
        if (selected >= first + visible) first = selected - visible + 1;
        first = std::clamp(first, 0, std::max(0, count - visible));
    }
};
} // namespace suiteosd
#endif
