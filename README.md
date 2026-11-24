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

## Aktueller Implementierungsstand (0.7.0, noch nicht auf TV abgenommen)

- Native `cOsdObject`-Home-Ansicht mit fester Kopfzeile, schmaler Icon-Leiste und eingeblendeten **allen** Labels beim Navigieren auf die Sidebar.
- Pfeiltasten, OK und Zurück sowie vertikale Home-Sektionen ohne sichtbaren Scrollbalken und horizontale Sender-/Aufnahmekacheln.
- Reale VDR-Kanäle, aktuelle/folgende EPG-Ereignisse und Aufnahmetitel als Snapshot (keine künstlichen Beispieldaten).
- **EPG-Zeitleiste nach bestehendem Webfrontend:** Das EPG-Symbol öffnet eine native Suite-Ansicht im bewährten Webfrontend-Stil: Sender links, horizontale proportionale Sendezeit-Kacheln, 30-Minuten-Raster, orange Jetzt-Markierung und aktive Auswahl, ohne sichtbaren Scrollbalken. Alternativ (Blaue Taste) **Zeit vertikal**, **Live-Liste**, **Läuft jetzt** und **Als nächstes**. Die Webfrontend-Tagesseiten **Aktuelle 24h / Nächste 24h** bleiben erhalten; die 10-Fuß-Ansicht zeigt wegen der Lesbarkeit jeweils einen verschiebbaren Vier-Stunden-Ausschnitt. Programme und Beschreibungen stammen direkt aus `cSchedules` unter VDR-Locks, nicht aus API v1. **OK/Info** öffnet echte EPG-Details; **Zurück** führt zum Suite-Home. Der native VDR-Programmführer ist weiterhin über VDR-Tasten aufrufbar.
- **Timeline-Fernbedienung:** Pfeile wählen Sender und Sendungen bzw. in der vertikalen Ansicht Kanal/Zeitschritte; Rot: Jetzt, Grün: aktuelle 24 Stunden, Gelb: nächste 24 Stunden, Blau: Ansichtswechsel. Der Headless-Test prüft Kanalfokus, Zeitfenster, 24-Stunden-Seiten, Ereignisgeometrie und fünf Darstellungsmodi.
- **EPG-Gruppen:** Die vorhandenen Kanalgruppen aus `channels.conf` (`cChannel::GroupSep()`) werden als Filter angezeigt. Taste `0` wechselt zwischen „Alle Sender“ und vorhandenen Gruppen. Der gewählte Sender bleibt bei einem Filterwechsel erhalten, soweit er in der Gruppe vorkommt.
- **EPG-Senderlogos:** Die für das Webfrontend vorhandenen lokalen VDR-Suite-/SkinDesigner-Logo-Verzeichnisse werden ohne HTTP durchsucht. PNG-/JPEG-Logos erscheinen in Kanalzeilen und Listen mit Textfallback; reine SVG-Dateien benötigen für eine Anzeige noch einen nativen Renderer. Maximal 48 kleine Logos im OSD-Cache.
- **EPG-TVscraper-Artwork:** Im Sendungsdetail wird über den nativen VDR-Service `GetScraperVideo` das Bild der markierten Sendung ermittelt und nach Freigabe der Schedule-Locks dekodiert. Kein zweiter Scraper und kein Public-API-v1-Request. Bei fehlendem TVScraper oder Artwork bleiben die VDR-EPG-Texte sichtbar.
- **EPG-Timer:** Im Sendungsdetail `Rot` oder `Record` wählen, dann mit `OK` die Aufnahme bestätigen; `Zurück` bricht ab. Der VDR-Kern wird unter seinen nativen Timer-/Kanal-/Schedule-Locks aufgerufen, dabei werden doppelte Timer erkannt und VDRs vorhandene Remote-Peer-Semantik berücksichtigt. Eine echte Timeranlage auf dem yaVDR ist noch nicht durchgeführt.
- **Webfrontend-Parität:** Die vier Erweiterungen sind implementiert, jedoch noch nicht visuell mit dem realen Fernseher abgenommen. SVG-Logos, detaillierte visuelle Abstimmung und die vollständige native Skin der tieferen VDR-Menüs sind weiter offen.
- Direkter VDR-Senderwechsel bei Auswahl einer Jetzt-Sendung und VDR-eigene Wiedergabe für ausgewählte Aufnahmen; kein zweiter Video-Player.
- Menüübergabe an **bereits installierte** VDR-Plugins `web` (HbbTV), `osdteletext` und `epgsearch` (Suchtimer/Suche, optional `epgsearchonly`) per `cRemote::CallPlugin`. Verfügbarkeit und Fokus-/Rückkehrverhalten sind auf dem echten VDR noch zu testen.
- **Aufnahmen mit TVScraper-Daten:** Die sieben gerade sichtbaren Aufnahmekacheln lesen lokal über den VDR-Service `GetScraperVideo` Filmtitel, Episodenname, Erscheinungsdatum, Beschreibung und Bildpfad. PNG-/JPEG-Artwork wird als kleines natives OSD-Bild eingeblendet; `Info` zeigt die Beschreibung und `Zurück` schließt die Detailansicht. Ohne installierten TVScraper oder bei nicht gefundenem Artwork bleiben Titel, Datum, VDR-Beschreibung und Wiedergabe verfügbar. Kein API-v1-Aufruf.
- **Artwork-Speicher:** Nur sichtbare Aufnahmen werden angefragt, der Thumbnail-Cache ist auf 48 Einträge begrenzt. Bei Bedarf kann das Home mit Taste `0` neu eingelesen werden; in der Aufnahmenansicht zeigt `Blau` Metadaten.
- **Aufnahmen 2.0:** Ein eigener TV-Kachelbrowser mit drei umschaltbaren Reitern **Ordner / Genre / Alle**. Die Ordner folgen direkt dem VDR-Aufnahmennamen einschließlich Unterordnern (`cRecording::BaseName()`); Genre-Gruppen beziehen mehrere Schlagwörter pro Aufnahme aus TVScraper, ohne etwas zu verschieben. Die Genre-Liste wird erst nach Aufrufen des Reiters befüllt, und fehlende Metadaten erscheinen unter „Ohne Genre“. Grid-Scrollen und Sortieren nach Datum/Name funktionieren ohne sichtbaren Scrollbalken.
- **VDR-Fernbedienung:** Im Home öffnet `Rot` den Aufnahmenbrowser, `Grün` den Suite-EPG, `Gelb`/`Info` das Informationsfenster zur ausgewählten Sendung oder Aufnahme und `Blau` die native Timerübersicht. In Aufnahmen: Pfeile/OK für Tabs, Ordner und Wiedergabe; `Rot` startet die VDR-Wiedergabe, `Grün`/`0` wechseln Name/Datum, `Blau`/`Info` öffnen die Metadaten, `Gelb` fragt vor dem Löschen nach. `Zurück` springt im Ordnerbaum eine Ebene höher, vom Wurzelverzeichnis zurück zum Home. Bei laufenden oder gerade bearbeiteten Aufnahmen wird die komplexere Löschlogik an das vorhandene VDR-Aufnahmenmenü abgegeben.
- **Optionale Live-Vorschau in Aufnahmen:** Das laufende Bild wird über die *native* Ausgabeplugin-Schnittstelle `cDevice::CanScaleVideo()` und `ScaleVideo()` auf die rechte Bildschirmseite verkleinert. Das TrueColor-OSD zeichnet dafür einen transparenten Ausschnitt. Beim Verlassen des Browsers oder Start der Wiedergabe wird das Bild mit `ScaleVideo(cRect::Null)` wiederhergestellt. Unterstützt ein Ausgabeplugin diese Funktion nicht, bleibt die Kachelansicht ohne Vorschau nutzbar. **Das Verhalten mit `vaapivideo` ist noch nicht auf dem realen yaVDR getestet.** Kein zusätzlicher TV-Stream oder Video-Decoder.
- **Gemeinsames Einstellungs-Dashboard (0.7.0):** Das Sidebar-Symbol Einstellungen öffnet jetzt sechs Kategorien im Suite-Design: VDR, VDR-Suite, Plugins, TV-Oberfläche, Benutzer & Geräte, System. **VDR/Plugins** nutzen unverändert VDRs native `kSetup`-Menüs. Die TV-Oberfläche kann die Mini-Live-TV-Vorschau wirklich ein-/ausschalten; die Einstellung wird im **vorhandenen** VDR-Plugin-Setup (`suiteosd.LivePreview`) gespeichert und ist dort ebenfalls änderbar. Suite-Core-Einstellungen (Backend/Addons/Media Tools, Benutzer/Geräte) sind als Bereiche navigierbar, aber ihre Bearbeitung bleibt bis zur bestätigten lokalen Core-/Agent-Anbindung im bestehenden Webfrontend. Kein doppelter Konfigurationsspeicher.
- Derzeit **keine** eingebettete Live-Vorschau im Home, keine gesonderte Skin-Implementierung und keine Public-API-v1-Anbindung. Das TV-Vorschaufeld im Home ist weiterhin ein ausdrücklich beschrifteter Platzhalter.
- Ein echter Ausgabe-/OSD-Provider und passende VDR-ABI sind für eine Anzeige nötig; der Code ist **ohne vorhandenes Ausgabeplugin entwickel- und CI-testbar**, aber noch nicht visuell abgenommen.

