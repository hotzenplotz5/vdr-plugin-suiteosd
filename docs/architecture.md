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
| VDR-Suite Core / Agent / vorhandene lokale Dienste | Suite-spezifische Addon-Funktionen (z. B. Media Tools, Image), dort wo VDR keine native Schnittstelle bietet | VDR-Kanäle, EPG, Timer und epgsearch-Suchtimer |
| VDR-SuiteBridge / vorhandene Plugins | Bestehende VDR-Integration und bereits implementierte Transport-/Session-Verträge | Neues paralleles Suite-Backend |
| `vaapivideo` oder anderes Ausgabeplugin | Video, Audio, OSD-Ausgabe auf HDMI | Suite-Menü- oder Addon-Logik |
| `vdr-plugin-web` + `cefbrowser` + `remotetranscode` | Bestehende HbbTV-Sitzung und Wiedergabe | Suite-eigene Browser-Neuimplementierung |

Die Planung führt **keine neue Public-API-Route, Authentisierungsmethode oder generische C++-Addon-ABI** ein. Erweiterungen müssen sich an den tatsächlichen vorhandenen APIs und der VDR-API orientieren; die dazu nötige Paritätsanalyse findet vor dem jeweiligen Slice statt.

## Anbindung und Ablauf

```text
IR/Fernbedienung -> VDR key events
                  -> suiteosd (cOsdMenu / cOsdObject, später cSkin)
                     |         |
                     |         +--> VDR-Kern direkt (Kanäle, EPG, Timer, Aufnahmen)
                     |         +--> lokale VDR-Plugins (epgsearch, Teletext, HbbTV)
                     +--> lokale Suite-Addon-Dienste nur bei Suite-eigenen Features
                                              |
                                    Suite Core / Backend-Agent
OSD frame -> VDR cOsdProvider -> vaapivideo oder anderes Ausgabeplugin -> HDMI
```

**Skin ≠ ganze Anwendung:** Eine `cSkin`-Implementierung kann Standard-VDR-Oberflächen zeichnen. Suite-Startbildschirm, Addon-Ansichten, Auswahl-/Navigationsmodelle und die Übergabe an fremde Plugins benötigen zusätzlich eigene Menüs/OSD-Objekte und bewusste Zustandsübergänge.

**Ausgabeplugin-Unabhängigkeit:** Keine Includes, Bibliotheken, gerätespezifischen Patches oder internen Methoden aus `vaapivideo`. Die tatsächlichen Darstellungsmöglichkeiten (TrueColor, Auflösung, Bilder) sind zur Laufzeit über VDR zu bestimmen. Bei einem schwächeren OSD-Provider muss die Kernbedienbarkeit erhalten bleiben; nicht unterstützte Effekte werden weggelassen.

## Renderwege: nativer Standard und möglicher GPU-Pfad (25.11.2026)

**Verbindliche Entscheidung:** Die native VDR-OSD-API bleibt der produktive Standard und der langfristig erhaltene Rückfallweg. Sie darf durch eine mögliche spätere GPU-/Web-UI-Darstellung weder ersetzt noch zur zweiten Wahl mit reduzierter Bedienbarkeit degradiert werden. Das Suite-OSD muss ohne CEF, DMA-BUF-Import oder spezielle Ausgabeplugin-Patches nutzbar bleiben.

**Standard / dauerhaft unterstützter Fallback (bestehende Architektur):**

```text
VDR-Suite / suiteosd (native Menüs, cSkin, cOsdObject)
    -> VDR-OSD-API (TrueColor-/ARGB-Flächen, sofern unterstützt)
    -> aktiver VDR-OSD-Provider / Ausgabeplugin
    -> gerätespezifische Ausgabe und Bildkomposition -> TV
```

