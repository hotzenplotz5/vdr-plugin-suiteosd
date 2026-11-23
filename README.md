# VDR Suite OSD

Ein eigenständiges natives VDR-Frontend für **VDR-Suite**. Das Plugin soll die Funktionen und installierten Addons der Suite am Fernseher per Fernbedienung verfügbar machen, ohne ein Ausgabeplugin zu forken.

**Projektstatus:** frühe Entwicklung; noch keine auf einem echten VDR verifizierte Version.

## Grundsätze

- **Kein Fork von vaapivideo:** Das Ausgabeplugin (z. B. vaapivideo oder softhddevice) bleibt Eigentümer von Video, Audio und OSD-Rendering.
- **Suite-OSD ist ein VDR-Plugin:** Die VDR-Skin-API und eigene native Menüs sind die Darstellungs- und Bedienebene. Es ist **kein** eingebetteter HTML-Browser.
- **Lokal zuerst:** VDR ist Eigentümer von Kanälen, EPG, Timern und Aufnahmen; `epgsearch` verwaltet die Suchtimer. Das Suite-OSD ruft diese Funktionen direkt lokal auf. Nur Suite-eigene Addons nutzen die dafür bereits vorhandenen Suite-/Agent-Dienste; Public API v1 bleibt für externe Clients.
- **Addons statt Sonderfälle:** EPG, Timer, Aufnahmen, Suchtimer, Teletext, HbbTV, Image, Media Tools und weitere installierte Suite-Funktionen sollen dieselbe navigierbare TV-Oberfläche erhalten.
- **Bestehende Komponenten wiederverwenden:** Teletext über vorhandene VDR-Plugins, HbbTV über vdr-plugin-web/cefbrowser/remotetranscode, Medienwiedergabe über nachgewiesene VDR-/Ausgabeplugin-Schnittstellen.
- **Keine zusätzlichen Aktivierungs- oder Berechtigungsgates:** Vorhandene Suite-Autorisierung beibehalten; eine Addon-Beschreibung erzeugt keine eigenen Rechte.

## Aktueller Implementierungsstand (0.4.0, noch nicht auf TV abgenommen)

