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

Eine Genre-Empfehlungsreihe wählt pro Home-Generation eine vorhandene Core-Genre-ID und behält sie beim Scrollen bei. Die Filme-/Neuaufnahme-/Empfehlungsreihe bietet bei weiteren Treffern einen navigierbaren Einstieg in die paginierte Sammlung. Keine erfundenen Empfehlungen oder Trefferzahlen. Der native Player übernimmt die explizit gewählte Position; Rückschreiben in den Core bleibt separat offen.

TV-Feedback 26.11.2026: OK auf einer Aufnahme im Suite-Browser führt nun in die interaktive Detailansicht, Rot/Play startet weiterhin direkt. Runter oder Blau aktiviert die sichtbare Cast-Leiste, Links/Rechts navigiert, OK öffnet die Personensuche. Die passive Standard-VDR-Info kann die Tasten nicht selbst übernehmen und ist weiterhin ein separater Einstieg. Keine Änderung von MainMenuHooks, Keymacros oder remote.conf.


## Nutzerfeedback: kein Browser-Login in der nativen Oberfläche

Home und Suche verlangen keine interaktive Human-Browsersitzung mehr. Weiterschauen verwendet bei nicht erreichbaren Core-Daten VDR-Resume, neue Aufnahmen sowie Genres und Ordner die bereits vorhandene native Browserprojektion. Keine neue Suchdatenbank, keine Änderung der HTTP-Authentifizierung. Native Ersatzdaten können vom Webfrontend abweichen; gemeinsame Suche, Core-Historie, zentrale Metadaten und Cover benötigen weiterhin eine transparente lokale Core-Anbindung. Nicht vorhandene gemeinsame Daten werden ausdrücklich benannt. Administrative Einstellungen bleiben getrennt.


## Native Core-Verbindung

Home und Suche nutzen ausschließlich den lokalen Unix-HTTP-Socket (`/run/vdr-suite/osd.sock`, alternativ `VDR_SUITE_LOCAL_OSD_SOCKET`). Kein TCP-Fallback, kein Browser-Cookie, keine Passwortabfrage. Die bisherige HTTP-Verbindung bleibt ausschließlich für die separat aufrufbaren Core-Administrationsfunktionen bestehen. Der Socket benötigt den passenden Core-Transport; ein AF_UNIX-Client-Vertragstest prüft echte Socket-Nutzung und die Abwesenheit von Browser-Credentials. Core-seitige Zuordnung und reale yaVDR-Abnahme sind getrennt zu belegen.

## Cinematic-Home 0.9.10 – freigegebene Gestaltung

| Aspekt | Umsetzung / Abnahme |
| --- | --- |
| Eingaben und Reihen | Bestehende native VDR-Snapshot-/Core-Rails, kein neuer Index; drei erste Reihen, sechs Karten, weitere Rails per Fernbedienung. |
| Geometrie | Gemeinsame 2:3-Kartenabmessungen, kein Zoom im Fokus; fest getrennte Live- und Fanart-Bereiche. Headless-Geometrie-/Navigationstest. |
| Metadaten | VDR-Eventidentität aus Sender, Event-ID und Startzeit; Aufnahmen über exakten nativen Dateinamen im vorhandenen lokalen Kontext. TVScraper-Fanart und optionale Mitwirkende. Keine erfundenen Daten; Bildfallback. |
| Aktionen und Paging | OK/Info/Zurück, Kanalwechsel, Timer, Wiedergabe und Sammlungen bleiben bei bisherigen Ownern. Kein Rechte-/API-Wechsel in diesem Design-Slice. |
| Performance | Begrenzte Poster-/Fokus-Caches, Fokusmetadaten erst nach kurzem Verweilen, Bilddekodierung außerhalb der Locks; lokale Minutenuhr statt Netzwerkpolling. |
| Abnahme | VDR 2.8.3/2.8.4 Build und Headless-Tests; reale Fanart-/Video-Komposition, Schriftlesbarkeit und Navigation auf yaVDR noch offen. |

Kodi/Plex bleiben UX-Referenzen für Poster-Navigation und Detailvorschau; eine aktuelle Produktgegenüberstellung ist nicht verifiziert. Maßgeblich ist das ausdrücklich bestätigte SuiteOSD-Mockup, nicht ein neues Fremdprodukt-Design.


## Cinematic-Renderer 0.9.11 – optische Korrektur

Die native Home-Oberfläche verwendet jetzt den produktiven C++-ARGB-Rasterpfad in `cinematic_home.h` für die Hintergrundbeleuchtung, weich auslaufende Rahmen, Posterkomposition und antialiaste Sidebar-Glyphen. Die fertigen Rasterflächen werden begrenzt gecacht. Die sechs 2:3-Karten in drei Reihen passen auch bei ausgeklappter Sidebar, ohne deren Navigation zu ändern. Der bisherige feste Footer entfällt; die Tastenfunktionen bleiben erhalten. Die Video-Geometrie des Aufnahmenbrowsers bleibt unverändert; Home nutzt die separate native Videoebene und die darunterliegende Fokus-Fanart. Alle bisherigen Sidebar-Aktionen bleiben sichtbar und erreichbar; die sieben beispielhaften Einträge im Mockup sind kein Auftrag, andere Funktionen zu entfernen. Raster-/Buildnachweise ersetzen nicht die echte VDR-/TV-Abnahme.

