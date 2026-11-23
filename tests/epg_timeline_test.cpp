#include "../epg_timeline.h"
#include <cassert>
#include <cmath>

int main()
{
    using namespace suiteosd;
    Timeline t;
    const time_t now = 1763901000;
    for (int ch = 1; ch <= 15; ++ch) {
        TimelineChannel c;
        c.number = ch;
        c.name = "Channel " + std::to_string(ch);
        for (int i = -1; i < 55; ++i) {
            TimelineEvent e;
            e.start = now + i * 3600;
            e.end = e.start + 3600;
            e.title = "Event " + std::to_string(i);
            c.events.push_back(e);
        }
        t.channels.push_back(c);
    }

    t.Reset(now, 9);
    assert(t.channel == 8);
    assert(t.firstChannel == 2);
    assert(t.Selected() != nullptr);
    assert(t.dayPage == 0);
    assert(t.view == Timeline::View::Horizontal);
    t.MoveChannel(3);
    assert(t.channel == 11 && t.firstChannel == 5);
    const auto *old = t.Selected();
    assert(old);
    t.MoveEvent(1);
    const auto *next = t.Selected();
    assert(next && next->start > old->start);

    double left = -1, span = -1;
    TimelineEvent longEvent;
    longEvent.start = t.visibleStart - 3600;
    longEvent.end = t.visibleStart + 1800;
    assert(t.Geometry(longEvent, left, span));
    assert(left == 0.0);
    assert(std::fabs(span - 0.125) < 1e-10);
    longEvent.start = t.visibleStart + 20000;
    longEvent.end += 20000;
    assert(!t.Geometry(longEvent, left, span));

    t.SetPage(1);
    assert(t.dayPage == 1);
    assert(t.visibleStart == t.baseStart + 86400);
    t.NextView();
    assert(t.view == Timeline::View::Vertical);
    t.NextView();
    assert(t.view == Timeline::View::Live);
    t.NextView();
    assert(t.view == Timeline::View::Now);
    t.NextView();
    assert(t.view == Timeline::View::Next);
    t.NextView();
    assert(t.view == Timeline::View::Horizontal);
    t.JumpNow();
    assert(t.dayPage == 0);
    assert(t.Selected());
    t.MoveChannel(-1000);
    assert(t.channel == 0);
    t.MoveChannel(1000);
    assert(t.channel == 14);

    // VDR group separators become the same named EPG filter as Webfrontend.
    Timeline grouped;
    grouped.channels.push_back({1, "Das Erste", "Das Erste", "Öffentlich-Rechtlich", {}});
    grouped.channels.push_back({2, "ZDF", "ZDF", "Öffentlich-Rechtlich", {}});
    grouped.channels.push_back({3, "RTL", "RTL", "Private", {}});
    grouped.channels.push_back({4, "Unsorted", "Unsorted", "", {}});
    grouped.InitializeGroups();
    grouped.Reset(now, 2);
    assert(grouped.GroupLabel() == "Alle Sender");
    assert(grouped.channels.size() == 4);
    grouped.CycleGroup();
    assert(grouped.GroupLabel() == "Öffentlich-Rechtlich");
    assert(grouped.channels.size() == 2 && grouped.channels[grouped.channel].number == 2);
    grouped.CycleGroup();
    assert(grouped.GroupLabel() == "Private");
    assert(grouped.channels.size() == 1 && grouped.channels[0].number == 3);
    grouped.CycleGroup();
    assert(grouped.GroupLabel() == "Weitere Sender");
    assert(grouped.channels.size() == 1);
    grouped.CycleGroup();
    assert(grouped.GroupLabel() == "Alle Sender" && grouped.channels.size() == 4);

    Timeline empty;
    empty.Reset(now, 1);
    empty.MoveChannel(1);
    empty.MoveEvent(1);
    empty.SetPage(1);
    assert(empty.Selected() == nullptr);
    return 0;
}
