#ifndef SUITEOSD_DETAIL_MODEL_H
#define SUITEOSD_DETAIL_MODEL_H

#include <algorithm>
#include <set>
#include <string>
#include <tuple>
#include <utility>
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

inline std::string RecordingDisplayTitle(const DetailMetadata &data,const std::string &fallback)
{
    return data.title.empty()?fallback:data.title;
}
inline std::string RecordingEpisodeLabel(const DetailMetadata &data)
{
    std::string label;
    if(data.season>0&&data.episodeNumber>0)
        label="S"+std::to_string(data.season)+" E"+std::to_string(data.episodeNumber);
    if(!data.episode.empty()&&data.episode!=data.title)
        label+=(label.empty()?"":" · ")+data.episode;
    return label;
}

inline std::string HeroFacts(const DetailMetadata &data)
{
    std::string result;
    auto add=[&](const std::string &value) {
        if (!value.empty()) result += (result.empty() ? "" : "  ·  ") + value;
    };
    if (data.release.size() >= 4 &&
        std::all_of(data.release.begin(),data.release.begin()+4,[](char c){return c>='0'&&c<='9';}))
        add(data.release.substr(0,4));
    else add(data.release);
    if (data.runtime > 0) add(std::to_string(data.runtime)+" Min.");
    for (const auto &genre:data.genres) add(genre);
    if (data.season>0) {
        std::string episode="Staffel "+std::to_string(data.season);
        if (data.episodeNumber>0) episode+=", Folge "+std::to_string(data.episodeNumber);
        add(episode);
    }
    return result;
}

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
