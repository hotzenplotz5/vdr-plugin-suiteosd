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
    for (int i = 0; i < 2; ++i) model.Move(0, 1);
    assert(model.selected == 8 && model.firstVisible == 8);
    model.Page(1);
    assert(model.selected == 16 && model.firstVisible == 16);
    model.Move(-1, 0); // first grid column must not go backwards
    assert(model.selected == 16);
    // Carousel is one photo per left/right key, independent of grid paging.
    model.SetView(suiteosd::PhotoGallery::View::Carousel);
    model.MoveCarousel(1);
    assert(model.selected == 17);
    model.MoveCarousel(-1);
    assert(model.selected == 16);
    model.MoveCarousel(-100);
    assert(model.selected == 0);
    model.MoveCarousel(999);
    assert(model.selected == 41);
    model.SetView(suiteosd::PhotoGallery::View::Grid);
    assert(model.view == suiteosd::PhotoGallery::View::Grid);
    model.SetView(suiteosd::PhotoGallery::View::Carousel);

    using G = suiteosd::PhotoGallery;
    assert(G::kVisible == 8);
    assert(G::GridStart(4,6)==0 && G::GridSpan(4,6)==2);
    assert(G::GridStart(5,6)==2 && G::GridSpan(5,6)==2);
    assert(G::GridStart(4,7)==0 && G::GridSpan(4,7)==2);
    assert(G::GridStart(5,7)==2 && G::GridSpan(5,7)==1);
    assert(G::GridStart(6,7)==3 && G::GridSpan(6,7)==1);
    model.SetEntries(std::vector<imagenext::Entry>(items.begin(),items.begin()+6));
    model.SetView(G::View::Grid);
    model.Move(1,0);model.Move(1,0);model.Move(1,0);
    model.Move(0,1);assert(model.selected==5);
    model.Move(0,-1);assert(model.selected==2);
    model.Move(0,1);assert(model.selected==5);
    model.Move(-1,0);assert(model.selected==4);

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
