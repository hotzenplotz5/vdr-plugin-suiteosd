# Suite-OSD Addon-Präsentation – Vertragsentwurf

Status: **Konzept**, noch keine implementierte öffentliche API. Ziel ist, alle tatsächlich installierten und nutzbaren Suite-Addons im nativen TV-Frontend anzuzeigen, ohne dass Addons unabhängig voneinander ein eigenes VDR-Hauptmenü und einen eigenen Berechtigungsmechanismus bauen müssen.

## Bestehendes vs. geplantes System

VDR-Suite besitzt bereits Quellmanifest-, Paketierungs- und Installations-Registry-Konzepte. Diese Metadaten bedeuten **nicht automatisch**, dass ein Addon aktiviert, verwendbar, autorisiert oder mit TV-UI ausgestattet ist. Deshalb werden aus einem Manifest **keine** ausführbaren Plugin-Befehle, URL-Pfade oder Benutzerrechte übernommen.

Die folgende Präsentationsbeschreibung ist zunächst nur ein **internes OSD-UI-Modell** (keine neue Suite-API, kein VDR-Service-Protokoll):

| Attribut | Zweck |
| --- | --- |
| `id` | stabile, vorhandene Addon-/Funktionskennung |
| `title` | lokalisierbare, kurze Menübezeichnung |
| `entry` | bekannter lokaler Einstieg: native Suite-Ansicht oder vorhandener VDR-Plugin-Aufruf |
| `kind` | `native`, `vdr-plugin`, `playback` |
| `capability` | tatsächlicher durch Core/VDR ermittelter Funktionsstatus; kein selbst erteiltes Recht |
| `state` | sichtbar, fehlend, aktiv, beschäftigt oder Fehler; konkrete Quelle dokumentieren |
| `navigation` | Fokus/Zurück und Remote-Key-Handoff |

Kein Feld enthält ein aus einem Addon-Manifest frei ausführbares Shell-Kommando. Die konkrete Datenquelle und ihre Feldnamen sind später aus den **bestehenden** Suite- und VDR-Verträgen abzuleiten, nicht durch Erfinden eines `/api/v1/addons/ui`-Endpunkts.

## Einbindungstypen

1. **Native TV-Ansicht**: Suite-OSD rendert lokale VDR-Daten und Navigation. **VDR** bleibt Besitzer von EPG, Timern, Aufnahmen und Wiedergabe; **Suite-Core/Agent** bleibt Besitzer ausschließlich der Suite-spezifischen Addon-Aktionen, etwa Media Tools und Image.
2. **Handoff an vorhandenes VDR-Plugin**: Vorhandene VDR-Plugin-Menüs direkt öffnen. Beispiele: `epgsearch`/optional `epgsearchonly` für Suchtimer, `osdteletext` für Teletext und `web` für HbbTV. Keine zweite Suchtimer-/Teletext-/Browser-Engine und kein Public-API-v1-Umweg.
3. **Handoff an Wiedergabe**: Auswahl im Suite-OSD, Start über eine belegte VDR-/Ausgabeplugin-Schnittstelle. Playback-Owner ist vom Media-Tools-Worker getrennt.

## Fernbedienung und Lifecycle

- **OK**: markierte Aktion; **Zurück**: innerhalb der Ansicht zurück, aus einem fremden Plugin zurück ins Suite-OSD; **Rot/Grün/Gelb/Blau**: pro Ansicht dokumentierte Aktion, bei HbbTV/Teletext an deren aktiven Eigentümer; **Home/Menu**: Rückkehr ins Suite-Hauptmenü, soweit vom aktiven VDR-Plugin tatsächlich unterstützt.
- Kein paralleles Handling desselben Key-Events durch zwei Fenster-/Browser-/Menü-Eigentümer.
- Beim Plugin-/Browser-Ende den zuletzt gültigen TV-Fokus wiederherstellen; Start/Stop muss mehrfach funktionieren, ohne verbliebene Menüs/Threads.
- Platzhalter-Elemente dürfen keine scheinbar produktive Aktion vortäuschen; noch nicht implementierte Addon-Ansichten werden als solche gekennzeichnet.

## Kerninvarianten

- Keine Duplizierung von Suite-Rechten, Datenbanken, Aufnahmeoperationen, Backend-Agent-Jobs oder HbbTV-Sitzungen.
- Keine neuen allgemeinen Aktivierungs-/Authentisierungssperren ohne explizite Maintainer-Genehmigung.
- VDR und seine Plugins bleiben Owner ihrer nativen Aktionen und Rechte (EPG, Timer, Aufnahmen, `epgsearch`-Suchtimer, Teletext, HbbTV). Suite-Core bleibt Owner **nur** der Suite-spezifischen Addon-Aktionen und deren bereits vorhandener Autorisierung. Keine zusätzliche API-v1-Abhängigkeit für lokale VDR-Funktionen.
- Fehler einer externen Suite-Verbindung dürfen das vorhandene VDR-Hauptmenü nicht unbenutzbar machen.
- Für langlaufende Vorgänge den vorhandenen SSE/Push-Kanal bevorzugen; bestätigte Zustände nach Reconnect einmal lesen.

## Definition of Done pro Addon

1. Feature im Suite-Hauptmenü auffindbar, ohne vorhandene Buttons zu verstecken.
2. Reale Navigation mit OK, Back, Farbtasten und Wechsel zwischen TV und Addon getestet.
3. Daten, Berechtigungen, Sortierung, Metadaten und Fehlerverhalten mit vorhandenem Webfrontend/VDR verglichen.
4. Mutierende Operationen laufen ausschließlich über die bestehende VDR-/Plugin- beziehungsweise lokale Suite-Addon-Schnittstelle ihres Eigentümers. Für Suite-eigene asynchrone Aktionen bestätigten Readback und gegebenenfalls Push verwenden.
5. HbbTV/Teletext/Player sind nach Ende sauber zum Suite-OSD oder Live-TV zurückgekehrt.
6. Testnachweis enthält genaue VDR-API, Ausgabegerät, Suite-Build und Plugin-Commit.

Bevor dieses interne Modell in einen öffentlichen, versionsgebundenen API-Vertrag überführt wird, erfolgt die Prüfung der tatsächlich implementierten Suite Addon-Verträge. Das Dokument allein erlaubt **keine neue generische öffentliche API oder Plugin-ABI**.
