# Suite-OSD – Architekturentwurf (OSD.0)

Stand: 2026-11-23 · Status: **Entwurf und Integrationsziel**, keine Laufzeit-Abnahme.

## Ziel und Nichtziele

Ein lokales, mit Fernbedienung bedienbares VDR-Suite-Frontend für alle tatsächlich verfügbaren Suite-Funktionen und Addons. Das Plugin ist nicht auf ein bestimmtes Videoausgabeplugin beschränkt. Es darf `vaapivideo` nicht forken, patchen oder dessen Geräte- und Ausgabeverwaltung übernehmen.

**Nichtziel:** vorhandene HTML-Seiten 1:1 im VDR-OSD ausführen, einen eigenen Decoder/Browser/Transcoder implementieren, VDR-Suite-Backend oder HbbTV-Komponenten extrahieren, neue Authentisierungs- und Aktivierungsregeln einführen.

## Zuständigkeiten

| Komponente | Zuständigkeit | Nicht zuständig für |
| --- | --- | --- |
| VDR | Kanal-/Timer-/Aufnahmesteuerung, Plugin-Lebenszyklus, Fernbedienungsereignisse, OSD-API | Suite-spezifisches Addon-Geschäftsmodell |
| `vdr-plugin-suiteosd` | TV-Navigation, Skin, Menüs, OSD-State, Aufruf bestehender Funktionen/Plugins | Speicherung und Entscheidung über Produktivrechte |
| VDR-Suite Core / Public API v1 | Daten, Aktionen, Rechte, backendgebundene Operationen, wiederverwendbare Domänenservices | HDMI-Rendering |
| VDR-SuiteBridge / vorhandene Plugins | Bestehende VDR-Integration und bereits implementierte Transport-/Session-Verträge | Neues paralleles Suite-Backend |
| `vaapivideo` oder anderes Ausgabeplugin | Video, Audio, OSD-Ausgabe auf HDMI | Suite-Menü- oder Addon-Logik |
| `vdr-plugin-web` + `cefbrowser` + `remotetranscode` | Bestehende HbbTV-Sitzung und Wiedergabe | Suite-eigene Browser-Neuimplementierung |

Die Planung führt **keine neue Public-API-Route, Authentisierungsmethode oder generische C++-Addon-ABI** ein. Erweiterungen müssen sich an den tatsächlichen vorhandenen APIs und der VDR-API orientieren; die dazu nötige Paritätsanalyse findet vor dem jeweiligen Slice statt.

## Anbindung und Ablauf

```text
IR/Fernbedienung -> VDR key events
                  -> suiteosd (cOsdMenu / cOsdObject, später cSkin)
                     |         |
                     |         +--> bestehendes VDR-Plugin-Menü / native Plugin-Interaktion
                     +--> bestehende SuiteBridge / VDR-Suite Public API v1
                                              |
                                     Domain / Grants / Backend-Agent
OSD frame -> VDR cOsdProvider -> vaapivideo oder anderes Ausgabeplugin -> HDMI
```

**Skin ≠ ganze Anwendung:** Eine `cSkin`-Implementierung kann Standard-VDR-Oberflächen zeichnen. Suite-Startbildschirm, Addon-Ansichten, Auswahl-/Navigationsmodelle und die Übergabe an fremde Plugins benötigen zusätzlich eigene Menüs/OSD-Objekte und bewusste Zustandsübergänge.

**Ausgabeplugin-Unabhängigkeit:** Keine Includes, Bibliotheken, gerätespezifischen Patches oder internen Methoden aus `vaapivideo`. Die tatsächlichen Darstellungsmöglichkeiten (TrueColor, Auflösung, Bilder) sind zur Laufzeit über VDR zu bestimmen. Bei einem schwächeren OSD-Provider muss die Kernbedienbarkeit erhalten bleiben; nicht unterstützte Effekte werden weggelassen.

## Medien und OSD-Handoff

- **Live-TV/EPG/Timer/Aufnahmen:** Suite-Daten und Aktionen über bereits etablierte Besitzer, native OSD-Navigation. Bestehendes Verhalten des Webfrontends ist Paritätsreferenz.
- **Teletext:** Bestehendes Teletext-Plugin verwenden, keine zweite Teletext-Engine bauen. Exakte Plugin-ID, Menü-/Service-Aufruf und Zurück-Verhalten gegen installierte Version nachweisen.
- **HbbTV:** Bestehende Kette `vdr-plugin-web` / `cefbrowser` / `remotetranscode` beibehalten. Ein Start aus Suite-OSD benötigt einen belegten Plugin-/Suite-Aufruf und klare Übergabe von Fokus, Farbtasten/Zurück, Browserbild und Medienausgabe. Kein Versprechen paralleler OSD-Renderer oder direkter Einbettung eines CEF-Fensters. Nach Ende Fokus/Live-TV zurückgeben. Falls das Ausgabeplugin für OSD-/Videohandoff spezielle Grenzen setzt, müssen diese gesondert am realen VDR getestet werden.
- **Image-Galerie:** Metadaten/Dateilisten aus dem künftigen Image-Addon; Decoder/Rendering nur über tatsächlich verfügbare native Bilddarstellung. Stand heute ist das **ein noch zu implementierendes** TV-Feature, nicht bereits in vaapivideo enthalten.
- **Media Tools:** Verwaltung, Konvertierung, Import-Status und Fortschritt über den bestehenden Core/Agent; Hintergrundverarbeitung bleibt serverseitig. Für lokale Wiedergabe eines Films wird erst nach Geräte- und Stream-/Datei-Test die geeignete bestehende Player-Schnittstelle gewählt. Media Tools und Media-Player sind unterschiedliche Verantwortungen.
- **Suchtimer und weitere Addons:** Bestehende Public-v1-Funktionen und Rechte einsetzen, ohne neue zweite Implementierung. Eigene TV-Ansichten schrittweise ergänzen; noch fehlende APIs werden als konkrete Paritätslücken dokumentiert.

