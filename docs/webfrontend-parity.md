# Webfrontend-Parität – SuiteOSD nach dem ersten TV-Test

Quellenbasis: [VDR-Suite main 52c67da](https://github.com/hotzenplotz5/vdr-suite/tree/52c67da2c74a2757785ded3eb3bccecc703756ce) und [SuiteOSD 0.9.5-dev, 8bcb0a1](https://github.com/hotzenplotz5/vdr-plugin-suiteosd/tree/8bcb0a1406d02c5169543d4808a29e2718d52354). Hardwareevidenz: Nutzerfotos vom 25.11.2026 (Home, Live-Kanalinfo, EPG, Aufnahmen, Genre, Live-Vorschau, Replay, native Aufnahmeinfo). Quellenmatrix, keine vollständige Laufzeit- oder Einzelbutton-Abnahme.

| Bereich | Web-Owner (Datei unter web/frontend) | SuiteOSD-Basis | Fehlende bzw. zu prüfende Parität |
| --- | --- | --- | --- |
| Home | home-live-hero.js, home-now-next.js, home-recording-discovery.js, home-continue-watching.js | Snapshot, Jetzt/Nächste, Aufnahmen | Weiterschauen, zuletzt gesehen, Genre-/Ordner-/Serienreihen, tatsächliche Home-Video-Vorschau, Kontextaktionen |
| Kanäle | modules/channels.js | native Kanalinfo, EPG-Gruppen, Live-TV | eigene Kanalansicht, Filter/Paging, Senderprogramm und Aktionen |
| EPG | app.js, epg-detail-owner.js, epg-metadata-detail.js | fünf Ansichten, Logos, Detail, Timererstellung | Detail-Parität, Metadaten/Personen-Verknüpfungen, Suche, Live-Aktualisierung, Timer-/Suchtimer-Aktionen |
| Aufnahmenbrowser | recordings2.js, recordings2-browser-view.js, recordings2-browser-cards.js | Ordner/Genre/Alle, Sortierung, Löschdialog, PIP | Web-Sortierung/Filter/Paging, Titelprojektion, konsistente Zähler, Ordnerbilder, Auswahl/Mehrfachaktionen prüfen |
| Aufnahmedetail | recordings2-hero-detail.js, recordings2-metadata-detail.js, recordings2-metadata-view.js | Home/Browser-Detail, native VDR-Detailseite, Replay-Info | einheitliche Detailnavigation, alle Fakten, Bildergalerie, Zuordnung, Personen/Verknüpfungen, Scrollgrenzen |
| Personen | recordings2-person-search-view.js | TVScraper-Service liefert lokale Namen/Rollen/Portraitpfade | klickbare Cast-Karten, gleiche Aufnahme-Treffer und Detail-/Play-Aktionen; Biografie nur bei belegter Quelle |
| Globale Suche | modules/global-search.js | kein durchgängiger eigener Sucheinstieg; epgsearch-Handoff ist etwas anderes | Bildschirmtastatur, Titel/Personen, Treffergruppen Aufnahmen/EPG/Personen, Bilder, Treffergründe, Back/Fokus |
| Serien/Filmreihen | home-recording-discovery.js, recordings2-metadata-assignment.js | Metadaten mit Staffel/Episode | Hierarchie, Staffel-/Episodenlisten, Cover, Zuordnung/Editor, Sammlungen |
| Bearbeitung | recordings2-actions.js, recordings2-move-editor.js, recordings2-marks-editor.js | native VDR-Menüs und Browser-Löschdialog; VDR-Replay-Keys | Umbenennen/Verschieben, Ordnerwahl, Schnittmarken/Cut, Ergebnisprüfung, identische native Aktionen |
| Timer | modules/timers.js | native kTimers und EPG-Erstellung | Konfliktanzeige, Formulare, Verzeichnisse, Edit/Delete/Readback im Suite-Design prüfen |
| Suchtimer | modules/searchtimers.js | epgsearch/epgsearchonly-Handoff | echtes Suchtimer-Menü, Preview, Filter/Verzeichnis/Workflow/Readback; keine zweite Suchregel-Engine |
| Media Tools | modules/media-import.js | erste Core-Settings, kein vollständiger Medienjob-UI | Import/Prüfen/Metadaten, Batch, Fortschritt, Ergebnis, bestehendes SSE |
| Bilder | image_next-Vertrag im SuiteOSD, docs/image-next-integration.md | Galerie/Raster/Karussell-Code | echtes installiertes image_next, Quellen, Vollbild/Diashow, Bedienabnahme |
| Einstellungen | settings-*-Dateien | native VDR-/Plugin-Setup, erste Core-Admin-Aktionen | vollständiger Vergleich Konten, Geräte, Addons, Transcode/Medien, Pairing/Rechte mit bestehenden Ownern |
| HbbTV/Teletext/Replay | live-tv-view.js, teletext-view.js, recordings2-playback.js | bestehende Plugins und nativer Player | Handoff, Tonspuren, Untertitel, Seek/Marks, Rückkehr; Hardware-Abnahme |
| Gestaltung | bestehendes Webdesign und docs/approved-tv-home-design.md | TrueColor, Logos, Fanart | echte Icons, Kontrast, Artwork, Textumbrüche, Fokus und einheitliche Details |

## Personen und Suche – nachgewiesener Vertrag

- TVScraper `cScraperVideo::getCharacters(true)` liefert `getPersonName()`, `getCharacterName()`, Typ und lokalen Bildpfad. Keine belegte stabile Personen-ID/Biografie im nativen Service. Werte unter gültigem VDR-Objekt kopieren; Bilddekodierung erst danach.
- Web `recordings2-person-search-view.js::findRecordings` nutzt `fetchClientRecordingPersons` mit gewähltem Backend, Name, limit 20, same-origin, no-store. `api/client-api.js` sendet an `/api/vdr/recordings/persons/search`. Ergebnisse aus `matches[].recording`; Klick öffnet vorhandene Detail-/Wiedergabefunktionen. Fehler und leer sind getrennt.
- Web `modules/global-search.js` nutzt `fetchClientGlobalSearch` an `/api/search`, Backend, query, limit 24, offset 0; Resultatgruppen Aufnahmen, EPG und Personen inkl. Treffergrund und Portrait. Alte Suchanfragen werden abgebrochen/ignoriert. Das ist nicht ein Suchtimer.
- Vorhandene Owner: `GlobalSearchController`, `RecordingPersonSearchController`, `PersonController`, persistente Suchprojektionen. Kein zusätzlicher Index im VDR-Prozess, kein TVScraper-Vollscan bei jedem Klick.
- OSD-Session und erlaubter Backend-Kontext müssen mit den bestehenden Ownern zusammenpassen. Keine neue Authentisierung, kein pauschaler Root-/Device-Bypass, keine neuen Grants. Normale native VDR-Aktionen bleiben unabhängig. Native Such-/Detailprojektionen müssen dieselben Treffer, Titel, Reihenfolge und Paging-Semantik wie der vorhandene Dienst erhalten.
- Der `cSkinDisplayMenu` ist ein passiver Renderer. Native Standardmenü-Tasten dürfen nicht durch einen erfundenen Skin-Key-Handler umgeleitet werden; klickbare Personen gehören in ein echtes VDR-Menü/OSD-Objekt.

## Akzeptanzmatrix OSD.8–11

| Aspekt | Referenz / Prüfung |
| --- | --- |
| Eingaben | gleiche Personennamen (auch Umlaute), Titel, Backend; gleiche native Aufnahme/Event-Identität |
| Ergebnisse | keine Doppeleinträge durch Rollen-/Sprachaliase; gleiche Trefferanzahl/Sortierung des Index; stabile Identität beim Öffnen |
| Metadaten | Titel, Episodenname, Rollen, Genre, Bildfallback; keine erfundenen Biografien |
| Paging | Cast ohne Abschneiden der Gesamtliste; Suchlimit/Offset des bestehenden Dienstes; später weitere Seiten erreichbar |
| Fehler | keine Ergebnisse vs. Dienstfehler getrennt; fehlende Portraits nur Textfallback |
| Berechtigungen | bestehende Sitzung/Backend-Sicht; keine zusätzlichen Regeln und kein Zugriff über fremde Backend-IDs |
| Live-Integration | Nutzer wählt Person -> Suche im vorhandenen Index -> Aufnahme/EPG-Detail -> Wiedergabe/Timer über bisherigen Owner; Zurück stellt Detail/Fokus wieder her |
| Regression | Headless-Modelle + echter Servicevergleich Web/OSD mit I, Robot / Will Smith und einer Serie; Live-TV-Vorschau/Replay-Rückkehr am Gerät |

Kodi/Plex sind UX-Referenzen für interaktive Medienbibliotheken. Ein aktueller, quellengestützter Wettbewerbsvergleich und vollständiger Web/OSD-Button-Test sind noch offen; keine behauptete Gesamtparität.
