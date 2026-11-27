# VDR Suite OSD

Ein eigenständiges natives VDR-Frontend für **VDR-Suite**. Das Plugin soll die Funktionen und installierten Addons der Suite am Fernseher per Fernbedienung verfügbar machen, ohne ein Ausgabeplugin zu forken.

**Projektstatus:** erster Hardwaretest auf yaVDR mit VDR 2.8.3 und VAAPIVideo 1.10.1. Die vollständige visuelle und Stabilitätsabnahme steht aus.

## Grundsätze

- **Kein Fork von vaapivideo:** Das Ausgabeplugin (z. B. vaapivideo oder softhddevice) bleibt Eigentümer von Video, Audio und OSD-Rendering.
- **Suite-OSD ist ein VDR-Plugin:** Die VDR-Skin-API und eigene native Menüs sind die Darstellungs- und Bedienebene. Es ist **kein** eingebetteter HTML-Browser.
- **Lokal zuerst:** VDR ist Eigentümer von Kanälen, EPG, Timern und Aufnahmen; `epgsearch` verwaltet die Suchtimer. Das Suite-OSD ruft diese Funktionen direkt lokal auf. Nur Suite-eigene Addons nutzen die dafür bereits vorhandenen Suite-/Agent-Dienste; Public API v1 bleibt für externe Clients.
- **Addons statt Sonderfälle:** EPG, Timer, Aufnahmen, Suchtimer, Teletext, HbbTV, Image, Media Tools und weitere installierte Suite-Funktionen sollen dieselbe navigierbare TV-Oberfläche erhalten.
- **Bestehende Komponenten wiederverwenden:** Teletext über vorhandene VDR-Plugins, HbbTV über vdr-plugin-web/cefbrowser/remotetranscode, Medienwiedergabe über nachgewiesene VDR-/Ausgabeplugin-Schnittstellen.
- **Keine zusätzlichen Aktivierungs- oder Berechtigungsgates:** Vorhandene Suite-Autorisierung beibehalten; eine Addon-Beschreibung erzeugt keine eigenen Rechte.

## Home-Performance (0.9.24-dev)

Home-Daten und Geometrie werden ereignisgesteuert vorbereitet. SSE für Aufnahmen und Wiedergabefortschritt läuft pluginweit, auch bei geschlossenem Menü. Aufnahme-Details, Browser-Kacheln, Replay-Info und Aufnahme-Fokus verwenden kanonische Suite-Metadaten mit exaktem Identitätsnachweis statt Titelgleichheit. Public-v1-Previews sind optional; ältere Core-Versionen bleiben über das Originalbild kompatibel. `HOME_PERF_OPEN` und `HOME_PERF_VIDEO` messen die Eröffnungsphasen und Videoskalierung. Reale TV-Abnahme steht aus. Die native Genre-Gruppierung, Geräte-Bootstrap und ImageNext-Bereitschaft verwenden noch ihre bisherigen Owner-Verträge.

## Home-Performance (0.9.23-dev)

Pluginweit begrenzte RAM-Arbeitscaches behalten native Poster und Metadaten, Fokusbilder, Logos und geometrische Renderbestandteile über Menüwechsel. Ein Worker bereitet native Inhalte auch bei geschlossenem Menü vor; ein begrenzter, an Menüwechsel gebundener Abruf wärmt private Public-v1-Aufnahmebilder. No-store-Projektionen und Details bleiben menügebunden. Das freigegebene Layout bleibt unverändert. Die Logzeilen `HOME_PERF` und `HOME_PERF_PROCESS` erfassen CPU-seitige Flush-Zeit, EPG-Auflösung, D-Pad, HTTP, Dekodierung und Speicher. Reale TV-Latenzen sind noch nicht gemessen. Weitere Owner-Befunde und offene Punkte stehen in [der Roadmap](docs/roadmap.md).

## Implementierungsstand zuvor (0.9.12-dev)

- **TV-Feedback:** Home verwendet bevorzugt TVScraper-Film-/Serientitel und Episode, mit nativem Namen nur als Fallback. Die Live-Videofläche endet vor Zeit und Fortschritt; Fanart-Komposition bleibt im Cache. Kontrast und fehlende Poster sind angepasst. Geräte-Pairing zeigt einen sichtbaren Status und prüft die aktive Bootstrap-Anfrage im vom Core gelieferten Intervall asynchron. Nach Genehmigung folgt einmalige Credential-Ausgabe, lokale Speicherung und Verbindungstest; die Rechtevergabe bleibt im Webfrontend. Echte TV-Abnahme offen.

