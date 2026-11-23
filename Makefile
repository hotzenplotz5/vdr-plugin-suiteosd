# Native VDR Suite OSD plugin – source build only (no live installation).
PLUGIN = suiteosd
PKG_CONFIG ?= pkg-config
CXX ?= c++

VDR_CXXFLAGS = $(shell $(PKG_CONFIG) --variable=cxxflags vdr)
VDR_CFLAGS = $(shell $(PKG_CONFIG) --variable=cflags vdr)
VDR_API_VERSION = $(shell $(PKG_CONFIG) --variable=apiversion vdr)

SOFILE = libvdr-$(PLUGIN).so
OBJECTS = suiteosd.o artwork_decode.o
# VDR_SOURCE_PARENT may point to the parent of a VDR 2.8.x source tree
# named "vdr". Example: VDR_SOURCE_PARENT=upstream with upstream/vdr/plugin.h.
SOURCE_FLAGS = $(if $(VDR_SOURCE_PARENT),-I$(VDR_SOURCE_PARENT))

CXXFLAGS += -std=c++17 -fPIC $(SOURCE_FLAGS) $(VDR_CXXFLAGS) $(VDR_CFLAGS) $(shell $(PKG_CONFIG) --cflags libpng)
ARTWORK_LIBS = $(shell $(PKG_CONFIG) --libs libpng) -ljpeg
DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

.PHONY: all clean show-vdr-api check-navigation check-artwork
all: $(SOFILE)

$(SOFILE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $^ $(ARTWORK_LIBS)

suiteosd.o: suiteosd.c navigation.h artwork_decode.h vendor/tvscraper/services.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(DEFINES) -c -o $@ $<

artwork_decode.o: artwork_decode.c artwork_decode.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

check-artwork:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o artwork-test tests/artwork_test.cpp artwork_decode.c $(ARTWORK_LIBS)
	./artwork-test
	rm -f artwork-test

check-navigation:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o navigation-test tests/navigation_test.cpp
	./navigation-test
	rm -f navigation-test

show-vdr-api:
	@printf 'VDR_API_VERSION=%s\n' '$(VDR_API_VERSION)'

clean:
	rm -f $(OBJECTS) $(SOFILE) artwork-test navigation-test
