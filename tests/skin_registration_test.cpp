#include "../skin_registration.h"
#include <cassert>
#include <memory>
#include <string>
#include <vector>

// Model the VDR cSkin constructor's self-registration and list ownership.
// This test exercises the same registration helper used by the native skin.
struct FakeSkin;
struct FakeList {
    std::vector<std::unique_ptr<FakeSkin>> items;
    FakeSkin *First();
    FakeSkin *Next(FakeSkin *skin);
    void Add(FakeSkin *skin);
};
static FakeList skins;
struct FakeSkin {
    explicit FakeSkin(const char *n) : name(n) { skins.Add(this); }
    virtual ~FakeSkin() = default;
    const char *Name() const { return name.c_str(); }
    std::string name;
};
FakeSkin *FakeList::First() { return items.empty() ? nullptr : items.front().get(); }
FakeSkin *FakeList::Next(FakeSkin *skin) {
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i].get() == skin)
            return i + 1 < items.size() ? items[i + 1].get() : nullptr;
    return nullptr;
}
void FakeList::Add(FakeSkin *skin) {
    for (const auto &item : items) assert(item.get() != skin);
    items.emplace_back(skin);
}
struct TestSkin : FakeSkin { TestSkin() : FakeSkin("suiteosd") {} };
int main() {
    new FakeSkin("lcars");
    suiteosd::RegisterSkinOnce<TestSkin>(skins, "suiteosd");
    assert(skins.items.size() == 2);
    auto *registered = skins.Next(skins.First());
    assert(registered && std::string(registered->Name()) == "suiteosd");
    assert(skins.Next(registered) == nullptr);
    suiteosd::RegisterSkinOnce<TestSkin>(skins, "suiteosd");
    assert(skins.items.size() == 2);
    assert(skins.Next(skins.First()) == registered);
}