Die im Ideendiagramm genannte Abfolge „ARGB Bitmap → vaapivideo → DRM Overlay“ ist **schematisch**: Ob und wie ein konkretes Ausgabeplugin das OSD per DRM-Plane, GPU-Komposition oder anderem Mechanismus ausgibt, ist eine Implementierungs- und Hardwarefrage. Daraus ergibt sich **kein** verifizierter generischer DRM-Overlay-Vertrag. Der bestehende TrueColor-Fallback auf andere VDR-Skins bei nicht unterstützten 32-Bit-Flächen bleibt unabhängig davon erhalten.

**Optionale zukünftige Alternative, ausdrücklich nicht implementiert:**

```text
CEF / andere Web-UI (optionaler Renderer)
    -> GPU-Textur bzw. DMA-BUF (zu spezifizierender Übergabevertrag)
    -> dafür nachweislich kompatibler Video-/OSD-Renderer
    -> Bildkomposition / TV
       \-> bei fehlender Unterstützung oder Fehler: natives VDR-OSD
```

Eine direkte Übergabe von GPU-Texturen oder DMA-BUF an `vaapivideo` ist **weder nachgewiesen noch Bestandteil** der Suite-OSD-Integration. Möglicherweise wären Erweiterungen oder ein separater Kompositionspfad nötig; daraus entsteht kein Auftrag zum Fork oder Patch von `vaapivideo`. Auch ein möglicher Geschwindigkeitsgewinn oder flüssigere Animationen ist derzeit nur eine Hypothese, keine Messung. Die bestehende HbbTV-/CEFBrowser-Kette wird dadurch nicht verändert.

**Voraussetzungen vor einer späteren Entscheidung:** Konkrete Schnittstellen- und Ownership-Prüfung (Textur-/DMA-BUF-Import, Synchronisation, Format/Stride/Modifier, Alpha/Scaling, Lifetime), Fernbedienungsfokus, Rückkehr zu Live-TV, Hardwarekompatibilität sowie vergleichende Messungen von Framerate, CPU-/GPU-Last und Latenz. Erst wenn ein optionaler GPU-Pfad ausdrücklich beauftragt und am Zielsystem nachgewiesen ist, könnte eine konfigurierbare Auswahl mit automatischem Rückfall auf das native OSD entstehen. Keine Pflichtabhängigkeit für andere VDR-Skins oder Ausgabeplugins.

## Medien und OSD-Handoff

- **Live-TV/EPG/Timer/Aufnahmen:** Direkt aus VDR über die vorhandenen Kern-Schnittstellen und Menü-/Wiedergabemechanismen; kein Public-API-v1-Umweg. Das Webfrontend bleibt gestalterische und funktionale Paritätsreferenz.
- **Teletext:** Bestehendes Teletext-Plugin verwenden, keine zweite Teletext-Engine bauen. Exakte Plugin-ID, Menü-/Service-Aufruf und Zurück-Verhalten gegen installierte Version nachweisen.
- **HbbTV:** Bestehende Kette `vdr-plugin-web` / `cefbrowser` / `remotetranscode` beibehalten. Ein Start aus Suite-OSD benötigt einen belegten Plugin-/Suite-Aufruf und klare Übergabe von Fokus, Farbtasten/Zurück, Browserbild und Medienausgabe. Kein Versprechen paralleler OSD-Renderer oder direkter Einbettung eines CEF-Fensters. Nach Ende Fokus/Live-TV zurückgeben. Falls das Ausgabeplugin für OSD-/Videohandoff spezielle Grenzen setzt, müssen diese gesondert am realen VDR getestet werden.
- **Image-Galerie:** Metadaten/Dateilisten aus dem künftigen Image-Addon; Decoder/Rendering nur über tatsächlich verfügbare native Bilddarstellung. Stand heute ist das **ein noch zu implementierendes** TV-Feature, nicht bereits in vaapivideo enthalten.
- **Media Tools:** Verwaltung, Konvertierung, Import-Status und Fortschritt über den bestehenden Core/Agent; Hintergrundverarbeitung bleibt serverseitig. Für lokale Wiedergabe eines Films wird erst nach Geräte- und Stream-/Datei-Test die geeignete bestehende Player-Schnittstelle gewählt. Media Tools und Media-Player sind unterschiedliche Verantwortungen.
- **Suchtimer:** Gehören zum lokalen VDR-Plugin `epgsearch` (nicht zum VDR-Kern). Das Suite-OSD startet dessen vorhandene native Oberfläche: bevorzugt `epgsearchonly` (direktes Suchmenü, sofern installiert), andernfalls `epgsearch` (dessen normales Hauptmenü). Änderungen erfolgen im Plugin; keine HTTP-/Public-API-v1-Schicht oder zweite Suchtimer-Verwaltung.
- **Weitere Suite-Addons:** Wo keine VDR-interne Schnittstelle existiert (z. B. Image oder Media Tools), den bestehenden lokalen Suite-/Agent-Dienst verwenden. Public API v1 bleibt für externe Clients bestehen, ist aber keine generelle Pflicht für das lokale OSD.

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
| Suchtimer | `epgsearch`-Plugin-Menü lokal; `epgsearchonly` wenn verfügbar, sonst `epgsearch` | Menüaufruf implementiert; echte VDR-/Plugin-Abnahme ausstehend |
| Teletext | vorhandenes Plugin inkl. Farb-/Zurück-Tasten | Nicht integriert |
| HbbTV | Start, Browser-Bild, Videos, Eingaben, Ende/TV-Rückkehr | Nicht integriert |
| Image | Liste, Bildanzeige, Galerie, Diashow | Nicht implementiert |
| Media Tools | Status/Aktionen, Langläufer-Rückmeldung | Nicht integriert |
| Videoplayer | Datei/Stream, Pause/Seek, Audio/Untertitel, TV-Rückkehr | Nicht integriert |

