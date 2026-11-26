#include "../detail_model.h"
#include "../search_navigation.h"
#include <cassert>

int main()
{
    using namespace suiteosd;
    DetailMetadata data;
    AddCredit(data, {"Will Smith", "Schauspiel", "Del Spooner", ""});
    AddCredit(data, {"Will Smith", "Schauspiel", "Del Spooner", "portrait.png"});
    AddCredit(data, {"Will Smith", "Produktion", "", ""});
    AddCredit(data, {"", "Schauspiel", "", ""});
    assert(data.people.size() == 2);
    assert(data.people[0].portraitPath == "portrait.png");
    assert(CreditText(data) == "Will Smith · Schauspiel · Del Spooner\nWill Smith · Produktion");
    CreditNavigation nav;
    nav.Open();
    for (int i = 0; i < 20; ++i) nav.Move(1, 12);
    assert(nav.selected == 11 && nav.first == 6);
    nav.Move(-10, 12);
    assert(nav.selected == 1 && nav.first == 1);
    nav.Close();
    assert(!nav.active && nav.selected == 0 && nav.first == 0);
    nav.Open();
    nav.Move(1, 0);
    assert(nav.selected == 0 && nav.first == 0);
    data.release="2004-07-15"; data.runtime=115; data.genres={"Action","Science-Fiction"};
    assert(HeroFacts(data)=="2004  ·  115 Min.  ·  Action  ·  Science-Fiction");
    SearchNavigation search;
    search.Open("Will Smith");
    assert(search.person && search.page == SearchPage::Results);
    search.NextPage();
    assert(search.offset == 20 && search.selected == 0);
    search.CycleTab();
    assert(search.tab == SearchTab::Epg && search.offset == 0);
    search.CycleTab();
    assert(search.tab == SearchTab::Recordings);
    search.Open();
    search.key = 26;
    search.Type();
    assert(search.query == "ä");
    search.Erase();
    assert(search.query.empty());
    search.Move(9, 12);
    assert(search.First() == 6);
}
