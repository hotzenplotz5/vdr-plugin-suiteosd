# Native VDR Suite OSD plugin – source build (no automatic installation).
# Uses the installed VDR pkg-config metadata and ABI.
PLUGIN = suiteosd
PKG_CONFIG ?= pkg-config
CXX ?= c++

VDR_CXXFLAGS = $(shell $(PKG_CONFIG) --variable=cxxflags vdr)
VDR_CFLAGS = $(shell $(PKG_CONFIG) --variable=cflags vdr)
VDR_API_VERSION = $(shell $(PKG_CONFIG) --variable=apiversion vdr)

SOFILE = libvdr-$(PLUGIN).so
OBJECTS = suiteosd.o

CXXFLAGS += -std=c++17 -fPIC $(VDR_CXXFLAGS) $(VDR_CFLAGS)
DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

.PHONY: all clean show-vdr-api
all: $(SOFILE)

$(SOFILE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $^

suiteosd.o: suiteosd.c
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(DEFINES) -c -o $@ $<

show-vdr-api:
	@printf 'VDR_API_VERSION=%s\n' '$(VDR_API_VERSION)'

clean:
	rm -f $(OBJECTS) $(SOFILE)
