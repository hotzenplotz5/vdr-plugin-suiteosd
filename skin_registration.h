#ifndef SUITEOSD_SKIN_REGISTRATION_H
#define SUITEOSD_SKIN_REGISTRATION_H
#include <string>

namespace suiteosd {
// VDR's cSkin constructor inserts itself into the owning intrusive list.
// Never append the newly constructed skin to that list a second time.
template<class Skin, class List>
void RegisterSkinOnce(List &skins, const char *name)
{
    for (auto *skin = skins.First(); skin; skin = skins.Next(skin))
        if (std::string(skin->Name()) == name) return;
    new Skin();
}
}
#endif
