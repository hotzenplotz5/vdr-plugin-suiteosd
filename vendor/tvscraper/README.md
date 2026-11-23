TVScraper service header (not a fork of the plugin)

Source: https://github.com/MarkusEh/vdr-plugin-tvscraper
Upstream pinned commit: 2e63f40aff747f6dd8786aa8c2c7a4ef65cd5f08
Source file: services.h, blob 4af17bad1cc003193970d0a284afbb5f156a9ab0
License: GNU GPL-2.0-or-later (see COPYING).
The header provides the public VDR in-process Service() ABI, not any scraper implementation, database, images, or remote access.
Runtime use requires an installed TVScraper plugin. No API v1 request is used.
When upgrading the installed plugin, review this service contract for compatibility.
