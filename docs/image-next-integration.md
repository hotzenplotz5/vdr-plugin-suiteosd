# SuiteOSD ↔ image_next: Bildergalerie (VDR Service v1)

**Status:** source implementation, built-in test coverage; real VDR / remote
acceptance remains open. Neither plugin needs a web API, additional account
rights, a browser, or its own video decoding.

## Ownership
- **suiteosd** (0.9.3-dev): renders the dark Suite gallery, folder/source
  navigation, selected tile, status panel and remote-control actions.
- **image_next** (0.7.0-rc1 development branch): owns the configured
  `imagesources.conf` sources, allowed directory listing, thumbnails and
  the existing VDR fullscreen/slideshow `cControl` player.
- The shared `image-next-service-v1.h` exists in both plugin repositories;
  changing the contract requires synchronized updates and CI.

## Native local VDR API
The caller looks up `cPluginManager::GetPlugin("image_next")`, then calls
`cPlugin::Service()` with the following identifiers and request objects:
- `ImageNext-Browse-v1.0` — sources (`source=-1`) or visible directory
  entries (`source` index, relative `directory`).
- `ImageNext-Thumbnail-v1.0` — enqueue/copy a bounded ARGB thumbnail from
  image_next's existing asynchronous cache. `ready=false` means try later.
- `ImageNext-Play-v1.0` — start the existing image_next picture control
  at the selected file, with optional automatic slideshow.

No direct filesystem enumeration or image decoding occurs in SuiteOSD.
The image_next service resolves requested paths to configured source roots
and rejects traversal or symlinks escaping those roots. Only image extensions
supported by image_next may be played. File paths are not returned as
unrestricted absolute filesystem paths.

## Remote and lifecycle
- SuiteOSD sidebar **Bilder**: enter the native gallery.
- **OK / Rot**: enter a source/folder or open the selected image.
- **Grün**: start the selected image in slideshow mode.
- **Gelb / Info**: show the selected file/folder name.
- **Blau / Zurück**: parent folder, source list, then Suite Home.
- **Pfeile**: move over the visible 5-column tile grid.
- **Channel +/-**: page forward/backward.
- **0**: refresh the current source/folder and its thumbnails.
- **Links** at the leftmost tile: focus SuiteOSD sidebar.

If image_next is not installed or predates this contract, the gallery
displays a helpful message instead of opening arbitrary folders. If the
decoder needs time, placeholders remain visible until async thumbnails
arrive. SuiteOSD holds at most 20 ready thumbnails in foreground memory.
Fullscreen playback is still owned by image_next (TrueColor OSD, with
legacy MPEG-2 fallback according to its own setup).

## Building and accepting
1. Build both plugins for the **same VDR ABI**. image_next source branch
   `fix/skindesigner-grid-20261125` implements the required services.
2. Configure image_next's imagesources.conf in VDR as usual.
3. Run `make check-photo-gallery` and the normal suiteosd CI build.
4. On a real VDR 2.8.4 with a 32-bit OSD, confirm visible thumbnails,
   remote folder navigation, back/return behavior, playback and slideshow;
   check the fallback when image_next or a source is unavailable.
5. Test at least one non-ASCII filename and one inaccessible source.

No separate SuiteOSD image decoder or parallel VDR player is introduced.