- **Cinematic-Home nach der freigegebenen Vorlage:** Native ARGB-Rastergrafiken mit geschwungenem Licht, weich auslaufenden Schatten, kantengeglätteten Rahmen und Icons, gecachten fertigen Posterkompositionen sowie neuer Kopf-/Sidebar-Gestaltung ohne permanente Fußleiste. Die visuelle Gesamt- und Hardwareabnahme ist ausdrücklich noch offen.
- **Layout:** sechs gleich große 2:3-Portraitkarten pro Reihe, drei sichtbare Reihen (Jetzt, Als nächstes, Neu aufgenommen), weitere Reihen scrollbar. Abgerundete Rahmen, dezente Schatten, Cyan-Fokus ohne Vergrößerung und proportional beschnittene Poster mit opakem Textverlauf. Aufnahmekarten haben keine Senderlogos.
- **Getrennte Vorschauen:** native Live-TV-Videoebene oben rechts, darunter die Fanart und Kurzinfo der fokussierten Sendung/Aufnahme. Nach kurzem Verweilen werden TVScraper-Daten geladen; VDR liefert reale Sendezeiten, Gesamtdauer, Restzeit und Aufnahmeinformationen. Keine zusätzliche Videoquelle, kein Netzwerkpolling. Das bisherige native Menü, die einklappbare Sidebar und bestehende Aktionen bleiben erhalten.
- **Abnahme:** aktuelle Quellcode-/Buildtests und der [Designvertrag](docs/approved-tv-home-design.md) sind getrennt von der noch ausstehenden visuellen Hardwareabnahme. Die folgenden älteren Abschnitte sind historische Implementierungsstände.

## Implementierungsstand zuvor (0.9.4-dev)

- **Bilder-Karussell im freigegebenen SuiteOSD-Design (0.9.4-dev):** Die Galerie bietet jetzt Raster **und** Karussell als persistente Anzeigeart (`suiteosd.PhotoView` in VDRs `setup.conf`; Wechsel mit Taste `1`). Die Karussellansicht zeigt ein großes scharfes mittleres Foto und je zwei nach innen geneigte, verkleinerte und abgedunkelte Nachbarbilder mit dezenter Spiegelung. **Die Perspektive ist bewusst nach innen orientiert:** Bei linken Fotos ist die rechte Innenkante höher, bei rechten Fotos die linke. Beim Wechsel mit Links/Rechts bewegen sich die Karten in rund 220 ms Richtung Mitte beziehungsweise nach außen. Die Galerie füllt fast die gesamte OSD-Fläche: kein linkes Menü, keine rechte Infospalte, nur der aktuelle Ordnername oben und eine kompakte Tastenleiste unten. Die eigentliche Bilddekodierung, Quellenvalidierung und Vollbild-/Diashow-Wiedergabe verbleiben bei `image_next`. Reine CPU-OSD-Projektion der ARGB-Vorschaubilder, kein GPU-3D- oder Browser-Layer. Headless-Perspektivtests und CI-Builds sind eingerichtet; ein realer Fernsehtest steht aus.

## Implementierungsstand zuvor (0.9.3-dev)

- **Bildergalerie mit image_next (0.9.3-dev):** Der Sidebar-Eintrag „Bilder“ öffnet einen eigenen SuiteOSD-Fotobrowser mit fünf Spalten, Cyan-Fokus und lokaler Ordner-/Quellennavigation. Quelle, Verzeichniseinträge und ARGB-Vorschaubilder kommen ausschließlich aus den drei versionierten nativen VDR-Services des installierten Plugins `image_next`; dessen Thumbnail-Cache bleibt verantwortlich. OK/Rot öffnet Bilder, Grün startet die Diashow über den bestehenden image_next-Player, Zurück/Blau wechselt Ordner/Quelle/Home. Bei fehlendem Plugin oder fehlender Bildquelle erscheint ein verständlicher Hinweis. **Beide Plugins müssen mit derselben VDR-ABI gebaut werden; die reale TV-Abnahme steht aus.** Siehe [Bilder-Integration](docs/image-next-integration.md).

## Implementierungsstand zuvor (0.9.2)

