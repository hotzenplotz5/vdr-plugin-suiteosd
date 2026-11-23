# VDR Suite OSD

Ein eigenständiges natives VDR-Frontend für **VDR-Suite**. Das Plugin soll die Funktionen und installierten Addons der Suite am Fernseher per Fernbedienung verfügbar machen, ohne ein Ausgabeplugin zu forken.

**Projektstatus:** frühe Entwicklung; noch keine auf einem echten VDR verifizierte Version.

## Grundsätze

- **Kein Fork von vaapivideo:** Das Ausgabeplugin (z. B. vaapivideo oder softhddevice) bleibt Eigentümer von Video, Audio und OSD-Rendering.
- **Suite-OSD ist ein VDR-Plugin:** Die VDR-Skin-API und eigene native Menüs sind die Darstellungs- und Bedienebene. Es ist **kein** eingebetteter HTML-Browser.
- **VDR-Suite bleibt fachliche Autorität:** Public API v1 und vorhandene Core-/Agent-/SuiteBridge-Dienste liefern Daten, Aktionen und Berechtigungen. Keine zweite Geschäftslogik und keine zweite Medienbibliothek.
- **Addons statt Sonderfälle:** EPG, Timer, Aufnahmen, Suchtimer, Teletext, HbbTV, Image, Media Tools und weitere installierte Suite-Funktionen sollen dieselbe navigierbare TV-Oberfläche erhalten.
- **Bestehende Komponenten wiederverwenden:** Teletext über vorhandene VDR-Plugins, HbbTV über vdr-plugin-web/cefbrowser/remotetranscode, Medienwiedergabe über nachgewiesene VDR-/Ausgabeplugin-Schnittstellen.
- **Keine zusätzlichen Aktivierungs- oder Berechtigungsgates:** Vorhandene Suite-Autorisierung beibehalten; eine Addon-Beschreibung erzeugt keine eigenen Rechte.

## Dokumente

- [Architektur und Integrationsgrenzen](docs/architecture.md)
- [Addon- und OSD-Interaktionsvertrag (Entwurf)](docs/addon-ui-contract.md)
- [Roadmap und Abnahmekriterien](docs/roadmap.md)

## Quellen und Abhängigkeiten

- [VDR-Suite](https://github.com/hotzenplotz5/vdr-suite) – Backend und Public API v1
- [vaapivideo](https://github.com/dnehring7/vdr-plugin-vaapivideo) – *optionales* Ausgabeplugin; kein Fork
- [VDR](https://www.tvdr.de/) – Plugin-, Skin- und OSD-Schnittstellen

**Kein Installationsversprechen:** Eine Quellcode-Skizze oder ein Git-Commit bedeutet nicht, dass das Plugin gegen die konkrete VDR-ABI gebaut und mit HbbTV, Teletext oder Medienwiedergabe getestet ist.
