#include "../navigation.h"
#include <cassert>

int main()
{
    using namespace suiteosd;
    Navigation n;
    assert(n.section == 0 && !n.sideFocused);
    n.Move(Direction::Left, 1, 5);
    assert(n.sideFocused); // collapsed icon rail expands all labels when focused
    n.Move(Direction::Down, 1, 5);
    assert(n.sideIndex == 1);
    n.Move(Direction::Right, 1, 5);
    assert(!n.sideFocused); // exit sidebar without activating
    n.Move(Direction::Down, 9, 4);
    assert(n.section == 1);
    for (int i = 0; i < 7; ++i)
        n.Move(Direction::Right, 9, 4);
    assert(n.card[1] == 7 && n.firstCard[1] == 4);
    n.Move(Direction::Down, 3, 4);
    assert(n.section == 2);
    assert(n.firstCard[1] == 4);
    n.Move(Direction::Down, 6, 4);
    assert(n.section == 3 && n.firstVisibleSection == 2);
    assert(n.Move(Direction::Select, 6, 4) == Activation::Recording);
    n.Move(Direction::Left, 6, 4);
    assert(n.sideFocused);
    n.Move(Direction::Up, 6, 4);
    assert(n.sideIndex == 0);
    // Every icon remains focusable; the last entry is Settings.
    n.Move(Direction::Down, 6, 4);
    for (int i = 0; i < 20; ++i) n.Move(Direction::Down, 6, 4);
    assert(n.sideIndex == Navigation::kSidebarItems - 1);
    for (int i = 0; i < 20; ++i) n.Move(Direction::Up, 6, 4);
    assert(n.sideIndex == 0);
    assert(n.Move(Direction::Select, 1, 4) == Activation::None);
    assert(n.section == 0 && !n.sideFocused);
    assert(n.Move(Direction::Back, 1, 4) == Activation::Close);

    Navigation direct;
    direct.Move(Direction::Left, 1, 4);
    direct.Move(Direction::Down, 1, 4);
    direct.Move(Direction::Down, 1, 4);
    direct.Move(Direction::Down, 1, 4);
    assert(direct.sideIndex == 3);
    assert(direct.Move(Direction::Select, 6, 4) == Activation::Module);
    assert(direct.sideIndex == 3); // recordings native VDR menu
    assert(!direct.sideFocused);
    assert(direct.section == 0); // Suite Home stays unchanged

    // Every non-Home icon calls its own native menu/plug-in adapter,
    // not another location of the Home carousel.
    for (int index = 1; index < Navigation::kSidebarItems; ++index) {
        Navigation menu;
        menu.Move(Direction::Left, 1, 4);
        for (int j = 0; j < index; ++j)
            menu.Move(Direction::Down, 1, 4);
        assert(menu.sideIndex == index);
        assert(menu.Move(Direction::Select, 1, 4) == Activation::Module);
    }
    return 0;
}
