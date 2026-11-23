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
  3. **Aufnahmen** – Kacheln mit Aufnahmetiteln und zukünftig Artwork;
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
| EPG | VDR-Direktmenübefehl `kSchedule` |
| Aufnahmen | VDR-Direktmenübefehl `kRecordings` |
| Timer | VDR-Direktmenübefehl `kTimers` |
| Einstellungen | VDR-Direktmenübefehl `kSetup` |
| HbbTV | Vorhandenes `web`-Plugin, `cRemote::CallPlugin()` |
| Teletext | Vorhandenes `osdteletext`-Plugin, `cRemote::CallPlugin()` |
| Suchtimer | Lokales `epgsearchonly` (direktes Suchmenü) oder `epgsearch` (Hauptmenü), `cRemote::CallPlugin()` |

Die Direktmenü-Befehle werden **in der VDR-Hauptschleife** verarbeitet; bloß `osSchedule` oder `osTimers` aus einer `cOsdObject`-Ansicht zurückzugeben, würde diese Menüs nicht öffnen. Der ursprüngliche Suite-Home-Screen bleibt bei dieser Übergabe unverändert; tiefere VDR-Menüs behalten vorerst ihre native Skin-Gestaltung. Die konsistente Rückkehr zu Suite-Home ist Teil der weiteren OSD-/Skin-Integration und ist **noch nicht** auf dem echten VDR getestet.

**Noch offen:** Kontextbezogenes Öffnen einer ausgewählten „Als Nächstes“-Kachel als VDR-Sendungsdetail mit Timeraktion, Senderlogo- und Aufnahme-Artwork, Videovorschau als echtes Bild-in-Bild, eine einheitlich gestaltete native Suchtimer-Ansicht und weitere Suite-Addons. Eine allgemeine EPG-Menüöffnung ist ausdrücklich **nicht** gleichbedeutend mit dem ausgewählten Sendungsdetail.

Die VDR-Hauptmenüs, Wiedergabe, Timer und HbbTV bleiben bei ihren bestehenden Eigentümern. Kein Fork von `vaapivideo`, keine zusätzliche Browser-/Playerinstanz und keine neue Autorisierungslogik.

## Entscheidung: lokaler VDR statt Public API v1 für Suchtimer

Das Suite-OSD läuft im selben VDR-Prozess wie die übrigen VDR-Plugins. Daher erfolgt die Suchtimer-Navigation **direkt lokal**, ohne HTTP oder Public API v1. Der Suchtimer-Eigentümer bleibt `epgsearch`; es wird kein eigener Suchalgorithmus und keine zweite Datenhaltung implementiert. `epgsearchonly` bietet, falls installiert, über `Epgsearch-searchmenu-v1.0` den direkten Sucheinstieg. Ohne dieses zusätzliche Plugin wird das reguläre `epgsearch`-Hauptmenü geöffnet; es ist nicht automatisch identisch mit der Suchtimer-Liste. Das genauere direkte Suchtimer-Menü inklusive konsistenter Suite-Navigation benötigt später einen belegten lokalen Plugin-Menüvertrag und einen echten VDR-Test.

Keine Änderung an der Public API v1 der übrigen Clients.
