# Freigegebenes TV-Home – Designvertrag

**Freigabe:** Nutzer am 23.11.2026, nach mehreren Entwürfen. Die finale visuelle Vorlage ist die ausdrücklich mit „Perfekt!!! Los!“ bestätigte TV-OSD-Ansicht, abgeleitet aus dem echten VDR-Suite-Webfrontend-Home.

## Layout

- Dunkles, ruhiges **VDR-Suite-Design**; keine neue Streaming-App-Ästhetik.
- Fixierte Kopfzeile mit VDR-Suite, aktuellem Kanal und später realen Statusdaten.
- **Links standardmäßig nur Icons.** Fokus/Navigieren auf die Leiste klappt die Beschreibungen **aller** Icons zugleich auf; Verlassen klappt sie wieder zu. Keine zweite, permanent offene Beschriftungsleiste.
- Startbereich: **Live-TV-Informationen** (Sender, Jetzt, Als nächstes, Zeit/Progress); rechts das aktuelle TV-Programm **wenn die Ausgabeplugin-Schnittstelle eine solche Einbettung erlaubt**.
- Darunter vertikale, fortsetzbare Home-Abschnitte in dieser Reihenfolge:
  1. **Was läuft jetzt** – horizontale Sender-/Sendungskacheln;
  2. **Was läuft als nächstes** – gleiche Kanalnavigation, folgende Sendung;
  3. **Aufnahmen** – Kacheln mit Aufnahmetiteln und optionalem lokalem TVScraper-Artwork;
  4. spätere Home-Abschnitte entsprechend tatsächlichem VDR-Suite-Webfrontend (z. B. Weiterschauen), sobald Daten/Player-Vertrag vorhanden.
- **Vertikales Scrollen ohne sichtbaren Scrollbalken**. Horizontal durch die Kacheln navigieren, mit Fokusrahmen und automatischem Versetzen des sichtbaren Bereichs.
- Kein doppeltes Menü, keine riesigen Kacheln/Glüheffekte; niedrige Informationsverluste gegenüber Webfrontend.
- Fernbedienungspfeile, OK, Zurück. Farbtasten nur für echte vorhandene Funktionen belegen.

## Technische Implementierungsetappen

1. **Jetzt:** nativer VDR-Menüeinstieg, Headless-Navigationsmodell, 32-Bit-OSD-Darstellung, reale lokale EPG-Daten und Aufnahmetitel. Die erste Umsetzung darf Text-/Farbflächen statt Artwork verwenden und kennzeichnet unverdrahtete Aktionen ausdrücklich.
2. **Danach:** Live-TV-Bildeinbettung/Ausgabeplugin und Bild-/Artwork-Rendering **nur nach echter API-/Hardwareprüfung**; Fokushandoff zu Teletext und HbbTV ohne Fork.
3. **Danach:** weitere direkte VDR-/Plugin-Menüs und -Aktionen, außerdem die bestehenden lokalen Suite-/Agent-Schnittstellen für Media Tools, Image und andere Suite-spezifische Addons. Public API v1 ist für VDR-eigene lokale Funktionen keine Voraussetzung.

Das Projekt ist ohne angeschlossenes Ausgabeplugin **baubar und navigationsseitig testbar**, aber **nicht visuell auf einem realen TV abnehmbar**. Die aktuelle Implementierung zeichnet vorläufig ein explizit als solches gekennzeichnetes Live-TV-Vorschaufeld: Sie darf keinen fremden Streamplayer erfinden oder das ganze Fernsehbild als skaliertes Video ausgeben.

## Quellenbasis

- Nutzerfreigabe am 23.11.2026 und echte Home-Ansicht als visuelle Referenz.
- VDR-2.8.3-Schnittstellen: `cOsdObject`, `cOsdProvider`, `cChannels`, `cSchedules`, `cRecordings`.
- VDR-Suite-Webfrontend und bestehende Source-Owner im Hauptrepo.

## VDR-eigene Daten- und Menüanbindung (23.11.2026)

Die nativen VDR-Schnittstellen bleiben die Quelle für Sender, aktuelle/folgende Sendung, Timer und Aufnahmen. Der Home-Hero verwendet für den eingeschalteten Sender `cSchedule::GetPresentEvent()` **und** `GetFollowingEvent()`; die beiden horizontalen Senderreihen verwenden dieselben VDR-Schedules. Damit werden keine zweiten EPG-Berechnungen angelegt.

