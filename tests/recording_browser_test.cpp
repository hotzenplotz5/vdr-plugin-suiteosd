#include "../recording_browser.h"
#include <cassert>
#include <string>

using namespace suiteosd;
static BrowserRecording rec(const std::string &id, const std::string &name,
                            time_t start, std::vector<std::string> genre = {})
{
    return {id, name, "", start, genre};
}

int main()
{
    RecordingBrowser b;
    b.source = {
        rec("one", "Action~Thriller~Film A", 10, {"Action", "Drama"}),
        rec("two", "Action~Film B", 30, {"Action"}),
        rec("three", "Dokumentation~Reportage", 20, {"Dokumentation"}),
        rec("four", "News", 40)
    };
    b.Refresh();
    assert(b.entries.size() == 3);
    assert(b.entries[0].title == "Action");
    assert(b.entries[0].kind == BrowserEntry::Kind::Folder);
    assert(b.entries[0].count == 2);
    assert(b.entries[1].title == "Dokumentation");
    assert(b.entries[2].fileName == "four");
    b.tabsFocused = false;
    assert(b.Open().empty());
    assert(b.folder == "Action");
    assert(b.entries.size() == 2);
    assert(b.entries[0].title == "Thriller");
    assert(b.entries[1].fileName == "two");
    assert(b.Open().empty());
    assert(b.folder == "Action~Thriller");
    assert(b.entries.size() == 1);
    assert(b.Open() == "one");
    assert(b.Back() && b.folder == "Action");
    assert(b.Back() && b.folder.empty());
    assert(!b.Back());

    b.ChangeView(RecordingBrowser::View::Genres);
    assert(b.entries.size() == 4); // Action, Dokumentation, Drama, Ohne Genre
    assert(b.entries[0].title == "Action" && b.entries[0].count == 2);
    assert(b.entries[3].title == "Ohne Genre");
    b.Open();
    assert(b.genre == "Action");
    assert(b.entries.size() == 2);
    assert(b.entries[0].fileName == "two"); // newest first
    assert(b.entries[1].fileName == "one");
    b.ToggleSort();
    assert(b.sort == RecordingBrowser::Sort::Name);
    assert(b.entries[0].fileName == "one");
    assert(b.Back() && b.genre.empty());

    b.ChangeView(RecordingBrowser::View::All);
    assert(b.entries.size() == 4);
    assert(b.entries[0].fileName == "four"); // newest
    b.tabsFocused = false;
    b.Move(0, 1);
    assert(b.tabsFocused); // first grid row -> tabs

    RecordingBrowser many;
    for (int i = 0; i < 25; ++i)
        many.source.push_back(rec(std::to_string(i), "Item" + std::to_string(i), i));
    many.ChangeView(RecordingBrowser::View::All);
    many.tabsFocused = false;
    for (int i = 0; i < 4; ++i) many.Move(0, 1);
    assert(many.selected == 16);
    assert(many.firstVisible >= 4 && many.selected < many.firstVisible + many.kVisibleItems);
    assert(many.Back() == false);
}