- **Kompakte Suite-Einblendungen (0.9.2):** Die nativen VDR-Skin-Klassen für Lautstärke/Stummschaltung (`cSkinDisplayVolume`), Audio-/Sprachauswahl (`cSkinDisplayTracks`) und Hinweis-/Warn-/Fehlermeldungen (`cSkinDisplayMessage`) verwenden das gleiche dunkle Suite-Design mit Cyan-Akzenten. Tonspur-Listen zeigen bis zu fünf sichtbare Einträge samt Auswahl, Spurenzahl und Stereo/Links/Rechts; Lautstärke wird aus dem von VDR gelieferten Wert angezeigt. Für diese drei Anzeigen wird bei fehlender TrueColor-OSD-Fläche auf eine bereits installierte VDR-Skin zurückgegriffen. Fernbedienungsaktionen und Einblendzeiten gehören weiter VDR; kein Ausgabeplugin-Fork und keine neuen Berechtigungen.

- **Standardmenüs im Suite-Design (0.9.1):** `suite_menu.c` zeichnet über `cSkinDisplayMenu` die echten VDR-/Plugin-Menüeinträge in der freigegebenen dunklen Zwei-Spalten-Gestaltung: links Menü und Fokusrahmen, rechts Auswahl/Metadaten/optional TVScraper-Fanart. Die native VDR-Menülogik, Sortierung, Farb-/Fernbedienungstasten und Aktionen bleiben erhalten. Timer, Kanäle, EPG-Einträge und Aufnahmen nutzen die bestehenden typisierten VDR-Menüdaten; textbasierte Einstellungs-/Plugin-Menüs behalten ihre übergebenen Inhalte, Tabs und Textrücklauf. `SetEvent`, `SetRecording`, `SetText` und `Scroll` sind implementiert. Wenn der OSD-Provider keine 32-Bit-Fläche bereitstellt, delegiert der Skin an eine vorhandene VDR-Skin. Eigene OSD-Renderer fremder Plugins werden nicht verändert. Ein echter TV-Test steht weiterhin aus.
- **VDR-Suite-Skin (0.9.0):** Optional auswählbarer VDR-`cSkin` namens `suiteosd` (Anzeige „VDR Suite (TV und Wiedergabe)“) für die Standard-VDR-Kanalinfo bei `OK` im Live-TV sowie die Wiedergabe-Fortschrittsanzeige bei `OK` während einer Aufnahme. Beide nutzen den lokalen VDR-/TVScraper-Eigentümer und zeigen echte Fanart und Metadaten, sofern verfügbar. Kein Web-/Public-v1-Request, kein zweiter Player. **Die Skin wird nicht automatisch aktiviert**: Sie muss bei Bedarf unter VDRs OSD-Einstellungen ausgewählt werden. Lautstärke, Audiospuren und Meldungen verwenden ab 0.9.2 eigene native Suite-Renderer; eine echte TV-/Remote-Abnahme und das visuelle Feintuning stehen weiterhin aus.
- **Aufnahme-Info (0.9.0):** Im Suite-Home bzw. Kachelbrowser öffnet `Info` die großflächige Detailseite mit lokaler Fanart, Poster, Episodenangaben, Genre, Laufzeit, Bewertung, Erscheinungsdatum und Synopsis. Dort startet `OK` oder `Rot` die native VDR-Wiedergabe. Wiedergaben, die aus Suite-OSD gestartet wurden, verwenden einen unveränderten VDR-Replay-Player mit erweitertem `GetInfo()`: `Info` während der Wiedergabe liefert die Suite-Detailseite. Andere native VDR-Wiedergabewege bleiben vorerst bei ihrem eigenen Info-Menü.
- Native `cOsdObject`-Home-Ansicht mit fester Kopfzeile, schmaler Icon-Leiste und eingeblendeten **allen** Labels beim Navigieren auf die Sidebar.
- Pfeiltasten, OK und Zurück sowie vertikale Home-Sektionen ohne sichtbaren Scrollbalken und horizontale Sender-/Aufnahmekacheln.
- Reale VDR-Kanäle, aktuelle/folgende EPG-Ereignisse und Aufnahmetitel als Snapshot (keine künstlichen Beispieldaten).
- **EPG-Zeitleiste nach bestehendem Webfrontend:** Das EPG-Symbol öffnet eine native Suite-Ansicht im bewährten Webfrontend-Stil: Sender links, horizontale proportionale Sendezeit-Kacheln, 30-Minuten-Raster, orange Jetzt-Markierung und aktive Auswahl, ohne sichtbaren Scrollbalken. Alternativ (Blaue Taste) **Zeit vertikal**, **Live-Liste**, **Läuft jetzt** und **Als nächstes**. Die Webfrontend-Tagesseiten **Aktuelle 24h / Nächste 24h** bleiben erhalten; die 10-Fuß-Ansicht zeigt wegen der Lesbarkeit jeweils einen verschiebbaren Vier-Stunden-Ausschnitt. Programme und Beschreibungen stammen direkt aus `cSchedules` unter VDR-Locks, nicht aus API v1. **OK/Info** öffnet echte EPG-Details; **Zurück** führt zum Suite-Home. Der native VDR-Programmführer ist weiterhin über VDR-Tasten aufrufbar.
- **Timeline-Fernbedienung:** Pfeile wählen Sender und Sendungen bzw. in der vertikalen Ansicht Kanal/Zeitschritte; Rot: Jetzt, Grün: aktuelle 24 Stunden, Gelb: nächste 24 Stunden, Blau: Ansichtswechsel. Der Headless-Test prüft Kanalfokus, Zeitfenster, 24-Stunden-Seiten, Ereignisgeometrie und fünf Darstellungsmodi.
- **EPG-Gruppen:** Die vorhandenen Kanalgruppen aus `channels.conf` (`cChannel::GroupSep()`) werden als Filter angezeigt. Taste `0` wechselt zwischen „Alle Sender“ und vorhandenen Gruppen. Der gewählte Sender bleibt bei einem Filterwechsel erhalten, soweit er in der Gruppe vorkommt.
- **EPG-Senderlogos:** Die für das Webfrontend vorhandenen lokalen VDR-Suite-/SkinDesigner-Logo-Verzeichnisse werden ohne HTTP durchsucht. PNG-, JPEG- und SVG-Logos erscheinen in Kanalzeilen und Listen mit Textfallback. SVGs werden lokal und größenbegrenzt mit librsvg gerendert. Maximal 48 kleine Logos im OSD-Cache.
- **EPG-TVscraper-Artwork:** Im Sendungsdetail wird über den nativen VDR-Service `GetScraperVideo` das Bild der markierten Sendung ermittelt und nach Freigabe der Schedule-Locks dekodiert. Kein zweiter Scraper und kein Public-API-v1-Request. Bei fehlendem TVScraper oder Artwork bleiben die VDR-EPG-Texte sichtbar.
- **EPG-Timer:** Im Sendungsdetail `Rot` oder `Record` wählen, dann mit `OK` die Aufnahme bestätigen; `Zurück` bricht ab. Der VDR-Kern wird unter seinen nativen Timer-/Kanal-/Schedule-Locks aufgerufen, dabei werden doppelte Timer erkannt und VDRs vorhandene Remote-Peer-Semantik berücksichtigt. Eine echte Timeranlage auf dem yaVDR ist noch nicht durchgeführt.
- **Webfrontend-Parität:** Die vier Erweiterungen sind implementiert, jedoch noch nicht visuell mit dem realen Fernseher abgenommen. Detaillierte visuelle Abstimmung und die vollständige native Skin der tieferen VDR-Menüs sind weiter offen.
- Direkter VDR-Senderwechsel bei Auswahl einer Jetzt-Sendung und VDR-eigene Wiedergabe für ausgewählte Aufnahmen; kein zweiter Video-Player.
- Menüübergabe an **bereits installierte** VDR-Plugins `web` (HbbTV), `osdteletext` und `epgsearch` (Suchtimer/Suche, optional `epgsearchonly`) per `cRemote::CallPlugin`. Verfügbarkeit und Fokus-/Rückkehrverhalten sind auf dem echten VDR noch zu testen.
- **Aufnahmen mit TVScraper-Daten:** Die sieben gerade sichtbaren Aufnahmekacheln lesen lokal über den VDR-Service `GetScraperVideo` Filmtitel, Episodenname, Erscheinungsdatum, Beschreibung und Bildpfad. PNG-/JPEG-Artwork wird als kleines natives OSD-Bild eingeblendet; `Info` zeigt die neue vollflächige Detailansicht einschließlich Fanart/Poster und Metadaten; `Zurück` schließt sie. Ohne installierten TVScraper oder bei nicht gefundenem Artwork bleiben Titel, Datum, VDR-Beschreibung und Wiedergabe verfügbar. Kein API-v1-Aufruf.
- **Artwork-Speicher:** Nur sichtbare Aufnahmen werden angefragt, der Thumbnail-Cache ist auf 48 Einträge begrenzt. Bei Bedarf kann das Home mit Taste `0` neu eingelesen werden; in der Aufnahmenansicht zeigt `Blau` Metadaten.
- **Aufnahmen 2.0:** Ein eigener TV-Kachelbrowser mit drei umschaltbaren Reitern **Ordner / Genre / Alle**. Die Ordner folgen direkt dem VDR-Aufnahmennamen einschließlich Unterordnern (`cRecording::BaseName()`); Genre-Gruppen beziehen mehrere Schlagwörter pro Aufnahme aus TVScraper, ohne etwas zu verschieben. Die Genre-Liste wird erst nach Aufrufen des Reiters befüllt, und fehlende Metadaten erscheinen unter „Ohne Genre“. Grid-Scrollen und Sortieren nach Datum/Name funktionieren ohne sichtbaren Scrollbalken.
- **VDR-Fernbedienung:** Im Home öffnet `Rot` den Aufnahmenbrowser, `Grün` den Suite-EPG, `Gelb`/`Info` das Informationsfenster zur ausgewählten Sendung oder Aufnahme und `Blau` die native Timerübersicht. In Aufnahmen: Pfeile/OK für Tabs, Ordner und Wiedergabe; `Rot` startet die VDR-Wiedergabe, `Grün`/`0` wechseln Name/Datum, `Blau`/`Info` öffnen die Metadaten, `Gelb` fragt vor dem Löschen nach. `Zurück` springt im Ordnerbaum eine Ebene höher, vom Wurzelverzeichnis zurück zum Home. Bei laufenden oder gerade bearbeiteten Aufnahmen wird die komplexere Löschlogik an das vorhandene VDR-Aufnahmenmenü abgegeben.
- **Optionale Live-Vorschau in Aufnahmen:** Das laufende Bild wird über die *native* Ausgabeplugin-Schnittstelle `cDevice::CanScaleVideo()` und `ScaleVideo()` auf die rechte Bildschirmseite verkleinert. Das TrueColor-OSD zeichnet dafür einen transparenten Ausschnitt. Beim Verlassen des Browsers oder Start der Wiedergabe wird das Bild mit `ScaleVideo(cRect::Null)` wiederhergestellt. Unterstützt ein Ausgabeplugin diese Funktion nicht, bleibt die Kachelansicht ohne Vorschau nutzbar. **Das Verhalten mit `vaapivideo` ist noch nicht auf dem realen yaVDR getestet.** Kein zusätzlicher TV-Stream oder Video-Decoder.
- **Gemeinsames Einstellungs-Dashboard (0.7.0):** Das Sidebar-Symbol Einstellungen öffnet jetzt sechs Kategorien im Suite-Design: VDR, VDR-Suite, Plugins, TV-Oberfläche, Benutzer & Geräte, System. **VDR/Plugins** nutzen unverändert VDRs native `kSetup`-Menüs. Die TV-Oberfläche kann die Mini-Live-TV-Vorschau wirklich ein-/ausschalten; die Einstellung wird im **vorhandenen** VDR-Plugin-Setup (`suiteosd.LivePreview`) gespeichert und ist dort ebenfalls änderbar. Seit 0.8.0 sind erste Suite-Core-Bearbeitungsfunktionen im TV-OSD erreichbar: Backend-Auswahl, Media-Tools-Aktivierung, H.264-/H.265-CRF, Account aktiv/inaktiv, Anzeigename und bestätigter Device-Widerruf. Der Zugang verwendet die **bestehende Human-Browser-Session mit CSRF und ETag**, ohne einen neuen Admin- oder Speicherweg. Weitere Rechte-/Pairing-/Account-Funktionen bleiben im Webfrontend; siehe [Core-Administration 0.8.0](docs/settings-core-administration.md).
- **Core-Admin-Client ab 0.8.0:** Eine maskierte Fernbedienungs-Tastatur meldet den bestehenden Human Account über die vorhandene Core-Sitzung an. Die im VDR-Plugin-Setup einstellbare Core-URL hat den nachgewiesenen lokalen Standard http://127.0.0.1:18080. Passwort und Session werden nicht persistiert; beim Verlassen der Einstellungen erfolgt Logout. Kein pauschaler Public-API-v1-Umweg für VDR-eigene Funktionen.
- Derzeit **keine** eingebettete Live-Vorschau im Home und keine Public-API-v1-Anbindung. Die Skin umfasst Kanalinfo, Wiedergabe, Standardmenüs und ab 0.9.2 auch Lautstärke, Tonspuren und Statusmeldungen; die echte TV-/Hardwareabnahme steht aus. Das TV-Vorschaufeld im Home ist weiterhin ein ausdrücklich beschrifteter Platzhalter.
- Ein echter Ausgabe-/OSD-Provider und passende VDR-ABI sind für eine Anzeige nötig; der Code ist **ohne vorhandenes Ausgabeplugin entwickel- und CI-testbar**, aber noch nicht visuell abgenommen.