Bei **OK auf der eingeklappten/ausgeklappten Seitenleiste** startet die ausgewählte native Funktion:

| Sidebar | Umsetzung |
| --- | --- |
| Home | Suite-Home, Fokus wieder im Hauptinhalt |
| Live-TV | Suite-OSD schließen, zum laufenden Bild |
| EPG | Native Suite-EPG-Zeitleiste auf Basis lokaler VDR-Schedules, im bestehenden Webfrontend-Design; VDR-Programmführer bleibt separat verfügbar |
| Aufnahmen | Native Suite-Kachelansicht mit VDR-Ordnern, TVScraper-Genre, Detailansicht, Direktwiedergabe und optionaler Live-Vorschau via `ScaleVideo()` |
| Timer | VDR-Direktmenübefehl `kTimers` |
| Einstellungen | Gemeinsames natives Suite-OSD-Dashboard; VDR-Setup/Plugin-Setup weiter über `kSetup`, Suite-OSD-Vorschau über denselben VDR-Plugin-Setup-Schlüssel |
| HbbTV | Vorhandenes `web`-Plugin, `cRemote::CallPlugin()` |
| Teletext | Vorhandenes `osdteletext`-Plugin, `cRemote::CallPlugin()` |
| Suchtimer | Lokales `epgsearchonly` (direktes Suchmenü) oder `epgsearch` (Hauptmenü), `cRemote::CallPlugin()` |

Die Direktmenü-Befehle werden **in der VDR-Hauptschleife** verarbeitet; bloß `osSchedule` oder `osTimers` aus einer `cOsdObject`-Ansicht zurückzugeben, würde diese Menüs nicht öffnen. Der ursprüngliche Suite-Home-Screen bleibt bei dieser Übergabe unverändert; tiefere VDR-Menüs behalten vorerst ihre native Skin-Gestaltung. Die konsistente Rückkehr zu Suite-Home ist Teil der weiteren OSD-/Skin-Integration und ist **noch nicht** auf dem echten VDR getestet.

**Noch offen:** Kontextbezogenes Öffnen einer ausgewählten „Als Nächstes“-Kachel als VDR-Sendungsdetail mit Timeraktion, Senderlogos und die reale TV-/Artwork-Darstellung auf Hardware, Videovorschau als echtes Bild-in-Bild, eine einheitlich gestaltete native Suchtimer-Ansicht und weitere Suite-Addons. Eine allgemeine EPG-Menüöffnung ist ausdrücklich **nicht** gleichbedeutend mit dem ausgewählten Sendungsdetail.

Die VDR-Hauptmenüs, Wiedergabe, Timer und HbbTV bleiben bei ihren bestehenden Eigentümern. Kein Fork von `vaapivideo`, keine zusätzliche Browser-/Playerinstanz und keine neue Autorisierungslogik.

## Entscheidung: lokaler VDR statt Public API v1 für Suchtimer

Das Suite-OSD läuft im selben VDR-Prozess wie die übrigen VDR-Plugins. Daher erfolgt die Suchtimer-Navigation **direkt lokal**, ohne HTTP oder Public API v1. Der Suchtimer-Eigentümer bleibt `epgsearch`; es wird kein eigener Suchalgorithmus und keine zweite Datenhaltung implementiert. `epgsearchonly` bietet, falls installiert, über `Epgsearch-searchmenu-v1.0` den direkten Sucheinstieg. Ohne dieses zusätzliche Plugin wird das reguläre `epgsearch`-Hauptmenü geöffnet; es ist nicht automatisch identisch mit der Suchtimer-Liste. Das genauere direkte Suchtimer-Menü inklusive konsistenter Suite-Navigation benötigt später einen belegten lokalen Plugin-Menüvertrag und einen echten VDR-Test.

Keine Änderung an der Public API v1 der übrigen Clients.

## Native TVScraper-Kacheln (Code-Stand 0.3.0)

