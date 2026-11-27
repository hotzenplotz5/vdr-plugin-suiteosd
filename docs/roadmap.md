# Suite-OSD – Roadmap

Stand: 2026-11-26 · Basis: separates `hotzenplotz5/vdr-plugin-suiteosd` Repository, `main`. `vaapivideo` wird nicht geforkt.

| Slice | Umsetzung | Beobachtbarer Abschluss |
| --- | --- | --- |
| **OSD.0 – Dokumentation** | Ziel, Eigentümer, Addon-Arten, Hardware- und Autorisierungsgrenzen | README, Architektur, UI-Vertrag, Roadmap im Repo |
| **OSD.1 – Minimales VDR-Plugin** | VDR-Plugin-Metadaten, Einstieg ins Hauptmenü, eigene native OSD-Ansicht, Standard-Makefile | Gegen installierte VDR-Header gebaut; Menü auf realem VDR sichtbar, Back funktioniert |
| **OSD.2 – Suite-Skin** | VDR-`cSkin`-Implementierung für Standardmenüs, Status-/Wiedergabeanzeige, Theme; Fallback auf einfache OSD-Flächen | Keine Änderungen am Ausgabeplugin, korrekte Standardmenüs/TV-Rückkehr |
| **OSD.3 – Native VDR-Integration** | Lokale VDR-Kernmenüs und `epgsearch`-Suchtimer direkt anbinden, ohne Public API v1; echte Navigations- und Datenparität prüfen | EPG, Timer, Aufnahmen, Suchtimer auf realem VDR funktional, ohne HTTP-Umweg |
| **OSD.4 – Plugin-Handoff** | Teletext- und HbbTV-Start-/Fokusrückgabe an die vorhandenen Plugins | Teletext-Farbtasten, HbbTV-Browser+Video, Exit und Live-TV stabil |
| **OSD.5 – Medien** | Media Tools und Image-Galerie über bestehende lokale Suite-Dienste; Datei-/Stream-Wiedergabe über den vorhandenen VDR-/Ausgabeplugin-Player | Importstatus, Bildanzeige, Video-Wiedergabe, Audio/Seek und TV-Rückkehr |
| **OSD.6 – Addon-Erweiterbarkeit** | Einheitlicher UI-Katalog auf Basis tatsächlich vorhandener Capability-/Plugin-Informationen statt eigener Business-Logik | Neue Suite-Funktion kann mit dokumentiertem Präsentationsadapter hinzugefügt werden |
| **OSD.7 – Paketierung & Abnahme** | Debian-/Ubuntu-Plugin-Paket, Source/ABI/Installer-/Real-VDR-Test | Install/Upgrade/Remove auf yaVDR; vaapivideo und alternativer OSD-Provider geprüft |

## Reihenfolge und Grenzen