## TV-Praxistest – Korrekturen 0.9.12

| Beobachtung | Korrektur | Abnahme |
| --- | --- | --- |
| Importname `DE - Rom (2005) (US)` auf Home-Poster | TVScraper-Titel und Episode aus demselben lokalen Scraper-Aufruf wie das Poster; nativer Name nur bei fehlenden Metadaten. Exakte Aufnahme-ID unverändert. | Titel-/Fallback-Unit-Test; TV offen |
| Live-Endzeit abgeschnitten, Video unter Progressbar | Gemessene Zeittextbreite; getrennte 16:9-Video- und Textgeometrie, opake Chrome; Browser-Video unverändert. | Layout-Assertions; reale Provider-Geometrie offen |
| Grelles Blau, Poster ohne Artwork | Dunklere Hintergrundmischung, zurückhaltende Rahmen und neutraler Ersatz. | Rastertests; TV-Vergleich offen |
| Verzögerung beim ersten Öffnen | Zusammengesetzte Fokus-Fanart cachen, Fokus-Leerlauf zeichnet nur rechte Spalte, unveränderte Navigation löst keinen Full-Redraw aus. | Laufzeitmessung offen |
| Genehmigung ohne sichtbare Rückmeldung | Native Statusmeldung nach Menüpanels; bestehender Public-v1-Bootstrap-GET im dokumentierten Intervall nur bei offenem Pairing; ein asynchroner Auftrag, keine Wiederholung des Credential-POST. Speicherung und authentifizierter Readback über bestehende Verträge. | Loopback-Test pending/approved, Speicherfehler/Retry, Device-GET; reale TV-/Core-Kopplung offen |

