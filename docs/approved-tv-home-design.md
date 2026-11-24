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
| Einstellungen | VDR-Direktmenübefehl `kSetup` |
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
