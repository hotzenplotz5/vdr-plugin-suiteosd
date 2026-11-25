# Suite-OSD – Roadmap

Stand: 2026-11-24 · Basis: separates `hotzenplotz5/vdr-plugin-suiteosd` Repository, `main`. `vaapivideo` wird nicht geforkt.

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