Die GitHub-Actions-Matrix kompiliert gegen den unveränderten Upstream-VDR-Quellstand der Tags **2.8.3** und **2.8.4** sowie separate Headless-Tests für Home-/EPG-/Aufnahmen- und Einstellungsnavigation, TV-Admin-Tastatur und HTTP-Cookie/CSRF/ETag-Integration, Kanalgruppen, lokale Logoauflösung und PNG-/JPEG-Bilddekodierung. Ein erfolgreicher Build bedeutet keine Funktionsabnahme auf dem realen yaVDR.

## Zusätzliche Build-Abhängigkeiten ab 0.3.0

`libpng-dev`, `libjpeg-dev` und `librsvg2-dev` für das lokale Dekodieren von TVScraper-Bildern, sowie die bereits benötigten VDR-Entwicklungsheader. Das Projekt enthält ausschließlich den öffentlichen TVScraper-Service-Header mit Upstream-Provenienz und GPL-Hinweis unter [vendor/tvscraper](vendor/tvscraper/README.md); **keinen TVScraper-Fork**. Zur Laufzeit ist TVScraper optional. Test mit dem tatsächlich installierten TVScraper und OSD-Provider steht noch aus.

## Core-Admin-Abhängigkeiten (0.8.0)

Für die authentifizierte Suite-Verwaltung werden zusätzlich libcurl4-openssl-dev und libjson-c-dev beim Build benötigt. Die CI installiert diese Header für beide VDR-2.8-Versionen. Runtime benötigt die zugehörigen Bibliotheken; Paketierung und Real-Host-Einrichtung sind nicht Teil dieser Quellcode-CI.