Die GitHub-Actions-Matrix kompiliert gegen den unveränderten Upstream-VDR-Quellstand der Tags **2.8.3** und **2.8.4** sowie separate Headless-Tests für Home-/EPG-/Aufnahmen- und Einstellungsnavigation, Kanalgruppen, lokale Logoauflösung und PNG-/JPEG-Bilddekodierung. Ein erfolgreicher Build bedeutet keine Funktionsabnahme auf dem realen yaVDR.

## Zusätzliche Build-Abhängigkeiten ab 0.3.0

`libpng-dev` und `libjpeg-dev` für das lokale Dekodieren von TVScraper-Bildern, sowie die bereits benötigten VDR-Entwicklungsheader. Das Projekt enthält ausschließlich den öffentlichen TVScraper-Service-Header mit Upstream-Provenienz und GPL-Hinweis unter [vendor/tvscraper](vendor/tvscraper/README.md); **keinen TVScraper-Fork**. Zur Laufzeit ist TVScraper optional. Test mit dem tatsächlich installierten TVScraper und OSD-Provider steht noch aus.

## Dokumente

- [Architektur und Integrationsgrenzen](docs/architecture.md)
- [Addon- und OSD-Interaktionsvertrag (Entwurf)](docs/addon-ui-contract.md)
- [Roadmap und Abnahmekriterien](docs/roadmap.md)
- [Gemeinsame VDR-/Suite-Einstellungen: Eigentümer und Bedienung](docs/settings-integration.md)

## Quellen und Abhängigkeiten

- [VDR-Suite](https://github.com/hotzenplotz5/vdr-suite) – Backend und Public API v1
- [vaapivideo](https://github.com/dnehring7/vdr-plugin-vaapivideo) – *optionales* Ausgabeplugin; kein Fork
- [VDR](https://www.tvdr.de/) – Plugin-, Skin- und OSD-Schnittstellen

**Kein Installationsversprechen:** Eine Quellcode-Skizze oder ein Git-Commit bedeutet nicht, dass das Plugin gegen die konkrete VDR-ABI gebaut und mit HbbTV, Teletext oder Medienwiedergabe getestet ist.
