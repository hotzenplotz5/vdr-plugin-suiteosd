#ifndef SUITEOSD_RECORDING_BROWSER_H
#define SUITEOSD_RECORDING_BROWSER_H

#include <algorithm>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace suiteosd {

struct BrowserRecording {
    std::string fileName;
    std::string name;         // cRecording::Name(), with VDR's '~' folder delimiter
    std::string title;        // cRecording::BaseName()
    time_t start = 0;
    std::vector<std::string> genres; // TVScraper; empty means "Ohne Genre"
};

struct BrowserEntry {
    enum class Kind { Folder, Genre, Recording };
    Kind kind = Kind::Recording;
    std::string title;
    std::string fileName;
    std::string path; // folder or genre key, never a filesystem path
    int count = 0;
};

struct RecordingBrowser {
    enum class View { Folders, Genres, All };
    enum class Sort { Newest, Name };
    static constexpr int kColumns = 4;
    static constexpr int kVisibleRows = 3;
    static constexpr int kVisibleItems = kColumns * kVisibleRows;

    View view = View::Folders;
    Sort sort = Sort::Newest;
    bool tabsFocused = true;
    std::string folder;
    std::string genre;
    int selected = 0;
    int firstVisible = 0;
    std::vector<BrowserRecording> source;
    std::vector<BrowserEntry> entries;

    static std::string ParentFolder(const std::string &value)
    {
        const size_t end = value.rfind('~');
        return end == std::string::npos ? "" : value.substr(0, end);
    }

    static std::string DisplayName(const BrowserRecording &recording)
    {
        if (!recording.title.empty()) return recording.title;
        const size_t delimiter = recording.name.rfind('~');
        return delimiter == std::string::npos ? recording.name : recording.name.substr(delimiter + 1);
    }

    void Refresh()
    {
        entries.clear();
        if (view == View::Folders) {
            std::map<std::string, int> children;
            for (const BrowserRecording &rec : source) {
                const std::string ancestor = ParentFolder(rec.name);
                const std::string prefix = folder.empty() ? "" : folder + "~";
                if (!folder.empty() && ancestor != folder &&
                    ancestor.compare(0, prefix.size(), prefix) != 0)
                    continue;
                const std::string tail = folder.empty() ? ancestor :
                    ancestor == folder ? "" : ancestor.substr(prefix.size());
                if (!tail.empty()) {
                    const std::string name = tail.substr(0, tail.find('~'));
                    children[name]++;
                } else {
                    BrowserEntry entry;
                    entry.kind = BrowserEntry::Kind::Recording;
                    entry.title = DisplayName(rec);
                    entry.fileName = rec.fileName;
                    entries.push_back(std::move(entry));
                }
            }
            for (const auto &child : children) {
                BrowserEntry entry;
                entry.kind = BrowserEntry::Kind::Folder;
                entry.title = child.first;
                entry.path = folder.empty() ? child.first : folder + "~" + child.first;
                entry.count = child.second;
                entries.push_back(std::move(entry));
            }
        } else if (view == View::Genres && genre.empty()) {
            std::map<std::string, int> counts;
            for (const BrowserRecording &rec : source) {
                if (rec.genres.empty()) ++counts["Ohne Genre"];
                else {
                    std::set<std::string> unique;
                    for (const std::string &name : rec.genres)
                        if (!name.empty()) unique.insert(name);
                    if (unique.empty()) ++counts["Ohne Genre"];
                    else for (const std::string &name : unique) ++counts[name];
                }
            }
            for (const auto &item : counts) {
                BrowserEntry entry;
                entry.kind = BrowserEntry::Kind::Genre;
                entry.title = entry.path = item.first;
                entry.count = item.second;
                entries.push_back(std::move(entry));
            }
        } else {
            for (const BrowserRecording &rec : source) {
                if (view == View::Genres) {
                    const std::string wanted = genre;
                    const bool without = rec.genres.empty() ||
                                         std::all_of(rec.genres.begin(), rec.genres.end(),
                                                     [](const std::string &g) { return g.empty(); });
                    if (wanted == "Ohne Genre" ? !without :
                        std::find(rec.genres.begin(), rec.genres.end(), wanted) == rec.genres.end())
                        continue;
                }
                BrowserEntry entry;
                entry.kind = BrowserEntry::Kind::Recording;
                entry.title = DisplayName(rec);
                entry.fileName = rec.fileName;
                entries.push_back(std::move(entry));
            }
        }
        auto startFor = [this](const BrowserEntry &entry) -> time_t {
            for (const BrowserRecording &r : source)
                if (r.fileName == entry.fileName) return r.start;
            return 0;
        };
        std::stable_sort(entries.begin(), entries.end(), [&](const BrowserEntry &a, const BrowserEntry &b) {
            if (a.kind != b.kind)
                return a.kind != BrowserEntry::Kind::Recording;
            if (sort == Sort::Newest && a.kind == BrowserEntry::Kind::Recording)
                return startFor(a) > startFor(b);
            return a.title < b.title;
        });
        KeepVisible();
    }

    void KeepVisible()
    {
        selected = std::clamp(selected, 0, std::max(0, int(entries.size()) - 1));
        const int pageStart = (selected / kColumns - kVisibleRows + 1) * kColumns;
        if (selected < firstVisible)
            firstVisible = (selected / kColumns) * kColumns;
        else if (selected >= firstVisible + kVisibleItems)
            firstVisible = std::max(0, pageStart);
        firstVisible = std::max(0, firstVisible);
    }

    void ChangeView(View next)
    {
        view = next;
        folder.clear();
        genre.clear();
        selected = firstVisible = 0;
        Refresh();
    }

    void ToggleSort()
    {
        const std::string previous = Selected() ? Selected()->fileName : "";
        sort = sort == Sort::Newest ? Sort::Name : Sort::Newest;
        Refresh();
        if (!previous.empty())
            for (int i = 0; i < int(entries.size()); ++i)
                if (entries[i].fileName == previous) {
                    selected = i;
                    KeepVisible();
                    break;
                }
    }

    const BrowserEntry *Selected() const
    {
        return selected >= 0 && selected < int(entries.size()) ? &entries[selected] : nullptr;
    }

    void Move(int dx, int dy)
    {
        if (entries.empty()) return;
        if (dy < 0 && selected < kColumns) {
            tabsFocused = true;
            return;
        }
        const int target = selected + dx + dy * kColumns;
        if (target >= 0 && target < int(entries.size())) {
            selected = target;
            KeepVisible();
        }
    }

    // On a folder/genre opens that level. On a recording, returns its file name.
    std::string Open()
    {
        const BrowserEntry *entry = Selected();
        if (!entry) return {};
        if (entry->kind == BrowserEntry::Kind::Recording)
            return entry->fileName;
        if (entry->kind == BrowserEntry::Kind::Folder)
            folder = entry->path;
        else
            genre = entry->path;
        selected = firstVisible = 0;
        tabsFocused = false;
        Refresh();
        return {};
    }

    // true = moved to parent; false = already at view root.
    bool Back()
    {
        if (view == View::Folders && !folder.empty())
            folder = ParentFolder(folder);
        else if (view == View::Genres && !genre.empty())
            genre.clear();
        else
            return false;
        selected = firstVisible = 0;
        tabsFocused = false;
        Refresh();
        return true;
    }
};

} // namespace suiteosd
#endif
