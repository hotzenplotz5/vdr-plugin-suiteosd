# Suite-OSD – Roadmap

Stand: 2026-11-23 · Basis: separates `hotzenplotz5/vdr-plugin-suiteosd` Repository, `main`. `vaapivideo` wird nicht geforkt.

| Slice | Umsetzung | Beobachtbarer Abschluss |
| --- | --- | --- |
| **OSD.0 – Dokumentation** | Ziel, Eigentümer, Addon-Arten, Hardware- und Autorisierungsgrenzen | README, Architektur, UI-Vertrag, Roadmap im Repo |
| **OSD.1 – Minimales VDR-Plugin** | VDR-Plugin-Metadaten, Einstieg ins Hauptmenü, eigene native OSD-Ansicht, Standard-Makefile | Gegen installierte VDR-Header gebaut; Menü auf realem VDR sichtbar, Back funktioniert |
| **OSD.2 – Suite-Skin** | VDR-`cSkin`-Implementierung für Standardmenüs, Status-/Wiedergabeanzeige, Theme; Fallback auf einfache OSD-Flächen | Keine Änderungen am Ausgabeplugin, korrekte Standardmenüs/TV-Rückkehr |
| **OSD.3 – Suite-Integration** | Existierende SuiteBridge-/Public-v1-Datenflüsse und lokale Identität prüfen, Live-Daten in ausgewählten nativen Menüs anzeigen; Push/Readback | EPG/Timer/Aufnahmen/Suchtimer mit Webfrontend-Parität auf realer Installation |
| **OSD.4 – Plugin-Handoff** | Teletext- und HbbTV-Start-/Fokusrückgabe an die vorhandenen Plugins | Teletext-Farbtasten, HbbTV-Browser+Video, Exit und Live-TV stabil |
| **OSD.5 – Medien** | Media Tools, Image-Galerie, Datei-/Stream-Wiedergabe, Player-Steuerung | Importstatus, Bildanzeige, Video-Wiedergabe, Audio/Seek und TV-Rückkehr |
| **OSD.6 – Addon-Erweiterbarkeit** | Einheitlicher UI-Katalog auf Basis tatsächlich vorhandener Capability-/Plugin-Informationen statt eigener Business-Logik | Neue Suite-Funktion kann mit dokumentiertem Präsentationsadapter hinzugefügt werden |
| **OSD.7 – Paketierung & Abnahme** | Debian-/Ubuntu-Plugin-Paket, Source/ABI/Installer-/Real-VDR-Test | Install/Upgrade/Remove auf yaVDR; vaapivideo und alternativer OSD-Provider geprüft |

## Reihenfolge und Grenzen

- Erst OSD.0, dann das kleinste echte Plugin-Skelett OSD.1. Spätere Slices sind **geplant**, nicht implementiert.
- `vdr-plugin-suiteosd` braucht keine veränderte `vaapivideo`-Quelle. Es soll auch mit anderen VDR-OSD-Providern funktionieren.
- Bei OSD.2 die verfügbaren OSD-Flächen/TrueColor-Fähigkeiten am echten Gerät nachweisen.
- HbbTV bleibt in seinen bisherigen drei Komponenten. Die in VDR-Suite am 23.11.2026 zurückgestellte HbbTV-Extraktion wird nicht neu gestartet.
- Keine neuen Git-Branches, Berechtigungsgates, produktiven API-Routen oder Addon-C++-Loader als Nebeneffekt.
- Kein `make install` auf dem produktiven VDR ohne verifizierten Build, ABI- und Installationsweg.

## Erste Akzeptanz und offener Hardware-Test

**OSD.1** soll mit dem kleinsten VDR-Menüeintrag begonnen werden. Die erste Quellcode-Version darf als `unverified scaffold` vorliegen; **vollendet** ist OSD.1 erst nach echten VDR-Headern, fehlerfreiem Build und Bedienprobe mit der Fernbedienung. Bis dahin keine Installationszusage.

Als spätere Regressionen gelten: Rückkehr zum aktiven Kanal, Wechsel zwischen Teletext und HbbTV, VDR-Plugin-Konflikte, OSD-Auflösung, fehlendes Backend, Sessionwechsel, lange Media-Tools-Aufträge und Medienwiedergabe.

## Vergleich zur bestehenden Oberfläche

Das vorhandene VDR-Suite-Webfrontend ist der funktionale Mindestumfang. Bei jedem TV-Feature die Eingaben, Gesamtanzahl/Sortierung, Titel/Metadaten, Paging, Errors, Berechtigungen und reale Aktionen gegenüber dem Webfrontend dokumentieren und testen. Kodi/Plex sind mögliche 10-Foot-UX-Referenzen; eine aktuelle Gegenüberstellung wurde noch nicht erhoben.
