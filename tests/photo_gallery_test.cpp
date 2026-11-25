#include "../photo_gallery.h"
#include <cassert>
#include <string>

int main()
{
    suiteosd::PhotoGallery model;
    assert(model.InSources());
    assert(!model.Back());
    std::vector<imagenext::Source> roots = {{0, "Fotos"}, {1, "Archiv"}};
    model.SetSources(roots);
    model.Move(1, 0);
    assert(model.SelectedSource() && model.SelectedSource()->index == 1);
    model.OpenSource(1);
    assert(!model.InSources() && model.directory.empty());
    std::vector<imagenext::Entry> items;
    for (int i = 0; i < 42; ++i)
        items.push_back({"bild" + std::to_string(i) + ".jpg",
                         "Urlaub/bild" + std::to_string(i) + ".jpg",
                         "Urlaub/bild" + std::to_string(i) + ".jpg", false});
    model.SetEntries(items);
    for (int i = 0; i < 3; ++i) model.Move(0, 1);
    assert(model.selected == 15 && model.firstVisible == 15);
    model.Page(1);
    assert(model.selected == 30 && model.firstVisible == 30);
    model.Move(-1, 0); // first grid column must not go backwards
    assert(model.selected == 30);
    // Carousel is one photo per left/right key, independent of grid paging.
    model.SetView(suiteosd::PhotoGallery::View::Carousel);
    model.MoveCarousel(1);
    assert(model.selected == 31);
    model.MoveCarousel(-1);
    assert(model.selected == 30);
    model.MoveCarousel(-100);
    assert(model.selected == 0);
    model.MoveCarousel(999);
    assert(model.selected == 41);
    model.SetView(suiteosd::PhotoGallery::View::Grid);
    assert(model.view == suiteosd::PhotoGallery::View::Grid);
    model.SetView(suiteosd::PhotoGallery::View::Carousel);

    model.OpenDirectory("Urlaub/2018");
    assert(model.directory == "Urlaub/2018" && model.Count() == 0);
    assert(model.Back() && model.directory == "Urlaub");
    assert(model.Back() && model.directory.empty());
    assert(model.Back() && model.InSources());
    assert(!model.Back());
    model.SetSources({});
    model.Move(0, 1);
    assert(model.selected == 0 && model.Count() == 0);
    return 0;
}