- Aufnahmequelle: VDR-`cRecordings` (Titel, Datum, Dateipfad, vorhandene VDR-Beschreibung und Wiedergabe).
- Zusätzliche Daten: installiertes VDR-`tvscraper`-Plugin über `GetScraperVideo`, während der `cRecording`-Zeiger mit `LOCK_RECORDINGS_READ` gültig ist. Serien-/Filmtitel, Episode, Veröffentlichungsdatum, Beschreibung und priorisiertes Bild in Querformat/Banner/Poster werden kopiert. Datei-Bilddekodierung erfolgt **nach Freigabe** des VDR-Locks.
- Bilder: lokal gespeicherte JPEG- und PNG-Dateien, skaliert auf ein kleines natives OSD-Bild ohne HTTP, neue Datenbank oder eigenen Scraper. Fehlende/Beschädigte Dateien fallen auf die normale Kachel zurück.
- Fokus und Info: `Info` auf einer Aufnahmekachel zeigt Metadaten oder VDR-Beschreibung. `OK` bleibt VDR-eigene Wiedergabe.
- Bis zu 48 sichtbare/zuletzt besuchte Artwork-Einträge im temporären OSD-Cache; manuelles Neuladen mit Blau. **Kein** Polling.
- Zur Laufzeit bleiben fehlendes TVScraper-Plugin und ein leerer Metadatensatz unkritisch. Eine echte Darstellung mit Ausgabeplugin auf dem yaVDR ist noch **nicht getestet**. Die CI prüft nur Kompilierung unter 2.8.3/2.8.4 sowie JPEG/PNG-Decoding.

