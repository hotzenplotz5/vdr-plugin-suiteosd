#ifndef SUITEOSD_LOGO_LOCATOR_H
#define SUITEOSD_LOGO_LOCATOR_H

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace suiteosd {

inline std::string NormalizeLogoName(const std::string &input)
{
    std::string out;
    bool space = false;
    for (unsigned char c : input) {
        if (c == ';') break;
        if (c == '\\') continue;
        if (std::isspace(c)) {
            if (!out.empty()) space = true;
            continue;
        }
        if (space) out.push_back(' ');
        space = false;
        // The VDR logo file convention uses '~' for '/'.
        out.push_back(c == '/' ? '~' : char(std::tolower(c)));
    }
    return out;
}

inline void PushUnique(std::vector<std::string> &names, const std::string &candidate)
{
    if (!candidate.empty() &&
        candidate.find("..") == std::string::npos &&
        candidate.find('/') == std::string::npos &&
        candidate.find('\\') == std::string::npos &&
        std::find(names.begin(), names.end(), candidate) == names.end())
        names.push_back(candidate);
}

inline void AppendLogoVariants(std::vector<std::string> &names, const std::string &title)
{
    const std::string name = NormalizeLogoName(title);
    if (name.empty()) return;
    const size_t n = name.size();
    std::string base = name;
    for (const char *suffix : {" uhd", " hd", " sd"}) {
        const size_t len = std::string(suffix).size();
        if (n > len && name.compare(n - len, len, suffix) == 0) {
            base = name.substr(0, n - len);
            break;
        }
    }
    for (const std::string &variant : {name, base}) {
        PushUnique(names, variant);
        for (char separator : {'_', '-', '\0'}) {
            std::string modified;
            for (char c : variant)
                if (c == ' ') {
                    if (separator != '\0') modified.push_back(separator);
                } else modified.push_back(c);
            PushUnique(names, modified);
        }
    }
    // Same common naming aliases as the VDR-Suite Webfrontend logos module.
    if (base == "das erste") PushUnique(names, "ard");
    if (base == "zdfneo") {
        PushUnique(names, "zdf_neo");
        PushUnique(names, "zdf neo");
    }
    if (base == "zdfinfo") {
        PushUnique(names, "zdf.info");
        PushUnique(names, "zdf_info");
    }
    if (base == "one") PushUnique(names, "one hd");
    if (base == "ntv") PushUnique(names, "n-tv");
    if (base == "pro sieben") {
        PushUnique(names, "prosieben");
        PushUnique(names, "pro7");
    }
    if (base == "kabel eins") PushUnique(names, "kabeleins");
    if (base == "rtlup") PushUnique(names, "rtl up");
}

inline std::vector<std::string> ChannelLogoRoots()
{
    std::vector<std::string> roots;
    for (const char *name : {"VDR_SUITE_CHANNEL_LOGO_ROOT", "VDR_SUITE_CHANNEL_LOGO_CACHE"}) {
        const char *value = std::getenv(name);
        if (value && *value) roots.emplace_back(value);
    }
    for (const char *path : {
            "/var/cache/vdr-suite/channel-logos",
            "/usr/share/vdr-suite/channel-logos",
            "/usr/share/vdr/plugins/skindesigner/skins/estuary4vdr/logos",
            "/usr/share/vdr/plugins/skindesigner/logos",
            "/usr/share/vdr/channellogos"})
        roots.emplace_back(path);
    return roots;
}

inline std::string FindChannelLogoInRoots(const std::vector<std::string> &roots,
                                           const std::string &name,
                                           const std::string &shortName = "")
{
    std::vector<std::string> names;
    AppendLogoVariants(names, name);
    AppendLogoVariants(names, shortName);
    // All supported local image formats can be decoded by the OSD.
    for (const std::string &root : roots)
        for (const std::string &variant : names)
            for (const char *ext : {".png", ".jpg", ".jpeg", ".svg"}) {
                const std::string path = root + "/" + variant + ext;
                std::ifstream f(path, std::ios::binary);
                if (f.good()) return path;
            }
    return {};
}

inline std::string FindChannelLogo(const std::string &name,
                                   const std::string &shortName = "")
{
    return FindChannelLogoInRoots(ChannelLogoRoots(), name, shortName);
}

} // namespace suiteosd
#endif
