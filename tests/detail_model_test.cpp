#include "../detail_model.h"
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
}