- OSD.0/OSD.1 liegen im Quellcode vor; OSD.2 (Skin), OSD.3 (lokale Integration) und Teile der späteren Integration sind begonnen. Alle Phasen benötigen weiterhin ihre jeweils beschriebene echte Geräte-/Funktionsabnahme.
- `vdr-plugin-suiteosd` braucht keine veränderte `vaapivideo`-Quelle. Es soll auch mit anderen VDR-OSD-Providern funktionieren.
- Bei OSD.2 die verfügbaren OSD-Flächen/TrueColor-Fähigkeiten am echten Gerät nachweisen.
- **Rendering-Entscheidung (25.11.2026):** Native VDR-OSD-API und der vorhandene Ausgabeprovider bleiben Standard **und dauerhafter Rückfallweg**. Ein möglicher CEF-/GPU-Textur-/DMA-BUF-Renderer ist lediglich eine spätere, optionale Forschungs-/Entwicklungsalternative; kein nachgewiesener `vaapivideo`-Vertrag, keine Pflichtpatches und kein aktuell eingeplanter Implementierungsslice. Erst nach Machbarkeit, Messungen und gesonderter Entscheidung aufnehmen. Details: [Architektur – Renderwege](architecture.md#renderwege-nativer-standard-und-möglicher-gpu-pfad-25112026).
- HbbTV bleibt in seinen bisherigen drei Komponenten. Die in VDR-Suite am 23.11.2026 zurückgestellte HbbTV-Extraktion wird nicht neu gestartet.
- Keine neuen Git-Branches, Berechtigungsgates, produktiven API-Routen oder Addon-C++-Loader als Nebeneffekt.
- Kein `make install` auf dem produktiven VDR ohne verifizierten Build, ABI- und Installationsweg.

## Erste Akzeptanz und offener Hardware-Test

**OSD.1** ist als native Home-Ansicht im Quellcode vorhanden und gegen VDR 2.8.3 und 2.8.4 per CI gebaut. **Vollendet** ist die Phase erst nach Bedienprobe am echten VDR mit Fernbedienung und Ausgabeplugin. Bis dahin keine Installationszusage.

Als spätere Regressionen gelten: Rückkehr zum aktiven Kanal, Wechsel zwischen Teletext und HbbTV, VDR-Plugin-Konflikte, OSD-Auflösung, fehlendes Backend, Sessionwechsel, lange Media-Tools-Aufträge und Medienwiedergabe.

## Vergleich zur bestehenden Oberfläche

Das vorhandene VDR-Suite-Webfrontend ist der funktionale Mindestumfang. Bei jedem TV-Feature die Eingaben, Gesamtanzahl/Sortierung, Titel/Metadaten, Paging, Errors, Berechtigungen und reale Aktionen gegenüber dem Webfrontend dokumentieren und testen. Kodi/Plex sind mögliche 10-Foot-UX-Referenzen; eine aktuelle Gegenüberstellung wurde noch nicht erhoben.

## Fortschritt – Native Aufnahmen 2.0

Der Kachelbrowser (Ordner/Genre/Alle), die Tastenbelegung, TVScraper-Genres, direkte Aufnahme-Wiedergabe und die optionale Ausgabeplugin-Videovoransicht sind auf `main` implementiert. CI-Proben gelten nur für Build und Headless-Modell; echte TV-Anzeige, Aufnahmelöschung und Video-Reset müssen am realen Gerät mit Ausgabeplugin getestet werden. Keine neue API v1 und kein `vaapivideo`-Fork.

## Stand – gemeinsame Einstellungen (0.7.0)

Native sechs Bereiche umfassende Einstellungen-Oberfläche ergänzt die Suite-Sidebar; VDR/Plugins verwenden die vorhandenen nativen Setup-Menüs, und das kleine Live-TV-Fenster besitzt eine gemeinsame persistent gespeicherte VDR-Plugin-Option. Suite-Core-/Agent-/Benutzer-Einstellungen bleiben bis zu einem verifizierten lokalen Besitzervertrag im Webfrontend editierbar. Eigene TV-Editoren dafür sind **noch nicht umgesetzt**. Automatische Build-/Navigationsprüfungen ersetzen nicht die spätere echte TV-Abnahme.

## Fortschritt – authentifizierte Suite-Core-Verwaltung (0.8.0)

Die VDR-Suite-Einstellungsseite bietet nun eine echte Human-Browser-Session mit TV-Fernbedienung und erste Backend-/Media-Tools-/Konto-/Device-Mutationen über bestehende revisions- und CSRF-geschützte Core-Endpunkte. Headless HTTP-/Navigationstests und VDR-2.8-CI sind der Source-Nachweis; ein echter TV- und Servermutationstest sowie umfassende Rechte-/Pairing-Editoren fehlen. Keine neue Berechtigungsregel, Core-Datenbank oder Public-v1-Route wurde eingeführt.

## Fortschritt – Suite-Skin und Aufnahme-Info (0.9.0, 24.11.2026)

- `suite_skin.c` implementiert einen optional auswählbaren VDR-`cSkin` für Kanalinfo (OK bei Live-TV) und Replay-Fortschritt (OK während einer Aufnahme), jeweils mit lokaler TVScraper-Fanart, Metadaten und Fortschritt. Er ersetzt kein Ausgabeplugin und wird nicht stillschweigend zur aktiven VDR-Skin erklärt.
- Suite-Home und Aufnahmebrowser öffnen per Info eine großflächige, native Detailseite im freigegebenen Webfrontend-Stil mit Poster, Fanart, Episodendaten, Genres, Laufzeit, Bewertung, Datum und Synopsis. OK/Rot startet dort die VDR-Wiedergabe.
- Über `cReplayControl::GetInfo()` liefern **aus Suite-OSD gestartete Wiedergaben** bei Info diese native Detailansicht. Wiedergabesteuerung, Schnittfunktionen und Player bleiben VDR-eigen. Replay aus fremden Menüeinträgen nutzt weiterhin deren normale VDR-Infoansicht.
- Die verbleibenden `cSkin`-Bereiche (vollständige native Menüs, Lautstärke, Audio und Meldungen) verwenden einstweilen eine vorhandene Skin; OSD.2 ist deshalb noch nicht vollständig. Kein TV-/Remote-Test erfolgt. Quellnachweis: [CI VDR 2.8.3/2.8.4 für Commit 073950e](https://github.com/hotzenplotz5/vdr-plugin-suiteosd/actions/runs/45481258523).

## OSD.2.1 – Native VDR-Standardmenüs (0.9.1)

`suite_menu.c` implementiert `cSkinDisplayMenu` als optisch einheitlichen VDR-Suite-Zwei-Spalten-Renderer. VDR liefert weiterhin die tatsächlichen Einträge, Auswahl, Farbtasten und Aktionen; die Skin übernimmt Darstellung und scrollbarige Texte. Typisierte VDR-Events, Timer, Sender und Aufnahmen bekommen nur bei gültigen VDR-Daten einen Informationsbereich mit optionaler lokaler TVScraper-Fanart. Keine zweite Datenhaltung, API-v1-Abfrage oder neue Menülogik. Der vorherige `DisplayMenu()`-Fallback wird nur bei fehlender TrueColor-OSD-Fläche verwendet. VDR-/Plugin-Menüs mit eigener OSD-Zeichnung werden nicht automatisch umgestaltet.

**Abnahme ausstehend:** Bild- und Fernbedienungstest auf yaVDR, insbesondere editierbare Setup-Tabs, lange EPG-/Aufnahmetexte, Seitenwechsel, Plugin-Menüs, alternative OSD-Provider und korrekte Rückkehr zum TV. Lautstärke, Audiospuren und Statusmeldungen sind weitere separate OSD.2-Schritte.

## OSD.2.2 – Lautstärke, Tonspuren und Meldungen (0.9.2, 25.11.2026)

Der native `suite_skin.c`-Renderer zeichnet jetzt auch VDR-`cSkinDisplayVolume`, `cSkinDisplayTracks` und `cSkinDisplayMessage`: kompakte dunkle Overlays für Lautstärke/Stummschaltung, Audio-/Spurwahl (mit maximal fünf gleichzeitig sichtbaren Spuren) und nach Status/Info/Warnung/Fehler differenzierte Meldungen. Auswahl, Lautstärke, Meldungstext und Einblenddauer bleiben bei VDR bzw. den vorhandenen Plugins; die neue Darstellung fügt keine zusätzliche Menü- oder Fernbedienungslogik hinzu. Wenn eine TrueColor-Fläche für diese Overlays fehlt, wird die vorhandene Skin verwendet. Kein `vaapivideo`-Fork, CEF oder DMA-BUF.

**Offen:** Echte Bedien- und Anzeigeabnahme mit `vaapivideo` und alternativen OSD-Providern, Audio-Track-Längen, Skalierung, Overlay-Überlagerung und zeitgerechtes Ausblenden. Builds und bestehende Headless-Tests sind dafür lediglich Quellcode-Nachweise; ein kompletter OSD.2-Abschluss erfordert die spätere Geräteabnahme.


## Weiterentwicklung nach dem ersten Hardwaretest (25.11.2026)

0.9.5-dev: lokaler Debian-Build und Installation bestätigt; VDR 2.8.3 / VAAPIVideo 1.10.1. Auf Fotos nachgewiesen: Home, Kanalinfo mit Logo/Fanart, EPG, Genre-Normalisierung, native skalierte Live-TV-Vorschau, Aufnahmen und Replay. Nicht abgenommen: vollständige Bedien-/Aktionsparität und Langzeitstabilität. Automatischen Attach nicht wieder einführen.

Die nächste Produktphase ist nicht nur Design. Der Webfrontend-Mindestumfang und die offenen Funktionslücken stehen in [der Funktionsmatrix](webfrontend-parity.md).

| Slice | Ziel |
| --- | --- |
| OSD.8 | Gemeinsame Detaildaten und einheitliche interaktive Details |
| OSD.9 | Mitwirkende/Porträts, Fernbedienungsnavigation, Auswahl |
| OSD.10 | Vorhandene Aufnahme- und EPG-Personensuche, Ergebnisaktionen |
| OSD.11 | Globale Suche mit vorhandener Suite-Suchsemantik |
| OSD.12 | Filmreihen, Serien, Staffeln, Episoden, Zuordnung |
| OSD.13 | Aufnahmebearbeitung, Verschieben, Schnittmarken |
| OSD.14 | Timer/Suchtimer: vollständige Funktions- und Bedienparität |
| OSD.15 | Addons, Bilder und Media Tools |
| OSD.16 | Design, Icons, Artwork, Kontrast und TV-Bedienkomfort |
| OSD.17 | Vollständige Paritäts-/Hardwareabnahme |

OSD.8/9 erster Slice: gemeinsamer Detaildaten-Adapter, native scrollbare Mitwirkenden-Texte, interaktive Mitwirkenden-Seite aus Suite-Aufnahmedetails und Suite-Replay-Info. **Personenauswahl startet noch keine Suche**; das ist OSD.10. Der passive VDR-Skin kann selbst keine neuen Fernbedienungsaktionen besitzen. Weitere native Detail-Einstiege benötigen ein interaktives Menü mit belegter VDR-Ownership. Kein neuer Suchindex.


## OSD.TV – Korrekturroadmap nach der TV-Abnahme vom 26.11.2026

**Priorität vor weiteren neuen Features.** Referenz: installierter Kandidat 0.9.16-dev, Produktcode-Commit [f0e97dc](https://github.com/hotzenplotz5/vdr-plugin-suiteosd/commit/f0e97dc27107aca2a6b7ab524cc40dae86a23131), [CI #219](https://github.com/hotzenplotz5/vdr-plugin-suiteosd/actions/runs/45891450581) erfolgreich (14 Jobs, VDR 2.8.3/2.8.4). Hardwarefeedback: yaVDR, VDR 2.8.3, VAAPIVideo. Das grundsätzliche Cinematic-Layout gefällt dem Maintainer bereits; beibehalten und verbessern, nicht neu erfinden.

Die folgenden Slices konkretisieren die noch offenen Arbeiten aus OSD.8–11, OSD.16 und OSD.17. Status ist das beobachtete Feedback, kein pauschaler Funktionsabschluss. Die Reihenfolge ist die vom Maintainer übernommene Priorisierung der Korrekturen.

| Slice / Priorität | Befund und Ziel | Beobachtbarer Abschluss |
| --- | --- | --- |
| **OSD.TV.1 – Suche (P1, offen)** | Bruce Willis: Webfrontend 11 Aufnahmen, OSD keine (ursprünglich Transportfehler). Im neueren Kandidaten Ewan McGregor mit HTTP 403 trotz Pairing. Tatsächliche Grants für das konkrete Gerät und Backend, exakten Core-Fehler und installierten API-Stand prüfen. Bestehende Rechte `recordings.view` / `epg.view` verwenden; keine zusätzliche Regel. Alle Suchbegriffe, Personen, Filme, Serien und EPG über den gemeinsamen Index; fehlende Personenprojektion und sichere native Aufnahme-ID-Zuordnung mit Core-Owner klären. | Dieselben Eingaben und Backend-Kontexte liefern fachlich gleiche Treffer, Counts, Sortierung und Paging wie Web; echte leere Ergebnisse von Fehlern unterscheiden. Personenliste, Details und native Wiedergabe funktionieren, auch mit Umlauten und anderen Personen. Keine Titelheuristik oder Sonderlösung für einen Schauspieler. |
| **OSD.TV.2 – Home/Public API v1 und SSE (P2, offen)** | `Zuletzt angesehen` meldet fehlenden Public-v1-Lesevertrag. `Filme der letzten 5 Jahre` benötigt den bereits im Core aufbereiteten, nach Erscheinungsdatum gefilterten Bestand statt eines lokalen Vollscans. Übrige Suite-eigene Rails (Historie, Empfehlungen usw.) auf tatsächliche authentifizierte Public-v1-Owner migrieren; lokale VDR-Funktionen bleiben nativ. Bestehender Geräte-SSE-Kanal für recordings ist im Plugin angebunden, reale automatische Aktualisierung und notwendige Historien-/Metadaten-Domänen sind nicht vollständig verifiziert. Fehlende Verträge im Hauptrepo konkret belegen und mit dem zuständigen Owner ergänzen; keine erfundenen Routen oder Berechtigungen. | Home lädt beim Öffnen, nach Anmeldung und Reconnect automatisch; zulässige Kacheln bleiben bei Refresh sichtbar, Fokus/Scrollposition bleiben erhalten. Relevante Aufnahme-/Metadaten-/Historienänderungen aktualisieren die betroffenen Reihen per bestehendem SSE-/Readback-Vertrag, ohne OK, manuelles Neuladen oder Status-Polling. Historie bleibt im vorgesehenen Actor-/Backend-Kontext; keine ungeklärte Vermischung von Geräte- und Benutzerhistorie. |
| **OSD.TV.3 – Performance (P3, offen)** | Erstöffnung und Navigation teilweise langsam. Filmreihe erscheint im TV-Test erst nach sehr langer Wartezeit; lokaler TVScraper-Fallback ist nur Übergang, keine dauerhafte Lösung für Core-Projektionen. Wiederholte Metadaten-/Artwork-Abfragen, synchrone HTTP-Aufrufe und unnötige Redraws untersuchen. | Kalt-/Warmstart, Zeit bis zu sichtbaren Rails und D-Pad-Latenz vor/nach Änderung messen; kein blockierendes Netzwerk im UI-Pfad, keine unnötige Vollbibliotheksabfrage pro Öffnen/Fokuswechsel. Begrenzte Caches, gezielte Invalidierung und nur betroffene Flächen neu zeichnen. Keine unbelegten Laufzeitversprechen. |
| **OSD.TV.4 – Vollbild (P4, offen)** | Schwarzer Außenrand im zuletzt gezeigten Menü ohne Live-TV; Menü soll die verfügbare Bildschirmfläche vollständig nutzen. Tatsächliche OSD-Geometrie, Provider und mögliche Video-/OSD-Ränder unterscheiden. | Menü füllt die nutzbare TV-Fläche; keine abgeschnittenen Inhalte, kein künstlicher schwarzer Rahmen, native Video-Skalierung und Rückkehr unverändert korrekt. Keine ungefragten globalen OSD-/Host-Konfigurationsänderungen. |
| **OSD.TV.5 – Mockup (P5, offen)** | Vor größerer visueller Änderung ein Mockup des tatsächlich vorhandenen Menüs vorlegen: bewährte Anordnung, größere nutzbare Fläche, dunklerer Grundton, stärkere Kachelrahmen. | Maintainer bestätigt die visuelle Vorlage. Kein frei erfundenes neues Layout und keine Behauptung, ein Mockup sei bereits umgesetzt. |
| **OSD.TV.6 – Cinematic-Design (P6, offen)** | Nach Mockup: dunkleres, weniger dominantes Blau; Kachelrahmen ca. 1–2 sichtbare Pixel stärker (auflösungsabhängig); Licht von oben links, Schatten nur nach unten rechts; dezente Relief-/Lichtakzente für räumliche Tiefe statt flacher 2D-Wirkung. Suche, Einstellungen, Aufnahmen und Details konsistent gestalten. | Reale TV-Fotos zeigen die bestätigte Vorlage, Lesbarkeit, klare Fokusmarkierung, gleichmäßige Kachelgrößen, keine überladenen Effekte oder zusätzlichen Menüs; Performance regressionsfrei. |
| **OSD.TV.7 – Metadaten (P7, teilweise)** | Technische Importnamen wie `DE - Rom (2005) (US)` durch echte Metadatentitel ersetzen, Serienname, Staffel/Episode und Episodentitel sinnvoll anordnen. Abgeschnittene Titel, fehlende Poster und inkonsistente Details in Home, Suche, Browser und Sammlungen prüfen. Vorhandene Metadatenquelle verwenden, Originalname nur als Fallback, Pfade und IDs unverändert. | Reale Vergleichstitel aus Web und OSD, konsistente Serien-/Filmdarstellung, sinnvolle Textumbrüche und Artwork-Fallback; exakte Identität bei Öffnen und Wiedergabe. |
| **OSD.TV.8 – Live-TV (P8, verbessert / Regression offen)** | Vorschau zuletzt wieder funktionsfähig, frühere Ausfälle bei Menüwechseln und Probleme mit schwarzen Balken, Video-/Rahmengeometrie und Senderlogo. Keine Verwechslung mit dem separaten Detached-Start. | Vorschau bleibt/reagiert zuverlässig bei Rückkehr zu Home und Aufnahmen; Logo und Rahmen sauber, keine Überlagerung der Zeit/Progressbar, echte Provider-/Senderbalken von Layoutfehlern unterscheiden; Vollbild-TV nach Verlassen. |
| **OSD.TV.9 – Pairing (P9, verbessert / Regression offen)** | Erfolgreiche Kopplung am TV nachgewiesen; vorher widersprüchliche Meldungen bei erneutem Statuscheck. Eindeutig „Erfolgreich gekoppelt“, keine verbleibenden Freigabeaufforderungen nach Abschluss. | Erneutes Öffnen, Verbindung prüfen und Neustart behalten gespeicherte Geräteanmeldung; Widerruf und reale Fehler korrekt melden. Keine neue Kopplungsanfrage oder Loginmaske bei gültiger Anmeldung. |

### Durchführung und Abnahme

- Auf aktuellem `main` von `hotzenplotz5/vdr-plugin-suiteosd` arbeiten. Vor Entwicklung vollständige aktuelle AGENTS.md und relevante Fachverträge lesen. Für erforderliche Core-Arbeiten `hotzenplotz5/vdr-suite` und dessen AGENTS.md/aktuelle Owner prüfen; keine eigenmächtige Branch-/API-/Security-Nebenarbeit.
- OSD.TV.1–3 zuerst zusammenhängend bearbeiten; unabhängige Fehler ohne erneute Beauftragung weiterverfolgen. Mockup als ausdrücklich verlangte Designvorlage vor OSD.TV.6. Bestehende native Funktionen erhalten. Kein zusätzlicher Suchindex, keine parallele Authentifizierung, keine neuen Gates, kein Polling-Ersatz für SSE.
- Headless-/Contract-Tests, echte Integration, VDR-Buildmatrix, CI, Paket und Hardwareabnahme getrennt nachweisen. Vergleich mit Webfrontend und konkrete Runtime-Messwerte dokumentieren. CI allein ist keine TV-Abnahme.
- Ein gemeinsames Update nach den ausführbaren Korrekturen vorbereiten; keinen produktiven VDR automatisch installieren oder neu starten. Lokale Änderungen im yaVDR-Checkout (insbesondere `suite_skin.c`) nicht überschreiben. Nur verifizierten Paketweg verwenden.
- **Außerhalb dieser Korrekturroadmap:** kls' mögliche generische Top-Level-Plugin-Schnittstelle im VDR-Kern und der dauerhafte Attached-Start von VAAPIVideo. Kein eigener VDR-/Ausgabeplugin-Patch und keine automatische Attach-/Keymacro-/MainMenuHooks-Änderung im Rahmen dieser Slices.

## OSD.PERF – Home Performance (27.11.2026)

**Nächstes eigenständiges Arbeitspaket; konkretisiert OSD.TV.3.** Geplant, noch nicht umgesetzt. Andere offene Roadmap-Slices bleiben erhalten. Ausgangspunkt: Produktcode [0.9.22-dev / 254ca5e](https://github.com/hotzenplotz5/vdr-plugin-suiteosd/commit/254ca5e0fab2f20727f236a28cd04f60f4c0e7c4), [CI #240](https://github.com/hotzenplotz5/vdr-plugin-suiteosd/actions/runs/45928351597) erfolgreich. Vor Umsetzung aktuellen main und real installierte Version unterscheiden.

### Ziel und nachgewiesene Ursachen

Der Nutzer erwartet bereits vorbereitete Cover und Metadaten beim Öffnen, flüssiges Scrollen und keinen sichtbaren Neuaufbau vorhandener Kacheln. Das bestätigte Vollbild-Design (drei Rails, sechs Poster bei eingeklappter Sidebar, Live-TV-Karussell, Detailpanel, keine Aufnahmedatumszeile auf Postern) bleibt unverändert.

- `MainMenuAction()` erzeugt je Öffnung ein neues `cSuiteHome`; Rail-Projektionen, lokale Poster, öffentliche Poster und Metadaten-Caches sind Mitglieder dieses Objekts. Das Schließen verliert den warmen Arbeitsbestand. Bereits umgesetzt sind LRU, SSE-Readback ohne pauschale Bildlöschung und vier parallele Poster-Worker innerhalb der Menüsitzung.
- `programmePoster()` und `recordingPoster()` führen bei Cache-Miss lokale Scraper-/Bildoperationen im Renderpfad aus; lokale Dateien sind nicht gleichbedeutend mit bereits dekodierten Kacheln.
- Eine öffentliche Aufnahme kann Detail-, Artwork-Manifest- und Binär-GET benötigen; die bestehende Public-v1-Artwork-Route liefert Originalbytes, nicht die optionale Home-Preview-Variante des Webfrontends. HTTP-Client erzeugt je Request einen neuen CURL-Handle. Die relative Bedeutung dieser Kosten muss gemessen werden.
- Quellen: [SuiteOSD](https://github.com/hotzenplotz5/vdr-plugin-suiteosd/blob/254ca5e0fab2f20727f236a28cd04f60f4c0e7c4/suiteosd.c), [HTTP-Client](https://github.com/hotzenplotz5/vdr-plugin-suiteosd/blob/254ca5e0fab2f20727f236a28cd04f60f4c0e7c4/core_admin_client.c), [Artwork-Vertrag](https://github.com/hotzenplotz5/vdr-suite/blob/main/api/public-recording-artwork.md), [Metadatenvertrag](https://github.com/hotzenplotz5/vdr-suite/blob/main/api/public-recording-metadata-detail.md).

| Slice | Geplante Umsetzung | Beobachtbare Abnahme |
| --- | --- | --- |
| OSD.PERF.0 | Messbasis und Owner: monotone Zeitmessung für Menüöffnung, ersten Frame, vollständige sichtbare Rails, D-Pad, HTTP, Scraper, Decode/Resize, Pixmap/Flush; Cache-Hits/Misses, Bytes, Request-Zahl, RSS und Worker-Laufzeit. Kalt-/Warmstart getrennt, bestehende Tests/Diagnostik nutzen. | Reproduzierbares Vorher-Profil oder explizit fehlende Hardware-Messwerte; keine erfundenen Messungen. |
| OSD.PERF.1 | Menüunabhängiger begrenzter RAM-Arbeitscache im bestehenden Plugin-Lebenszyklus, getrennt von OSD-/Pixmap-Ownership. Rail-Projektionen, Identität, zulässige Metadaten und dekodierte Poster wiederverwenden; stabile Schlüssel nach Profil/Backend/Actor/Ressource, Artwork-Identität, Größe und Version. Bestehenden LRU erhalten, Speicherbudget anhand tatsächlicher Pixelkosten dimensionieren. | Menü schließen/öffnen und unverändert zurückscrollen: warme zulässige Kacheln sofort verfügbar, keine wiederholten Binärdownloads oder Dekodierung bei Cache-Hit. Keine alten OSD-Handles/Threads oder kontextfremden Daten. |
| OSD.PERF.2 | Hintergrundvorbereitung im vorhandenen Plugin-Lebenszyklus auch bei geschlossenem Menü; zuerst Startansicht, dann benachbarte Rails und horizontale Kacheln. Vorhandene SSE-/native Änderungsmechanismen und initialen/reconnect Readback verwenden; sichtbare Misses priorisieren, in-flight deduplizieren, begrenzte Worker. Scraper-/Datei-/Decode-Arbeit nur außerhalb des Renderpfads bzw. VDR-Locks soweit ABI/Threadvertrag erlaubt; OSD-Zeichnen beim Owner belassen. | Nach vorbereiteter TV-Phase ist Home warm, keine Netz-/Datei-/Decode-Wartezeit auf der Fernbedienung, keine TV-/Replay-Störung. Sauberer Shutdown, keine unkontrollierte Vorladung der gesamten Bibliothek. |
| OSD.PERF.3 | Kachelgrößen statt Originale: vorhandene Home-Preview-/Thumbnail-Owner im Hauptrepo und lokale TVScraper-Bildvarianten prüfen. Kleine geeignete Bilder wiederverwenden; bestehende Public-v1-Routen/Grants erhalten. Wenn der benötigte Vertrag fehlt, konkret belegen und die notwendige Core-Änderung gesondert mit Maintainer abstimmen. | Weniger übertragene/decodierte Pixel und Bytes, gleiche Cover-Auswahl und Qualität auf TV, kein zweiter Scraper oder Thumbnail-Generator. |
| OSD.PERF.4 | Neustartübergreifender Thumbnail-Arbeitscache: vorhandenen Cache-Owner, Dateiformat, Budget, Versionierung und erlaubte private Cache-Semantik prüfen; bevorzugt wiederverwenden. Nur bei nachgewiesenem Bedarf und nach ausdrücklicher Entscheidung neue Persistenz umsetzen. Keine dauerhafte Speicherung von no-store-Metadaten oder unzulässige Verlängerung von Artwork-Freigaben. | Neustart mit vorhandenem zulässigem Thumbnail-Bestand schneller; korrekte Ersetzung bei Bildänderung, Kontextwechsel/Widerruf, begrenzter Platzbedarf. |
| OSD.PERF.5 | Verbleibende HTTP-Kosten messen; Verbindungen im bestehenden Client sicher wiederverwenden, unnötige Readbacks vermeiden. Kompakte Home-Projektion mit bereits vorhandenen Titel-/Episoden-/Artwork-Referenzen nur falls Messung den Bedarf bestätigt und bestehender Vertrag/Owner es zulässt; neue API-Verträge gesondert abstimmen. | Request-Zahl und Latenz sinken, identische fachliche Projektion, IDs, Sortierung, Paging, Grants und Fehlersemantik. |
| OSD.PERF.6 | Gezielte Render-Optimierung und Abschluss: Wiederverwendung dekodierter Bilder/Kompositionen, keine unnötigen Ganzflächen-Redraws, zusammengefasste Readbacks; vorher/nachher messen und vorhandene VDR-Buildmatrix/Package/TV prüfen. | Keine verschwindenden vorhandenen Cover bei unverändertem SSE-Reconnect, Scrollen oder Stillstand; flüssige Fernbedienung, nachweisbar schnellere Kalt-/Warmöffnung ohne Wiedergaberegression. |

### Mess- und Abnahmeziele

Orientierende Zielwerte, keine bereits erreichten Werte und keine zusätzlichen CI-Gates: warmes Home mit vorbereiteten sichtbaren Kacheln möglichst innerhalb 300 ms, sichtbare Fokusreaktion möglichst innerhalb 100 ms (Animationsdauer separat messen). Kaltstart bis erster Frame und vollständige sichtbare Rails, p50/p95 bei wiederholtem Öffnen und D-Pad, HTTP-Anzahl/Bytes, Dekodierungen und Peak-RSS vorher/nachher dokumentieren; tatsächliche Hardwaregrenzen und Abweichungen transparent ausweisen. Als anfänglichen Budgetrahmen 128–256 MiB für den gesamten Home-Arbeitscache prüfen, nicht pauschal je Cache reservieren; kleinere Systeme und 4K berücksichtigen.

Regressionsfälle: schneller Wechsel zwischen Rails, Rückkehr nach Schließen, VDR-Neustart, identischer Readback, tatsächliche Metadaten-/Coveränderung, fehlendes Artwork, lokale/entfernte Profile, temporärer Core-Ausfall, expliziter Widerruf, Auflösungswechsel, TV-/Replay-Handoff und Shutdown bei laufender Arbeit. Bei no-store-Projektionen und privaten Artwork-Caches vorhandene Verträge einhalten; nicht durch Disk-Cache oder lokale Pfade umgehen.

### Arbeitsgrenzen und Durchführung

Auf aktuellem main des SuiteOSD-Plugins beginnen; AGENTS.md und relevante Fachverträge vollständig lesen. Keine neuen Branches, Berechtigungen, Gates, Hintergrund-Systemdienste, zweite Datenbanken, VDR-Core-/Ausgabeplugin-Patches oder produktive Installation. OSD.PERF.0–2 zuerst zusammenhängend bearbeiten; weitere Slices anhand Messungen im genehmigten Scope fortführen. Eine in der Roadmap aufgeführte Prüfoption ist keine Freigabe für neue API-Routen oder persistente Architektur. Bei einem tatsächlich erforderlichen externen Owner-Wechsel zuerst dessen Regeln/Vertrag prüfen; nur die konkret abstimmungspflichtige Änderung zurückstellen und unabhängige Arbeiten weiterführen. Vorhandenes Design unverändert lassen. CI/Build/Contract/Paket/REAL-VDR getrennt dokumentieren. Roadmap-Fortschritt im selben Arbeitspaket aktualisieren.


### OSD.PERF – first native working-set slice (0.9.23-dev)

Source scope: native poster/title/logo cache owned by the plugin, not the short-lived OSD. Geometry changes and explicit refresh invalidate the native working set. Initial now/next cards and nearby native recordings are prepared from the existing VDR snapshot when the menu is closed; one worker reads/decodes copied paths outside VDR locks, with bounded queued work and main-thread cache publication. Visible misses are prioritized. VDR/TVScraper service calls retain their existing main-thread contract; the remaining service cost and actual idle cadence must be measured. No OSD handle crosses the worker boundary. The native poster pixel working-set target is 32 MiB; other Home raster caches still require combined-budget accounting. Protected Public-v1 rails/detail responses remain menu-owned pending a live context/SSE lifecycle change; do not persist no-store data. The existing private artwork and SSE contracts are unchanged. No API, permission, layout, system service or storage changes.

PERF.0 partial: HOME_PERF log on menu destruction records CPU-side first OSD flush, full draws, flush time/count, key-to-first-flush samples (not physical TV latency), native cache hits/misses, scraper call/time and completed native preparation count, and native artwork/logo allocation. Actual cold/warm TV measurements, full-rail readiness, HTTP/decode timings and peak RSS remain NOT_VERIFIED. PERF.1/2 partial (native working set only); PERF.3–6 remain open. Existing build matrix and focused tests need verification on the new exact code SHA; no package or real-TV acceptance claimed.

PERF.0/5 follow-up: process-wide HTTP request/body-byte/time/new-connection and decoder attempt/time deltas during menu lifetime, plus process peak RSS, are logged separately as HOME_PERF_PROCESS. Request-local curl options are reset after every request, while the individual client retains its connection pool; origin changes discard that pool. No response caching or API changes. Unchanged native poster pixels retain their existing cache entry. Runtime timings and full-rail readiness remain unverified; counters include other SuiteOSD activity in the same process and decode attempts include misses.

PERF.1/4/5 follow-up: private decoded Public-v1 artwork may now survive menu destruction within the actual successful image response's private max-age (capped at the documented 300 seconds). The 32 MiB RAM-only working set is keyed by exact server/device credential digest, backend, recording and pixel geometry; a fresh authorized manifest must confirm the opaque asset before reuse. No raw response, detail or manifest is retained across menus. Explicit denial/missing artwork removes the entry; expiry and geometry reset remove stale data. Non-image requests retain no-store, image requests follow their actual response cache policy. No persistent disk cache, API extension or background public rail owner. Headless tests cover scope/expiry/byte eviction and actual private HTTP policy; TV acceptance remains open.

PERF.2/6 follow-up: closed-menu native prewarm copies only first twelve channels plus the tuned event and selects six newest recording IDs (no full recording description copy in the periodic prime). Settled native focus metadata resolves under the existing VDR thread/lock contract and its fanart decodes on the same bounded preparation worker; the tuned programme is primed while closed. Native focus previews and geometry-only background/frame/glyph rasters survive menu destruction. The frame and composite working sets target 16 MiB each and focus preview count is bounded by output geometry; total process RSS remains a measurement, not an enforced process memory cap. The visible focus/animation design is unchanged. Still open: complete rail-readiness measurement, closed-menu authorized Public-v1 rails under no-store, remaining local logo/detail IO, and real TV timing/continuity.


#### PERF.0–6: bounded private artwork preparation and owner findings

- Initial EPG rail resolution and pixel-card coverage are logged separately from first flush. Missing artwork is a resolved outcome; the pixel count includes logo fallback. This is a software readback, not a measurement of physical TV scan-out. D-pad values are first-flush latency, not display latency.
- Channel logo file lookup/decode moved out of Home rendering. A one-shot, throttled closed-menu pass uses existing New/Movies Public-v1 projections, at most six cards each; the projections are discarded in the worker. Only private-cacheable decoded images are retained; a fresh authorized manifest is still required on display. No periodic HTTP polling, new API, or persistent database. Opening cancels further prewarm requests; an already-running request may complete. Shutdown joins the worker.
- Owner verification against vdr-suite e0169fcba59252fe251795465e607c8d1526bf2b: `api/public-recording-artwork.md` explicitly excludes the Web preview variant. Existing `ArtworkPreviewCache` (`core/media/src/ArtworkPreviewCache.cpp`, `services/artwork/README.md`) persists daemon-owned previews in /var/cache/vdr-suite/artwork-previews (1024 entries / 64 MiB). Reuse through an optional variant on the existing Public-v1 artwork endpoint requires an agreed owner contract, not direct filesystem access or a second generator. TVScraper GetPosterThumb (pinned upstream 2e63f40) uses different image-selection levels from the current GetScraperVideo::getImage; not substituted blindly. No new persistence is needed until owner reuse is evaluated.
- Public no-store rails/details remain menu-owned. Especially Continue/Recent, a device credential is not proof of unchanged playback account binding; the existing binding mutation and event contract do not establish such an identity proof for retained projections. A persistent live projection owner and/or compact projection contract still need owner analysis. Private artwork is not a substitute for that work.
- Raster working-set targets: native posters 32 MiB, private artwork 32 MiB, menu public posters up to 32 MiB, geometry frames and composites 16 MiB each, focus previews approximately 16 MiB, two screen backgrounds plus logos/glyphs/transient allocations. At 1080p backgrounds alone use about 15.8 MiB; at 4K about 63.3 MiB. This is not a strict combined process cap. Measure real RSS, CPU, playback and warm/cold latency before sizing further.
- Integration reference before this slice: CI #245 on 61544ef35b0aa7184eaf0553990744f49d59060c, all 14 jobs passed. Real TV measurements, package install verification and 300/100 ms targets remain open; no production installation performed.


### OSD.PERF – event-driven preparation and canonical recording data (0.9.24-dev)

Native VDR callbacks and state revisions replace scheduled Home priming; snapshots and initial background/frame preparation run off the menu-opening path. Recording/progress SSE subscriptions live with the plugin and reconnect on transport failure. Public recording metadata and artwork for details, browser cards, replay info and focus use the existing exact native-match proof, never a title-only match. The optional Home preview variant falls back to the original on an unsupported older Core (HTTP 400 with successful original read); actual cache headers remain authoritative. No changes to the Core branch, API grants, VDR or output plugin. Remaining: native genre grouping, legacy bootstrap/image_next readiness contracts, real TV timing, package acceptance and combined RSS measurement. Native EPG still uses TVScraper; main Core cannot supply fields only available on the unmerged API branch. No claim of complete polling removal outside Home or complete hardware acceptance.