## Sitzungen, Rechte und Ereignisse

- Jede mutierende Aktion bleibt bei ihrem existierenden autoritativen VDR-/Suite-Service, mit vorhandener Autorisierung und Ergebnisprüfung. Ein Menüeintrag oder Addon-Metadatum gewährt niemals Rechte. Keine stillen zusätzlichen Gates.
- Das Plugin wird nicht selbst zu einem unbegrenzt berechtigten API-Superuser. Der passende lokale Identitäts-/Autoritätsvertrag ist **vor produktiven Suite-Schreibaktionen** anhand vorhandener Implementierung festzulegen.
- Bei asynchronen Suite-Aktionen Push/SSE vor Polling. Ein initialer Snapshot und ein Reconnect-Readback dürfen sinnvoll sein; keine Status-Polling-Schleifen ohne Freigabe.
- Bei fehlender Suite-Verbindung native VDR-Funktionen weiterhin bedienen, zuletzt erfolgreiche reine Lesedaten nicht ungefragt vernichten und echte Mutationsfehler pro Aktion anzeigen.
- Plugin-Lebenszyklus und Sperrgrenzen: Menü öffnen, verlassen, Plugin wechseln, Wiedergabe starten/stoppen, VDR beenden; dabei keine alten Handles/Fernbedienungsfoki oder unverwalteten Threads hinterlassen.

## Nachweisbare Integrations- und Paritätsmatrix vor jedem Addon

Für jede Funktion die folgende Matrix gegen den **tatsächlich funktionierenden** VDR-Suite-Webfrontend- und VDR-Pfad ausfüllen: Eingaben, Ergebnisanzahl, Sortierung, Metadaten/Titel, Pagination, Fehlerzustände, Grants, Live-Integration und Bedienpfad. Ein TV-Client darf nicht eine neue unabhängige Suche oder Aufnahmen-Sortierung erfinden.

| Teil | Erwartetes Ziel | Heute |
| --- | --- | --- |
| VDR-Menüstart | Menu öffnet, Back kehrt zum vorherigen Fokus zurück | Nicht getestet |
| EPG / Timer / Aufnahmen | Parität von Kanälen, Rechten, Sortierung und Aktionen | Nicht implementiert |
| Suchtimer | Public-v1-Parität und vorhandene Grants | Nicht implementiert |
| Teletext | vorhandenes Plugin inkl. Farb-/Zurück-Tasten | Nicht integriert |
| HbbTV | Start, Browser-Bild, Videos, Eingaben, Ende/TV-Rückkehr | Nicht integriert |
| Image | Liste, Bildanzeige, Galerie, Diashow | Nicht implementiert |
| Media Tools | Status/Aktionen, Langläufer-Rückmeldung | Nicht integriert |
| Videoplayer | Datei/Stream, Pause/Seek, Audio/Untertitel, TV-Rückkehr | Nicht integriert |

**Vergleich zu Konkurrenz:** Kodi und Plex dienen später als UX-Referenz für 10-Foot-Navigation, Suche und Medienbibliothek. Aktuelle Feature-Parität ist **nicht geprüft**. Kein fremdes Produkt rechtfertigt eine ungefragte Funktionsausweitung.

## Offene technische Prüfungen

1. Installierte VDR-API-Version, vorhandene OSD-Provider und Geräteausgabe.
2. Tatsächlich verwendbare VDR-Plugin-Aufrufe für Teletext/HbbTV und deren OSD-Ownership.
3. Bereits existierende SuiteBridge-/API-v1-Transportpfade für einen lokalen VDR-Client inkl. Identität und Push.
4. Native Bilder und Videos: OSD/Player-Funktionen des eingesetzten Ausgabeplugins und Rückkehr zum TV.
5. Reale Abnahme mit yaVDR und kompatiblem VDR-Plugin-Build; reine GitHub-Commits oder Quellcode-Tests sind kein Hardware-Nachweis.

## Gültige Suite-Quellen

Geprüfte Projektbasis: `hotzenplotz5/vdr-suite` `main@2142c8138008b020508f5acd43dba8cba1cb2985`, `AGENTS.md` Blob `55555d95e64159c62dd2b2a7d5c0f56834dc1529`.

- [Suite Addon-Paketierung](https://github.com/hotzenplotz5/vdr-suite/blob/2142c8138008b020508f5acd43dba8cba1cb2985/docs/development/addon-source-packaging.md)
- [Suite installierte Addon-Registry](https://github.com/hotzenplotz5/vdr-suite/blob/2142c8138008b020508f5acd43dba8cba1cb2985/docs/development/addon-installed-registry-contract.md)
- [Zurückgestellte HbbTV-Extraktion](https://github.com/hotzenplotz5/vdr-suite/blob/2142c8138008b020508f5acd43dba8cba1cb2985/docs/planning/hbbtv-addon-extraction-roadmap.md)

Das Suite-OSD ist **keine Reaktivierung** der zurückgestellten HbbTV-Addon-Extraktion.
