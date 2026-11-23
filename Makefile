# Native VDR Suite OSD plugin – source build only (no live installation).
PLUGIN = suiteosd
PKG_CONFIG ?= pkg-config
CXX ?= c++

VDR_CXXFLAGS = $(shell $(PKG_CONFIG) --variable=cxxflags vdr)
VDR_CFLAGS = $(shell $(PKG_CONFIG) --variable=cflags vdr)
VDR_API_VERSION = $(shell $(PKG_CONFIG) --variable=apiversion vdr)

SOFILE = libvdr-$(PLUGIN).so
OBJECTS = suiteosd.o
# VDR_SOURCE_PARENT may point to the parent of a VDR 2.8.x source tree
# named "vdr". Example: VDR_SOURCE_PARENT=upstream with upstream/vdr/plugin.h.
SOURCE_FLAGS = $(if $(VDR_SOURCE_PARENT),-I$(VDR_SOURCE_PARENT))

CXXFLAGS += -std=c++17 -fPIC $(SOURCE_FLAGS) $(VDR_CXXFLAGS) $(VDR_CFLAGS)
DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

.PHONY: all clean show-vdr-api check-navigation
all: $(SOFILE)

$(SOFILE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $^

suiteosd.o: suiteosd.c navigation.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(DEFINES) -c -o $@ $<

check-navigation:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o /tmp/suiteosd-navigation-test.$$$$ tests/navigation_test.cpp && \
	/tmp/suiteosd-navigation-test.$$$$; status=$$?; \
	rm -f /tmp/suiteosd-navigation-test.*; \
	exit $$status

show-vdr-api:
	@printf 'VDR_API_VERSION=%s\n' '$(VDR_API_VERSION)'

clean:
	rm -f $(OBJECTS) $(SOFILE)