**Vergleich zu Konkurrenz:** Kodi und Plex dienen später als UX-Referenz für 10-Foot-Navigation, Suche und Medienbibliothek. Aktuelle Feature-Parität ist **nicht geprüft**. Kein fremdes Produkt rechtfertigt eine ungefragte Funktionsausweitung.

## Offene technische Prüfungen

1. Installierte VDR-API-Version, vorhandene OSD-Provider und Geräteausgabe.
2. Tatsächlich verwendbare VDR-Plugin-Aufrufe für Teletext/HbbTV und deren OSD-Ownership.
3. Lokale VDR-/Plugin-Schnittstellen für sämtliche VDR-eigenen Funktionen vorrangig nutzen; nur für Suite-eigene Addons die tatsächlich vorhandenen lokalen Suite-/Agent-Verträge prüfen. Kein pauschaler Public-API-v1-Client im Suite-OSD.
4. Native Bilder und Videos: OSD/Player-Funktionen des eingesetzten Ausgabeplugins und Rückkehr zum TV.
5. Reale Abnahme mit yaVDR und kompatiblem VDR-Plugin-Build; reine GitHub-Commits oder Quellcode-Tests sind kein Hardware-Nachweis.

## Gültige Suite-Quellen

Geprüfte Projektbasis: `hotzenplotz5/vdr-suite` `main@2142c8138008b020508f5acd43dba8cba1cb2985`, `AGENTS.md` Blob `55555d95e64159c62dd2b2a7d5c0f56834dc1529`.

- [Suite Addon-Paketierung](https://github.com/hotzenplotz5/vdr-suite/blob/2142c8138008b020508f5acd43dba8cba1cb2985/docs/development/addon-source-packaging.md)
- [Suite installierte Addon-Registry](https://github.com/hotzenplotz5/vdr-suite/blob/2142c8138008b020508f5acd43dba8cba1cb2985/docs/development/addon-installed-registry-contract.md)
- [Zurückgestellte HbbTV-Extraktion](https://github.com/hotzenplotz5/vdr-suite/blob/2142c8138008b020508f5acd43dba8cba1cb2985/docs/planning/hbbtv-addon-extraction-roadmap.md)

Das Suite-OSD ist **keine Reaktivierung** der zurückgestellten HbbTV-Addon-Extraktion.