- Native `cOsdObject`-Home-Ansicht mit fester Kopfzeile, schmaler Icon-Leiste und eingeblendeten **allen** Labels beim Navigieren auf die Sidebar.
- Pfeiltasten, OK und Zurück sowie vertikale Home-Sektionen ohne sichtbaren Scrollbalken und horizontale Sender-/Aufnahmekacheln.
- Reale VDR-Kanäle, aktuelle/folgende EPG-Ereignisse und Aufnahmetitel als Snapshot (keine künstlichen Beispieldaten).
- **EPG-Zeitleiste nach bestehendem Webfrontend:** Das EPG-Symbol öffnet eine native Suite-Ansicht im bewährten Webfrontend-Stil: Sender links, horizontale proportionale Sendezeit-Kacheln, 30-Minuten-Raster, orange Jetzt-Markierung und aktive Auswahl, ohne sichtbaren Scrollbalken. Alternativ (Blaue Taste) **Zeit vertikal**, **Live-Liste**, **Läuft jetzt** und **Als nächstes**. Die Webfrontend-Tagesseiten **Aktuelle 24h / Nächste 24h** bleiben erhalten; die 10-Fuß-Ansicht zeigt wegen der Lesbarkeit jeweils einen verschiebbaren Vier-Stunden-Ausschnitt. Programme und Beschreibungen stammen direkt aus `cSchedules` unter VDR-Locks, nicht aus API v1. **OK/Info** öffnet echte EPG-Details; **Zurück** führt zum Suite-Home. Der native VDR-Programmführer ist weiterhin über VDR-Tasten aufrufbar.
- **Timeline-Fernbedienung:** Pfeile wählen Sender und Sendungen bzw. in der vertikalen Ansicht Kanal/Zeitschritte; Rot: Jetzt, Grün: aktuelle 24 Stunden, Gelb: nächste 24 Stunden, Blau: Ansichtswechsel. Der Headless-Test prüft Kanalfokus, Zeitfenster, 24-Stunden-Seiten, Ereignisgeometrie und fünf Darstellungsmodi.
- **Noch kein vollständiger Webfrontend-Feature-Paritätsnachweis:** Kanalgruppenfilter, Senderlogos, EPG-Artwork, direktes Erstellen eines Timers für die markierte Sendung sowie reales TV-OSD-/Fernbedienungsverhalten stehen aus. Die erste Version übernimmt die Kerngeometrie und Ansichten – keine Behauptung pixelgenauer TV-Abnahme.
- Direkter VDR-Senderwechsel bei Auswahl einer Jetzt-Sendung und VDR-eigene Wiedergabe für ausgewählte Aufnahmen; kein zweiter Video-Player.
- Menüübergabe an **bereits installierte** VDR-Plugins `web` (HbbTV), `osdteletext` und `epgsearch` (Suchtimer/Suche, optional `epgsearchonly`) per `cRemote::CallPlugin`. Verfügbarkeit und Fokus-/Rückkehrverhalten sind auf dem echten VDR noch zu testen.
- **Aufnahmen mit TVScraper-Daten:** Die sieben gerade sichtbaren Aufnahmekacheln lesen lokal über den VDR-Service `GetScraperVideo` Filmtitel, Episodenname, Erscheinungsdatum, Beschreibung und Bildpfad. PNG-/JPEG-Artwork wird als kleines natives OSD-Bild eingeblendet; `Info` zeigt die Beschreibung und `Zurück` schließt die Detailansicht. Ohne installierten TVScraper oder bei nicht gefundenem Artwork bleiben Titel, Datum, VDR-Beschreibung und Wiedergabe verfügbar. Kein API-v1-Aufruf.
- **Artwork-Speicher:** Nur sichtbare Aufnahmen werden angefragt, das Thumbnail-Cache ist auf 48 Einträge begrenzt; die blaue Taste verwirft den Cache und lädt die Daten neu.
- Derzeit **keine** eingebettete Live-Vorschau, keine gesonderte Skin-Implementierung und keine Public-API-v1-Anbindung. Das TV-Vorschaufeld ist weiterhin ein ausdrücklich beschrifteter Platzhalter.
- Ein echter Ausgabe-/OSD-Provider und passende VDR-ABI sind für eine Anzeige nötig; der Code ist **ohne vorhandenes Ausgabeplugin entwickel- und CI-testbar**, aber noch nicht visuell abgenommen.

Die GitHub-Actions-Matrix kompiliert gegen den unveränderten Upstream-VDR-Quellstand der Tags **2.8.3** und **2.8.4** sowie separate Headless-Tests für Navigation und PNG-/JPEG-Bilddekodierung. Ein erfolgreicher Build bedeutet keine Funktionsabnahme auf dem realen yaVDR.

## Zusätzliche Build-Abhängigkeiten ab 0.3.0

`libpng-dev` und `libjpeg-dev` für das lokale Dekodieren von TVScraper-Bildern, sowie die bereits benötigten VDR-Entwicklungsheader. Das Projekt enthält ausschließlich den öffentlichen TVScraper-Service-Header mit Upstream-Provenienz und GPL-Hinweis unter [vendor/tvscraper](vendor/tvscraper/README.md); **keinen TVScraper-Fork**. Zur Laufzeit ist TVScraper optional. Test mit dem tatsächlich installierten TVScraper und OSD-Provider steht noch aus.

## Dokumente

- [Architektur und Integrationsgrenzen](docs/architecture.md)
- [Addon- und OSD-Interaktionsvertrag (Entwurf)](docs/addon-ui-contract.md)
- [Roadmap und Abnahmekriterien](docs/roadmap.md)

## Quellen und Abhängigkeiten

- [VDR-Suite](https://github.com/hotzenplotz5/vdr-suite) – Backend und Public API v1
- [vaapivideo](https://github.com/dnehring7/vdr-plugin-vaapivideo) – *optionales* Ausgabeplugin; kein Fork
- [VDR](https://www.tvdr.de/) – Plugin-, Skin- und OSD-Schnittstellen

**Kein Installationsversprechen:** Eine Quellcode-Skizze oder ein Git-Commit bedeutet nicht, dass das Plugin gegen die konkrete VDR-ABI gebaut und mit HbbTV, Teletext oder Medienwiedergabe getestet ist.
