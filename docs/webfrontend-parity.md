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

OSD.10/11 Transportbasis: Der vorhandene Core-HTTP-Client bietet jetzt die read-only Methoden `recordingPersons` und `globalSearch` mit explizitem Backend, Query-Encoding, Counts, Paging und bestehenden Sitzungsheadern. HTTP-Mock prüft beide realen Web-Routen sowie UTF-8/Sonderzeichen und Metadatenprojektion. Noch nicht aus der Cast-/Suchoberfläche aufgerufen. Vor UI-Wiring: tatsächlich lokales Backend und bestehende Lese-Session auf yaVDR bestimmen; stabile Zuordnung der Ergebnisse zu nativen Aufnahme-/EPG-Aktionen nachweisen. Kein Hardcoding eines fremden Backend-Kontexts und kein zweiter Index.


## Reale Core-Leseprobe (26.11.2026)

Auf yaVDR wurden beide Web-GETs ohne Sitzung mit `authentication_required` beantwortet. Mit der bestehenden Human-Browsersitzung funktionieren beide Routen für `backend=default`, `Will Smith`, `limit=3`: Personensuche 10 Aufnahmen insgesamt, 3 geliefert (Rollen und native Identitäten); globale Suche 9 Aufnahmen, 1 EPG-Treffer, 3 Aufnahmen auf der Seite, Personenbildreferenz und `recordingHasMore=true`. Die unterschiedlichen Counts und Reihenfolgen werden beibehalten; ihre Ursache wurde nicht untersucht. Ein reduzierter HTTP-Regressionstest bildet diese Antwortformen mit synthetischen Dateipfaden ab. Das bestätigt den lesenden Vertrag, nicht die TV-Bedienung oder die aktuelle Existenz jedes Aufnahme-/Event-Objekts. Native Wiedergabe muss die konkrete Identität im lokalen VDR auflösen; keine Titelheuristik. Die bestehende Sitzung muss für Suche und Rückkehr sinnvoll wiederverwendet werden, ohne pro Klick eine Anmeldung zu erzwingen. Kein neuer Auth-Bypass, keine weitere Berechtigungsregel.


## Interaktive Suche im Suite-Menü (26.11.2026)

Die Suite-eigenen Home-/Aufnahmebrowser-Details öffnen über die Mitwirkenden-Auswahl per OK die vorhandene Personensuche. Eigener Sidebar-Eintrag Suche mit Bildschirmtastatur (inkl. Umlauten), Treffergruppen, Paging, native Aufnahme-Identität und Wiedergabe; EPG-Treffer öffnen zunächst eine Lese-Detailseite. Suche nutzt eine Human-Browsersitzung im Menü-Lebenszyklus; Login erst bei HTTP 401, Wiederverwendung für weitere Suchen, kein Passwortspeicher. Der native VDR-Kontext ist das vorhandene lokale `default`-Backend; fremde oder nicht mehr vorhandene Treffer werden nicht anhand des Titels als lokale Aufnahme geöffnet. Der passive Standard-VDR-Aufnahmeinfo-Skin und Replay-Info sind noch nicht vollständig mit der Suchnavigation verbunden. Echte Geräteabnahme, EPG-Timeraktion aus Suchtreffern, Trefferbilder, vollständige Detailgestaltung und Portierung weiterer Webfunktionen bleiben offen.


0.9.6-dev: Der gemeinsame Aufnahme-Hero verwendet die konkrete Webfrontend-Vorlage `recordings2-hero-detail.js` (dunkler Hintergrund, großflächige Fanart, Poster, Fakten, Inhalt, Mitwirkenden-Leiste). Die interaktive Suite-Aufnahmeinfo navigiert direkt auf der sichtbaren Cast-Leiste; die native Standard-VDR-Info nutzt denselben Renderer, bleibt jedoch unter VDR-Tasten-Ownership und verweist auf den Suite-Aufnahmenbrowser. Die Suite-Replay-Info übergibt eine ausgewählte Person per bestehendem VDR-Plugin-Handoff an die Suchansicht; Rückkehr/Fokus im Replay noch hardwareseitig zu prüfen. Keine neue VDR-Core-Abhängigkeit und kein Ausgabeplugin-Fork.


## Home 0.9.7 – Core-Datenreihen

Aus `home-continue-watching.js`, `home-recently-watched.js`, `home-recording-discovery.js` und `genre-client-api.js` übernommen: bestehende Human-Sitzung, POST `operation:list` für Weiterschauen/Zuletzt angesehen, GET Genre-/Ordner-/Aufnahmenquery mit kanonischen IDs, Release-Jahrfilter `currentYear-4..currentYear`, serverseitige Reihenfolge und Trefferzahlen, kein zusätzlicher Suchindex. Erste Home-Seite je Reihe beim Fokussieren (kein Polling); fehlende Sitzung meldet der Server, OK führt zur vorhandenen Login-Tastatur. Native lokale Neuaufnahme-Reihe bleibt auch ohne Core erreichbar. Ordner-/Genre-Auswahl öffnet eine eigene paginierte Sammlung; exakte native Aufnahme-ID führt zur lokalen Detailansicht und Wiedergabe. Fortsetzen setzt auf ausdrückliche Auswahl den nativen VDR-Resume-Eintrag, Von vorn löscht ihn wie der VDR. Cross-Client-Rückschreiben der Wiedergabeposition, vollständige Serien-Hierarchie samt Overrides, zufällige Empfehlungsrails, kanonische Core-Poster-Overrides, Echtzeit-SSE und Geräteabnahme bleiben offen. Root-Ordner-Projektion zählt eingebettete Einzelaufnahmen dedupliziert auf der ersten Seite; vollständige Parität bei späteren Seiten mit doppelten Leaf-Identitäten ist noch nicht geprüft. Die neuen HTTP-Tests verwenden synthetische Fixtures und belegen den Client-Vertrag, nicht den Live-Server.
