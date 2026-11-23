/*
 * suiteosd.c: native VDR entry point for the VDR Suite OSD.
 *
 * OSD.1 scaffold: independent of video output plugins and VDR-Suite transport.
 * This is not yet a Suite skin or an addon integration.
 */
#include <vdr/plugin.h>
#include <vdr/osdbase.h>

static const char *VERSION = "0.1.0";
static const char *DESCRIPTION = trNOOP("VDR Suite OSD");

class cSuiteOsdAbout final : public cOsdMenu {
public:
    cSuiteOsdAbout()
        : cOsdMenu(tr("About VDR Suite OSD"))
    {
        Add(new cOsdItem(tr("Native VDR frontend (prototype)"), osUnknown, false));
        Add(new cOsdItem(tr("Independent of the video output plugin"), osUnknown, false));
    }
};

class cSuiteOsdMenu final : public cOsdMenu {
public:
    cSuiteOsdMenu()
        : cOsdMenu(tr("VDR Suite"))
    {
        Add(new cOsdItem(tr("About")));
    }

    eOSState ProcessKey(eKeys key) override
    {
        eOSState state = cOsdMenu::ProcessKey(key);
        if (state == osUnknown && key == kOk && !HasSubMenu())
            return AddSubMenu(new cSuiteOsdAbout());
        return state;
    }
};

class cPluginSuiteOsd final : public cPlugin {
public:
    const char *Version(void) override { return VERSION; }
    const char *Description(void) override { return tr(DESCRIPTION); }
    const char *MainMenuEntry(void) override { return tr("VDR Suite"); }
    cOsdObject *MainMenuAction(void) override { return new cSuiteOsdMenu(); }
};

VDRPLUGINCREATOR(cPluginSuiteOsd);
