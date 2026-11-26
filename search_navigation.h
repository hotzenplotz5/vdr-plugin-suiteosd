#ifndef SUITEOSD_SEARCH_NAVIGATION_H
#define SUITEOSD_SEARCH_NAVIGATION_H
#include <algorithm>
#include <string>
#include <vector>
namespace suiteosd {
enum class SearchPage { Query, Login, Results, Recording, Event, PublicRecording };
enum class SearchTab { Recordings, Epg, People };
struct SearchNavigation {
    static constexpr int pageSize = 20, visibleRows = 6, columns = 10;
    SearchPage page = SearchPage::Query;
    SearchTab tab = SearchTab::Recordings;
    std::string query;
    bool person = false, uppercase = false;
    int key = 0, offset = 0, selected = 0;
    void Open(const std::string &name = "") {
        *this = SearchNavigation{};
        query = name; person = !name.empty();
        if (person) page = SearchPage::Results;
    }
    static const std::vector<std::string> &Keys() {
        static const std::vector<std::string> keys = {
            "a","b","c","d","e","f","g","h","i","j",
            "k","l","m","n","o","p","q","r","s","t",
            "u","v","w","x","y","z","ä","ö","ü","ß",
            "0","1","2","3","4","5","6","7","8","9",
            " ","-",".","&","'","!",":",",","/","?"
        };
        return keys;
    }
    std::string KeyLabel(int index) const {
        std::string s = Keys()[index];
        if (uppercase) {
            if (s == "ä") return "Ä";
            if (s == "ö") return "Ö";
            if (s == "ü") return "Ü";
            if (s == "ß") return "ẞ";
            if (s.size() == 1 && s[0] >= 'a' && s[0] <= 'z')
                s[0] = char(s[0] - 'a' + 'A');
        }
        return s;
    }
    void MoveKey(int dx, int dy) {
        int col = std::clamp(key % columns + dx, 0, columns - 1);
        int row = std::clamp(key / columns + dy, 0, int(Keys().size()) / columns - 1);
        key = row * columns + col;
    }
    void Type() { query += KeyLabel(key); }
    void Erase() {
        if (query.empty()) return;
        size_t i = query.size() - 1;
        while (i && (static_cast<unsigned char>(query[i]) & 0xC0) == 0x80) --i;
        query.erase(i);
    }
    void Move(int delta, int count) {
        selected = std::clamp(selected + delta, 0, std::max(0, count-1));
    }
    int First() const { return selected / visibleRows * visibleRows; }
    void CycleTab() {
        tab = static_cast<SearchTab>((int(tab) + 1) % (person ? 2 : 3));
        offset = selected = 0;
    }
    void NextPage() { offset += pageSize; selected = 0; }
    void PreviousPage() { offset = std::max(0, offset-pageSize); selected = 0; }
};
} // namespace suiteosd
#endif
