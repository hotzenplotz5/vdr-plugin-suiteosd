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

## Aktueller Implementierungsstand (0.2.0, noch nicht auf TV abgenommen)

- Native `cOsdObject`-Home-Ansicht mit fester Kopfzeile, schmaler Icon-Leiste und eingeblendeten **allen** Labels beim Navigieren auf die Sidebar.
- Pfeiltasten, OK und Zurück sowie vertikale Home-Sektionen ohne sichtbaren Scrollbalken und horizontale Sender-/Aufnahmekacheln.
- Reale VDR-Kanäle, aktuelle/folgende EPG-Ereignisse und Aufnahmetitel als Snapshot (keine künstlichen Beispieldaten).
- Direkter VDR-Senderwechsel bei Auswahl einer Jetzt-Sendung und VDR-eigene Wiedergabe für ausgewählte Aufnahmen; kein zweiter Video-Player.
- Menüübergabe an **bereits installierte** VDR-Plugins `web` (HbbTV), `osdteletext` und `epgsearch` (Suchtimer/Suche, optional `epgsearchonly`) per `cRemote::CallPlugin`. Verfügbarkeit und Fokus-/Rückkehrverhalten sind auf dem echten VDR noch zu testen.
- Derzeit **keine** eingebettete Live-Vorschau, kein Artwork, keine eigenständigen Suite-Addon-Ansichten und noch keine Public-API-v1-Anbindung. Das Vorschaufeld ist ein ausdrücklich beschrifteter Platzhalter.
- Ein echter Ausgabe-/OSD-Provider und passende VDR-ABI sind für eine Anzeige nötig; der Code ist **ohne vorhandenes Ausgabeplugin entwickel- und CI-testbar**, aber noch nicht visuell abgenommen.

Die GitHub-Actions-Matrix kompiliert gegen den unveränderten Upstream-VDR-Quellstand der Tags **2.8.3** und **2.8.4** sowie einen separaten Headless-Navigationstest. Ein erfolgreicher Build bedeutet keine Funktionsabnahme auf dem realen yaVDR.

## Dokumente

- [Architektur und Integrationsgrenzen](docs/architecture.md)
- [Addon- und OSD-Interaktionsvertrag (Entwurf)](docs/addon-ui-contract.md)
- [Roadmap und Abnahmekriterien](docs/roadmap.md)

## Quellen und Abhängigkeiten

- [VDR-Suite](https://github.com/hotzenplotz5/vdr-suite) – Backend und Public API v1
- [vaapivideo](https://github.com/dnehring7/vdr-plugin-vaapivideo) – *optionales* Ausgabeplugin; kein Fork
- [VDR](https://www.tvdr.de/) – Plugin-, Skin- und OSD-Schnittstellen

**Kein Installationsversprechen:** Eine Quellcode-Skizze oder ein Git-Commit bedeutet nicht, dass das Plugin gegen die konkrete VDR-ABI gebaut und mit HbbTV, Teletext oder Medienwiedergabe getestet ist.