Quellvertrag: [TVScraper Upstream-Service-Header](https://github.com/MarkusEh/vdr-plugin-tvscraper/blob/2e63f40aff747f6dd8786aa8c2c7a4ef65cd5f08/services.h), mit GPL-Provenienz im Repo. Die Runtime-ABI-Kompatibilität eines abweichenden lokal installierten TVScraper-Builds muss später am VDR geprüft werden.

## EPG-Timeline – verbindliche Webfrontend-Vorlage (23.11.2026)

Die Nutzerentscheidung ist **kein neuer EPG-Entwurf**: Die bestehende EPG-Zeitleiste des VDR-Suite-Webfrontends ist die Vorlage. Die Implementierung basiert auf `web/frontend/app.js` (`renderEpgTimeView`, `epgTimelineDisplayBounds`, `epgEventPositionForBounds`) und `web/frontend/style.css` (`epg-time-grid`, `epg-time-row`, `epg-now-line`).

**Aktuell programmiert:** horizontale Zeitachse oben mit Kanalspalte links; zeitlich proportionale Sendungsblöcke; orange Jetzt-Markierung; blau markierte Auswahl; direkt aus VDR stammende Programmtexte; selbstständig nachgeführtes Sender-/Zeitfenster ohne sichtbaren Scrollbalken; optionaler vertikaler Zeitachsenmodus und Live-/Jetzt-/Nächste-Listen; native Detailansicht; Rückkehr zu Suite-Home. Das Frontend zeigt 24 Stunden als sehr breite scrollbare Zeitachse (2-Stunden-Markierungen, 15 Sender pro Seite). Das native TV-OSD nutzt dasselbe 24-Stunden-Paging in **lesbaren Vier-Stunden-Ausschnitten** mit 30-Minuten-Ticks und sieben sichtbaren Sendern. Das ist die Anpassung an Fernbedienung und Betrachtungsabstand, keine neue Designsprache.

**Noch ausstehend:** pixel- und artworkbezogene TV-Abnahme, SVG-Logo-Dekodierung, dynamische Echtzeitupdates sowie die optisch durchgängige VDR-Skin der tieferen Menüs. Lokale PNG-/JPEG-Logos, TVScraper-EPG-Artwork, Timer-Erstellung mit Bestätigung und Kanalgruppenfilter sind implementiert, aber nicht auf dem realen Gerät abgenommen. Die Webfrontend-Funktionen werden nicht als vollständig portiert ausgegeben. Kein HTTP und keine Public API v1 für VDR-eigene EPG-Daten.

## EPG-Vervollständigung 0.5.0 – native Funktionen, keine API v1

- **Senderlogos**: `logo_locator.h` verwendet dieselben nachgewiesenen Dateiverzeichnisse und Namen-/Qualitätsvarianten wie die VDR-Suite-Frontend-Asset-Auslieferung. Unterstützt werden lokal vorhandene PNG-/JPEG-Logos, mit Fallback auf Sendernummer und -name; SVG wird derzeit nicht dekodiert. Der C++-Headless-Test prüft Kandidaten, Varianten, fehlende Dateien und ungültige Dateinamen.
- **EPG-Artwork**: Der `tvscraper`-Service `GetScraperVideo` wird mit dem unter VDR-`LOCK_SCHEDULES_READ` neu aufgelösten `cEvent` aufgerufen. Kein VDR-`cEvent`-Zeiger entkommt dem Lock; nur der lokale Bildpfad wird kopiert, und das Bild wird anschließend ohne Lock dekodiert. Maximal zwölf zuletzt betrachtete Eventbilder werden im RAM gehalten. Ohne Plugin/Bild bleiben Programminformationen unverändert sichtbar.
- **Kanalgruppen**: VDR-Gruppen aus `cChannel::GroupSep()`; Taste `0` wechselt den Filter. Wie im Webfrontend werden alle Sender und die vorhandenen Gruppennamen angeboten. Das lokale `Timeline`-Modell hält die unfiltrierten Daten als Snapshots und kopiert die sichtbare Gruppe. Headless-Regressionstest für Gruppenfolge und Fokus.
- **Timer**: In der EPG-Detailansicht `Rot`/`Record` und anschließendes `OK` zur Bestätigung. Vor dem Schreiben wird das gewählte Ereignis über Sendernummer, Event-ID und Startzeit gegen die **aktuellen** VDR-Schedules geprüft. `cTimers` erstellt den Timer direkt mit den nativen VDR-Methoden, VDR-Timer-Defaults, doppelten Timerprüfungen und `HandleRemoteTimerModifications` bei Peer-Konfiguration. Fehler und Erfolgs-Readback bleiben in der Detailansicht sichtbar. Keine eigene Timertabelle oder HTTP-Schnittstelle.
- **Noch offen**: Echte Timer-Readback-/Remote-Peer- und Bild-/Fernbedienungsabnahme am yaVDR mit Ausgabeplugin sowie reine SVG-Dateien. Erfolgreiche CI-Builds allein belegen keine reale Bildausgabe oder Timeranlage.

## Aufnahmen 2.0 – Fernbedienung und native Preview (24.11.2026)

**Verbindliche Darstellung:** Eigenständige Aufnahmen-Kachelansicht im **VDR-Suite-Webfrontend-Design**. Links die zusammenklappbare Suite-Sidebar; im Hauptbereich Ordner/Genre/Alle als umschaltbare Reiter sowie Cover-Kacheln; oben rechts optional ein kleines Live-TV-Bild. Die Ordneransicht ist der Startmodus. Keine neue Designvariante.

**Daten und Navigation:**
- Aufnahmen werden ausschließlich aus VDR-`cRecordings` gelesen; Unterordner aus dem VDR-Aufnahmenamen, Titel aus `cRecording::BaseName()`. Kein alternativer Index und kein Dateisystem-Scan.
- TVScraper über den lokalen VDR-`GetScraperVideo`-Service: Film-/Serienmetadaten und Genrezuordnung. Mehrere Genres je Aufnahme zulässig; eine Aufnahme wird dabei nicht verschoben oder dupliziert. Ohne TVScraper fällt der Reiter „Genre“ auf „Ohne Genre“ zurück. Genreanfragen werden beim erstmaligen Genreaufruf gemacht und danach pro Dateinamen zwischengespeichert.
- `recording_browser.h` kapselt Ordner-/Genre-/Alle-Filter, Auswahl, Paging und Datum-/Namenssortierung, ohne VDR-Pointer. Ein reiner C++-Test deckt mehrstufige Ordner, mehrere Genres, die Sortierung und das 4×3-Kachelgitter ab.

**Gewohnte Bedienung:** `Rot` von Home öffnet Aufnahmen; `Grün` EPG; `Gelb`/`Info` öffnet die Informationen zur fokussierten Home-Sendung bzw. Aufnahme; `Blau` Timer. In der Aufnahmenansicht: Pfeile/OK, `Rot` VDR-Wiedergabe, `Grün`/`0` Sortierung, `Blau`/`Info` Metadaten und `Gelb` Löschbestätigung. `Zurück` geht zum übergeordneten Ordner/Genre-Wurzel oder ins Home. Falls eine Aufnahme gerade läuft bzw. bearbeitet wird, wird der aufwendige Löschfall durch die native VDR-Aufnahmenverwaltung bearbeitet.

**Live-TV:** `cDevice::CanScaleVideo()` liefert die tatsächliche darstellbare Zielgröße. Bei Unterstützung verkleinert `ScaleVideo()` **das vorhandene** Fernsehbild; darüber liegt ein transparentes Loch im TrueColor-OSD. `ScaleVideo(cRect::Null)` setzt das normale Vollbild wieder ein beim Schließen, beim Wechsel in andere Ansichten und vor Wiedergabe, auch bei Destruktion. Bei fehlender Funktion oder Replay erscheint ein klarer Text-Fallback. Kein Eingriff in das Ausgabeplugin, keine zweite Videoquelle.

**Abnahme noch offen:** VDR-Laufzeitprobe mit echtem TVScraper, Genreauflösung auf größeren Bibliotheken, Lösch-/Replay-Verhalten sowie Fähigkeit des installierten Ausgabeplugins zur Videoverkleinerung. Ein CI-Build gegen VDR-Header allein belegt keine reale Bildausgabe.

## Gemeinsame Einstellungen (0.7.0, 24.11.2026)

Anstelle des früheren direkten Sprungs zu `kSetup` öffnet die Suite-Sidebar jetzt ein Dashboard in derselben dunklen Designsprache. Die sechs Kategorien sind **VDR**, **VDR-Suite**, **Plugins**, **TV-Oberfläche**, **Benutzer & Geräte** und **System**. VDR und Plugins bleiben in ihren Originalmenüs; Suite-Core-/Agent-Daten behalten ihre vorhandene Autorität. Die lokale Mini-Live-TV-Vorschau wird als erste echte Suite-OSD-Option mit `SetupStore` im VDR-Plugin-Namespace gespeichert. Die Bereiche Suite-Core und Benutzer/Geräte sind vorerst bewusst als Informationen statt falscher editierbarer Felder dargestellt. Die nächste Integrationsstufe muss zunächst reale lokale Core-/Agent-Verträge nachweisen. Details und Abnahme: [Settings-Vertrag](settings-integration.md).

## Core-Verwaltung 0.8.0

Ergänzend zum vereinheitlichten VDR- und Suite-Menü bietet die native TV-Oberfläche einen vorhandenen Human-Account-Login mit verdecktem Passwortfeld. Backend-/Media-Tools-, Konto- und Device-Aktionen verwenden die bestehenden Core-HTTP-Owner und dessen CSRF-/ETag-/Grant-Verträge; VDR-eigene EPG-, Timer- und Aufnahmedaten bleiben weiterhin lokal. Diese Erweiterung ändert die freigegebene Gestaltung nicht. Noch nicht umgesetzt sind vollständige Konto-/Device-Grants, Kopplungsfreigaben und beliebige weitere Backend-/Addon-Einstellungen. Details: [Core-Verwaltung](settings-core-administration.md).

## Suite-Skin 0.9.0: OK / Info bei Live-TV und Wiedergabe (24.11.2026)

Die mit dem Nutzer abgestimmten Mockups führen zu drei getrennten nativen Bedienflächen:

1. **OK im Live-TV**: Die VDR-`cSkinDisplayChannel`-Einblendung zeigt den Sender, die aktuelle und folgende Sendung samt Uhrzeiten, Programmfortschritt, Metadaten und lokaler TVScraper-Fanart. Das TV-Bild bleibt Eigentum des Ausgabeplugins.
2. **OK bei laufender Aufnahme**: `cSkinDisplayReplay` zeichnet eine untere VDR-Suite-Wiedergabeleiste mit Titel, Position/Gesamtdauer, Wiedergabemodus, Metadaten und Fanart. Erneutes OK wird wie gewohnt vom VDR-Replay-Control verarbeitet.
3. **Info bei einer Suite-Aufnahme**: Im Suite-Aufnahmenbrowser oder Home öffnet die Auswahl eine vollflächige native Detailansicht mit Fanart, Poster und TVScraper-Metadaten. Bei Wiedergaben, die aus diesen Suite-Menüs gestartet wurden, verwendet die erweiterte VDR-`cReplayControl`-Klasse deren `GetInfo()`-Einstieg, um die entsprechende Infoansicht während des Abspielens zu öffnen.

Alle Bilddateien werden nur aus dem lokalen TVScraper-Cache gelesen. Pfade und Metadaten werden aus dem VDR-Service kopiert, solange die VDR-Quellzeiger gültig sind; JPEG-/PNG-Dekodierung geschieht über den vorhandenen Artwork-Decoder. Fehlende Fanart verhindert weder Senderinformation noch Wiedergabe. **Kein neuer Streamplayer, HTTP-EPG-Client oder API-v1-Zwang.**

**Offene Abnahme:** Die neue Skin muss über VDRs OSD-Einstellungen ausgewählt werden; sie wird nicht ungefragt als Standard gesetzt. Native VDR-Menüs, Lautstärke, Audio/Spuren und Meldungen fallen derzeit auf eine vorhandene VDR-Skin zurück. TVScraper-ABI, Fanart-Darstellung, VDR-Rückkehr, Tastennavigation und Ausgabeplugin werden erst auf realem yaVDR abgenommen; ein grüner CI-Build ist hierfür kein Ersatz.


## Korrektur nach TV-Abnahme (26.11.2026)

Home nutzt denselben nativen ScaleVideo-Pfad wie der Aufnahmenbrowser: oben rechts ein 16:9-Fenster, keine zweite Videoquelle, und Rücksetzen beim Wechsel in Details/andere Ansichten sowie beim Schließen. Beide Ansichten respektieren die vorhandene LivePreview-Einstellung. Logos werden mit einer opaken dunklen Unterlage alpha-komponiert; der Home-Hintergrund und die Kanalinfo-Leiste sind deckend. Ein TV-Test der Home-Geometrie und der Rückkehr zur Vollbildausgabe bleibt erforderlich.

## TV-Home 0.9.7 (26.11.2026)

Nutzerreferenz: sieben mobile Webfrontend-Screenshots (Home, Jetzt/Danach, Weiterschauen, Serien, Genres/Ordner, Neu aufgenommen, Empfehlungen, zuletzt angesehen). Quellvorlage `hotzenplotz5/vdr-suite@52c67da2c74a2757785ded3eb3bccecc703756ce`, `web/frontend/home-live-hero.js` und `home-recording-discovery.js`. Die Palette übernimmt dunkle Flächen, helle Logo-Matten, Weiß und sparsame blaue Akzente. Native Linienicons, vertikal fokussierte Reihen mit Portrait-Artwork, native TV-Vorschau, neueste Aufnahmen und Timer aus dem gewählten Event. Die Daten stammen aus dem bestehenden lokalen VDR/TVScraper, nicht einer zweiten Datenbank. Die restlichen Discovery-/History-Reihen benötigen die bestehenden Core-Owner und werden nicht durch erfundene Daten ersetzt. Der native klassische Menüeintrag bleibt unabhängig vom Remapping erreichbar. Hardwareabnahme offen.

### Standard-Hauptmenü ohne VDR-Patch

VDR 2.8.3 verarbeitet `kMenu` vor Plugin-`ProcessKey`; `User0` bis `User9` werden dagegen über `KeyMacros` verarbeitet und `@suiteosd` öffnet `MainMenuAction`. Eine freie User-Taste in `keymacros.conf` auf `@suiteosd` legen und die physische Menu-Zuordnung in `remote.conf` auf genau diese User-Taste umbenennen. Keine bestehende fremde User-Taste überschreiben; Originaldateien vor dem Wechsel sichern. Der neue Sidebar-Eintrag **VDR-Menü** stellt den klassischen Einstieg über den unveränderten `kMenu` bereit. Zurück schließt Home; erneutes physisches Menu öffnet Home neu. Die Änderung ist eine Host-Konfiguration, keine automatische Paketmutation.

## Hardware-Feedback 26.11.2026

Die native Video-Geometrie des Aufnahmenbrowsers war auf dem TV erfolgreich. Home und Sammlungen nutzen nun dieselbe rechte Vorschaufläche und halten sie auch beim Wechsel der Reihen. Die linke Fläche zeigt fünf Karten; der Browser nutzt zwei statt drei Reihen mit höherem Posterbereich. Die Anmelde-/Genre-/Ordner-Reihen belegen weniger Höhe. Die interaktive Detailseite markiert den Einstieg in die Cast-Navigation explizit. Die passive VDR-Aufnahmeinfo kann weiterhin keine eigenen Tasten verarbeiten. Cover und manuelle Core-Metadatenzuordnungen sind noch nicht vollständig portiert. Keine Änderungen an Menü-Tastenbelegung, Ausgabeplugin oder Boot-Konfiguration. Hardwaretest ausstehend.

## Freigegebener Cinematic-Home-Entwurf (26.11.2026)

Explizit bestätigte letzte TV-Ansicht: 16:9, dunkles Blau, gleich große 2:3-Portrait-Kacheln (auch im Fokus), sechs Spalten und drei sichtbare Reihen. Jetzt, Als nächstes, Neu aufgenommen zuerst, weitere vorhandene Reihen vertikal navigierbar. Dezente abgerundete Rahmen/Schatten, cyanfarbener Fokus ohne Größenänderung; keine Senderlogos auf Aufnahmen. Die einklappbare Sidebar bleibt erhalten. Native Live-TV-Vorschau oben rechts zeigt ausschließlich den eingeschalteten Sender; darunter eine separate Fokuskarte mit TVScraper-Landscape-Fanart, Titel, Sender, Sendezeit, Gesamtdauer, Restzeit bei laufenden Ereignissen, Kurzbeschreibung. Fehlende Daten nicht erfinden. Fokusmetadaten werden erst nach kurzem Verweilen über vorhandene VDR-/TVScraper-Schnittstellen geladen, begrenzt im RAM gehalten; keine API-/Playeränderung. Hardwareabnahme noch offen.

Die Poster werden proportional mittig beschnitten und im Cache mit einem opaken unteren Textverlauf vorbereitet; keine Verzerrung und keine transparenten Bildbereiche zur Live-Video-Ebene. Restzeit und Live-Fortschritt basieren auf dem lokalen Zeitwert und den VDR-Ereignisgrenzen; die Minutenanzeige wird im nativen OSD-Leerlauf aktualisiert. Navigation, Seitengröße und abweichende Kernfunktionen bleiben beim jeweiligen vorhandenen Owner.

## Größere Home-Kacheln (26.11.2026)

Auf Maintainer-Wunsch fünf gleich große 2:3-Kacheln je Reihe statt sechs (160 × 240 statt 132 × 198 logische Einheiten); horizontaler Abstand 20 statt 36, bei ausgeklappter Sidebar weiterhin 14. Zwei vollständige Reihen sind gleichzeitig sichtbar; alle weiteren bleiben vertikal per Fernbedienung erreichbar. Die native Live-TV-/Fokusfläche und die Bildproportionen bleiben unverändert. Der echte TV-Test steht aus.

## Drei Reihen mit weichem Scrollen (27.11.2026)

Neuere Maintainer-Freigabe ersetzt die Zwei-Reihen-Geometrie: fünf gleich große 2:3-Poster (140 × 210 logische Einheiten) in genau drei vollständig sichtbaren Rails. Der vertikale Ausschnitt reicht von 82 bis 850 der 900 logischen OSD-Einheiten und skaliert mit dem realen OSD. Erst beim Verlassen des sichtbaren Ausschnitts wird eine weitere Reihe mit einer 240-ms-Smoothstep-Animation über einen nativen Pixmap-DrawPort eingefahren. Kopfzeile, Sidebar, Live-TV und Detailpanel bleiben stehen. Bei fehlender Pixmap-Unterstützung bleibt die direkte Navigation verfügbar. Daten, Reihenfolge, horizontales Paging und Aktionen bleiben unverändert. Hardware-/Frameratenabnahme offen.

## Home-Feedback 27.11.2026 – Live-TV-Hero und sechs Poster

Die erste scrollbare Reihe ist Live-TV mit dem vorherigen und nächsten VDR-Sender, mittig dem tatsächlich laufenden Bild sowie Jetzt/Danach. Links/Rechts fokussiert die drei Karten; OK auf einem Nachbarsender schaltet nativ um, OK in der Mitte kehrt ins Vollbild zurück. Beim Scrollen aus der ersten Reihe wird die native Videoebene zurückgesetzt; drei Poster-Rails bleiben sichtbar. Rechts bleibt die fokussierte Detailkarte. Sechs gleich große 2:3-Poster (136 × 204, Überschrift 38 logische Einheiten) im normalen Zustand, fünf bei ausgeklappter Sidebar (kein Überdecken der Detailkarte). Sichtbare Public-v1-Aufnahmen laden Metadatendetail und bevorzugtes Artwork asynchron ohne OK; vorhandener lokaler TVScraper nur nach exakter nativer Identitätsauflösung. Keine neue API oder Berechtigung. Hardware-/Performanceabnahme offen.
