# Native VDR Suite OSD plugin – source build only (no live installation).
PLUGIN = suiteosd
PKG_CONFIG ?= pkg-config
CXX ?= c++

VDR_CXXFLAGS = $(shell $(PKG_CONFIG) --variable=cxxflags vdr)
VDR_CFLAGS = $(shell $(PKG_CONFIG) --variable=cflags vdr)
VDR_API_VERSION = $(shell $(PKG_CONFIG) --variable=apiversion vdr)

SOFILE = libvdr-$(PLUGIN).so
OBJECTS = suiteosd.o suite_skin.o suite_menu.o artwork_decode.o core_admin_client.o
# VDR_SOURCE_PARENT may point to the parent of a VDR 2.8.x source tree
# named "vdr". Example: VDR_SOURCE_PARENT=upstream with upstream/vdr/plugin.h.
SOURCE_FLAGS = $(if $(VDR_SOURCE_PARENT),-I$(VDR_SOURCE_PARENT))

CXXFLAGS += -std=c++17 -fPIC $(SOURCE_FLAGS) $(VDR_CXXFLAGS) $(VDR_CFLAGS) $(shell $(PKG_CONFIG) --cflags libpng librsvg-2.0 gdk-pixbuf-2.0 libwebp libcurl json-c)
ARTWORK_LIBS = $(shell $(PKG_CONFIG) --libs libpng librsvg-2.0 gdk-pixbuf-2.0 libwebp) -ljpeg
CORE_LIBS = $(shell $(PKG_CONFIG) --libs libcurl json-c libcrypto)
DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

.PHONY: all clean show-vdr-api check-navigation check-artwork check-epg-timeline check-logos check-recording-browser check-photo-gallery check-photo-carousel check-settings-navigation check-core-admin-navigation check-core-admin-http
all: $(SOFILE)

$(SOFILE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $^ $(ARTWORK_LIBS) $(CORE_LIBS)

suiteosd.o: suiteosd.c home_perf.h pairing_dialog.h cinematic_home.h home_layout.h home_cache.h web_palette.h sidebar_icons.h navigation.h core_admin_client.h core_admin_navigation.h settings_navigation.h recording_browser.h vendor/vdr-suite/CanonicalGenreRegistry.h vendor/vdr-suite/GenreClassification.h photo_gallery.h photo_carousel.h image-next-service-v1.h epg_timeline.h logo_locator.h artwork_decode.h scraper_detail.h detail_model.h recording_detail_view.h search_navigation.h vendor/tvscraper/services.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(DEFINES) -c -o $@ $<

suite_menu.o: suite_menu.c pairing_dialog.h web_palette.h artwork_decode.h scraper_detail.h detail_model.h recording_detail_view.h vendor/vdr-suite/CanonicalGenreRegistry.h vendor/vdr-suite/GenreClassification.h vendor/tvscraper/services.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(DEFINES) -c -o $@ $<

suite_skin.o: suite_skin.c web_palette.h artwork_decode.h logo_locator.h skin_registration.h vendor/tvscraper/services.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(DEFINES) -c -o $@ $<

core_admin_client.o: core_admin_client.c home_perf.h core_admin_client.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

artwork_decode.o: artwork_decode.c home_perf.h artwork_decode.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

check-artwork:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic $(shell $(PKG_CONFIG) --cflags libpng librsvg-2.0 gdk-pixbuf-2.0 libwebp) -o artwork-test tests/artwork_test.cpp artwork_decode.c $(ARTWORK_LIBS)
	./artwork-test
	rm -f artwork-test

check-logos:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o logo-locator-test tests/logo_locator_test.cpp
	./logo-locator-test
	rm -f logo-locator-test

check-epg-timeline:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o epg-timeline-test tests/epg_timeline_test.cpp
	./epg-timeline-test
	rm -f epg-timeline-test

check-core-admin-navigation:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o core-admin-navigation-test tests/core_admin_navigation_test.cpp
	./core-admin-navigation-test
	rm -f core-admin-navigation-test

check-core-admin-http:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -pthread -o core-admin-http-test tests/core_admin_http_test.cpp core_admin_client.c $(CORE_LIBS)
	./core-admin-http-test
	rm -f core-admin-http-test

check-settings-navigation:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o settings-navigation-test tests/settings_navigation_test.cpp
	./settings-navigation-test core-admin-navigation-test core-admin-http-test
	rm -f settings-navigation-test

check-photo-carousel:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o photo-carousel-test tests/photo_carousel_test.cpp
	./photo-carousel-test
	rm -f photo-carousel-test

check-photo-gallery:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o photo-gallery-test tests/photo_gallery_test.cpp
	./photo-gallery-test
	rm -f photo-gallery-test

check-recording-browser:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o recording-browser-test tests/recording_browser_test.cpp
	./recording-browser-test settings-navigation-test
	rm -f recording-browser-test

.PHONY: check-skin-registration
check-skin-registration:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o skin-registration-test tests/skin_registration_test.cpp
	./skin-registration-test
	rm -f skin-registration-test

.PHONY: check-detail-model
check-detail-model:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o detail-model-test tests/detail_model_test.cpp
	./detail-model-test
	rm -f detail-model-test

check-navigation:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -o navigation-test tests/navigation_test.cpp
	./navigation-test
	rm -f navigation-test

show-vdr-api:
	@printf 'VDR_API_VERSION=%s\n' '$(VDR_API_VERSION)'

clean:
	rm -f $(OBJECTS) $(SOFILE) artwork-test navigation-test epg-timeline-test logo-locator-test recording-browser-test photo-gallery-test photo-carousel-test
