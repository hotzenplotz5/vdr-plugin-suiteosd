#ifndef SUITEOSD_SCRAPER_DETAIL_H
#define SUITEOSD_SCRAPER_DETAIL_H
#include "detail_model.h"
#include "vendor/tvscraper/services.h"
#include "vendor/vdr-suite/CanonicalGenreRegistry.h"
#include <utility>

namespace suiteosd {
inline std::string CreditRole(eCharacterType type)
{
    switch (type) {
    case eCharacterType::director: return "Regie";
    case eCharacterType::actor: return "Schauspiel";
    case eCharacterType::guestStar: return "Gastrolle";
    case eCharacterType::writer: return "Autor";
    case eCharacterType::screenplay: return "Drehbuch";
    case eCharacterType::producer: return "Produktion";
    case eCharacterType::executiveProducer: return "Ausführende Produktion";
    case eCharacterType::creator: return "Schöpfer";
    case eCharacterType::showrunner: return "Showrunner";
    case eCharacterType::host: return "Moderation";
    case eCharacterType::originalMusicComposer: return "Musik";
    case eCharacterType::musicalGuest: return "Musikgast";
    case eCharacterType::crew: return "Team";
    default: return "Mitwirkende";
    }
}

inline DetailMetadata ReadScraperDetail(cScraperVideo *video, bool includePeople = true)
{
    DetailMetadata result;
    if (!video) return result;
    video->getOverview(&result.title, &result.episode, &result.release,
                       &result.runtime, nullptr, nullptr, nullptr);
    std::vector<std::string> genres;
    video->getMovieOrTv(nullptr, nullptr, nullptr, &result.overview,
                        &genres, nullptr, nullptr, nullptr,
                        nullptr, nullptr, &result.rating, nullptr, nullptr,
                        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    static const CanonicalGenreRegistry registry;
    for (const auto &genre : genres) {
        auto normalized = registry.classify(genre);
        if (normalized.id != "unclassified" &&
            std::find(result.genres.begin(), result.genres.end(), normalized.labelDe) == result.genres.end())
            result.genres.push_back(normalized.labelDe);
    }
    result.season = video->getSeasonNumber();
    result.episodeNumber = video->getEpisodeNumber();
    if (video->getVideoType() == tSeries) {
        std::string episodeOverview;
        int episodeRuntime = 0;
        if (video->getEpisode(nullptr, &episodeOverview, nullptr, nullptr,
                              &episodeRuntime, nullptr, nullptr, nullptr)) {
            if (!episodeOverview.empty()) result.overview = std::move(episodeOverview);
            if (episodeRuntime > 0) result.runtime = episodeRuntime;
        }
    }
    const cImageLevels levels(eImageLevel::episodeMovie, eImageLevel::seasonMovie,
                             eImageLevel::tvShowCollection);
    result.fanartPath = video->getImage(levels, cOrientations(eOrientation::landscape,
                                               eOrientation::banner), true).path;
    result.posterPath = video->getImage(levels, cOrientations(eOrientation::portrait), true).path;
    if (includePeople) {
        for (auto &character : video->getCharacters(true)) {
            if (!character) continue;
            AddCredit(result, {character->getPersonName(), CreditRole(character->getType()),
                              character->getCharacterName(), character->getImage().path});
        }
    }
    return result;
}
} // namespace suiteosd
#endif