## Dokumente

- [Architektur und Integrationsgrenzen](docs/architecture.md)
- [Addon- und OSD-Interaktionsvertrag (Entwurf)](docs/addon-ui-contract.md)
- [Roadmap und Abnahmekriterien](docs/roadmap.md)
- [Gemeinsame VDR-/Suite-Einstellungen: Eigentümer und Bedienung](docs/settings-integration.md)
- [Core-Administration direkt im TV-OSD (0.8.0)](docs/settings-core-administration.md)

## Quellen und Abhängigkeiten

- [VDR-Suite](https://github.com/hotzenplotz5/vdr-suite) – Backend und Public API v1
- [vaapivideo](https://github.com/dnehring7/vdr-plugin-vaapivideo) – *optionales* Ausgabeplugin; kein Fork
- [VDR](https://www.tvdr.de/) – Plugin-, Skin- und OSD-Schnittstellen

**Kein Installationsversprechen:** Eine Quellcode-Skizze oder ein Git-Commit bedeutet nicht, dass das Plugin gegen die konkrete VDR-ABI gebaut und mit HbbTV, Teletext oder Medienwiedergabe getestet ist.

Die native Aufnahme-Genreübersicht verwendet die aus VDR-Suite übernommene kanonische Genre-Registry (`vendor/vdr-suite`, siehe Herkunftsnachweis). Bekannte deutsche/englische Aliase werden zusammengefasst und Aufnahmen pro Kategorie nur einmal gezählt.

## Lokales Debian-Paket

Mit zum Ziel-VDR passenden Entwicklungsheadern und den in `debian/control` angegebenen Build-Abhängigkeiten:

```sh
dpkg-buildpackage -b -us -uc
```

Das Paket nutzt den VDR-API-Suffix aus pkg-config und die ABI-Abhängigkeit über das VDR-Debhelper-Addon. Der Build installiert nichts auf dem laufenden VDR. Die Paketierung ist für lokale Hardwaretests bestimmt; eine Veröffentlichung (einschließlich Lizenz- und Provenienzprüfung) ist noch nicht freigegeben.