Der Bootstrap-GET ist der vom [Core-Vertrag](https://github.com/hotzenplotz5/vdr-suite/blob/main/api/device-pairing.md) vorgesehene Statuspfad für anonyme Pairing-Token, kein allgemeines Suite-Statuspolling. Der OSD-Setup-Worker wird beim Schließen aufgeräumt; ein gerade laufender HTTP-Aufruf kann den Menüabschluss bis zu seinem Timeout verzögern. Keine Änderung an VDR-Core, Ausgabeplugin, Dateinamen, Berechtigungen oder Hostkonfiguration.

Bei Fokuswechsel ohne Scrollen der sichtbaren Reihen bleiben Hintergrund und Seitenleiste unangetastet; nur die betroffene(n) Reihe(n), die fokussierte Detailkarte und bei Übergang zu/von Live-TV dessen Fokusrahmen werden neu ausgegeben. Bei horizontaler Verschiebung wird der betreffende Streifen aus dem gecachten Hintergrund rekonstruiert. Kein vollständiger Screen-Redraw im normalen Leerlauf für neue Fanart. UTF-8-Textbegrenzung erfolgt an Zeichen- statt Byte-Grenzen; Titel und Mitwirkende erhalten lesbare Zeilen und Ellipsen, der Endzeittext eine gemessene vollständige Breite. Video-/Logo-Overlays bleiben bewusst unabhängig von der Ausgabeplugin-Implementierung; vom Sender oder Provider erzeugte Bildbalken können auf Hardware weiterhin vorhanden sein.

Das native Plugin-Setup öffnet für „Gerät koppeln“ eine eigene VDR-Unterseite. Die Suite-Skin zeichnet den vorhandenen Code prominent, Status und die tatsächlich vom Menü-Owner bearbeiteten Aktionen separat. Andere Skins erhalten die gleichen normalen VDR-Menüzeilen. Zurück führt zu den Plugin-Einstellungen; eine dort noch offene Anfrage wird weitergeprüft und erneutes Öffnen verwendet dieselbe laufende Anfrage. Kein neuer Authentifizierungs- oder Datenpfad. Die Bedienkette und der visuelle Dialog benötigen noch den realen TV-Test.

## Home-Scrollkorrektur 0.9.18 (27.11.2026)

Akzeptanz: fünf Karten je Rail, drei vollständig sichtbare Rails; Pfeil hoch/runter innerhalb des Ausschnitts ändert nur den Fokus, erst am Rand verschiebt sich der Ausschnitt weich in beide Richtungen. Horizontaler Fokus und Paging bleiben je Reihe erhalten; Live-TV, Sidebar und Kopfzeile bleiben fest. Das native OSD-Pixmap-Viewport bildet das weiche Scrollprinzip des Webfrontends nach (kein Browser, kein neuer Datenpfad). Eingaben, Counts, Titel, Metadaten, Grants und Aktionen unverändert. Die Bewegung ist auf 240 ms angelegt, ohne Datenanfragen pro Animationsframe; bei fehlender Pixmap-Funktion bleibt die statische Darstellung. Kodi/Plex sind nur UX-Referenzen; aktuelle Konkurrenzmessung nicht verifiziert. Headless-Navigation und Interpolation prüfbar; tatsächliche Bildrate, Lesbarkeit und schnelle Tastenwiederholung auf yaVDR bleiben hardwareseitig zu verifizieren.

## Home-Feedback 0.9.19

Web-Referenz: home-live-hero.js (neighborChannel, createNeighbor, renderHero); vorheriger/nächster Sender gemäß tatsächlicher VDR-Kanalliste, mit Wrap wie im Web. Native Auswahl per D-Pad und OK; das reale Video bleibt beim tatsächlich eingeschalteten Sender (kein paralleler Player). Sechs Poster bei eingeklappter Sidebar, drei sichtbare Rails einschließlich des ersten Live-Hero, beim Scrollen danach drei Poster-Rails. Public-v1 Recording-Detail und Artwork-Manifest/Binärroute werden für sichtbare öffentliche rec_-IDs automatisch auf einem Worker verwendet. Bevorzugtes Poster vor anderen vorhandenen Bildern, lokaler TVScraper nur mit nachgewiesener nativer Zuordnung. Begrenzter RAM-Cache und Invalidierung durch bestehendes Recording-SSE; keine zyklischen Netzwerkabfragen. Titel/IDs/Counts/Paging und bestehende Grants bleiben beim Core; keine neuen Rechte. Genre-/Ordnerkarten haben im aktuellen Public-v1-Vertrag keine eigenen Posterreferenzen und bleiben Textkarten. Kodi/Plex nicht erneut verifiziert. Reale TV-/Core-Abnahme offen.

Alle sichtbaren Rails werden beim Zeichnen bzw. Fokuswechsel unmittelbar in die bestehende Read-Queue eingereiht; gleichzeitig eintreffende Readbacks und Tastenereignisse führen ebenfalls zum sichtbaren Readback. Eine ausstehende Genre-Abhängigkeit unterbindet nicht das Laden anderer sichtbarer Rails. Die Web-Owner `renderGenreRail` und `renderFolderRail` zeigen Kategorien ebenfalls als Text-/Anzahlkarten; sie sind keine fehlenden Aufnahmebilder. Die Posterhöhe wurde für überlappungsfreie Überschriften leicht reduziert.

## Vollbildfläche 0.9.20

Das native Dashboard nutzt `cDevice::GetOsdSize` und `NewOsd(0,0)` bei gültiger Ausgabegröße; `cOsd::OsdWidth/Height/Left/Top` beschreiben dagegen die konfigurierten VDR-Menüränder. Die Video-Rechteck-Umrechnung nutzt den tatsächlich gewählten OSD-Ursprung. Fullscreen- und Preview-Geometrie für 720p/1080p/2160p sind headless abgedeckt; die echte Gerätekomposition beim Scrollen bleibt Hardwareabnahme. Keine Änderung an Daten, Metadaten, API, Grants oder Navigation; Konkurrenzvergleich unverändert.

## Home-Cache-Korrektur 0.9.20

Scrollen und Fokuswechsel nutzen vorhandene dekodierte Bilder. Lokale und öffentliche Poster, Rahmen, Logos, Kompositionen und Fokusbilder verdrängen einzelne LRU-Einträge statt vollständiger Caches. Poster-Arbeitsmengen sind an die tatsächliche Pixelgröße gebunden (bis 192, nominell 64 MiB pro Poster-Cache). SSE-hello/update führt weiterhin zu einem autoritativen Readback, hält aber die letzte Darstellung während der Aktualisierung. Öffentliche Artwork-IDs werden über das vorhandene Manifest verglichen; bei derselben ID innerhalb des dokumentierten 300-s-Cache-Fensters erfolgt kein neuer Binärabruf. Geänderte Bilddaten ersetzen die bisherigen erst nach erfolgreicher Dekodierung; Fehler behalten die bisherige zulässige Darstellung, explizite 401/403 entfernen den betroffenen Cacheinhalt. Autoritativ leeres Manifest löscht das betreffende Poster. Sichtbare Karten werden vor angrenzenden Rails vorbereitet. Keine neue API, kein Polling und keine Änderung an bestehenden Grants. LRU- und HTTP-Contract-Tests im vorhandenen CI; tatsächliche Ladezeit und Flackerfreiheit auf TV offen.

Identische Collection-Readbacks werden nach sichtbarer Projektion verglichen: weder ein unverändertes Ergebnis noch eine Änderung ausschließlich außerhalb des aktuellen Viewports löst einen kompletten Home-Redraw aus. Eine normale Aktualisierungsanfrage ersetzt keine bereits gültige Rail durch einen Ladezustand. Contract-Regressionen prüfen Identität, Titel und Fortschritt.
