# Gemeinsame Einstellungen: VDR und VDR-Suite (Stufe 1, 0.7.0)

**Historischer Stand der ersten Integrationsstufe:** Suite-OSD 0.8.0 ergänzt inzwischen authentifizierte Suite-Core-Verwaltung. Aktueller Vertrag: [Core-Administration am TV](settings-core-administration.md). Die nachstehenden 0.7.0-Angaben beschreiben bewusst den früheren Stand.

## Geltende Entscheidung

Ein Einstellungsmenü im freigegebenen VDR-Suite-TV-Design; **kein zweiter Konfigurationsbestand** und keine eigene Backend-Authentifizierung. Jedes Feld wird weiterhin von seinem tatsächlichen Eigentümer gespeichert. Die lokale TV-OSD-Bedienung ist nicht gleichbedeutend mit dem vollständigen Admin-Webfrontend.

| Bereich im TV-OSD | Daten-/Aktions-Eigentümer | Aktueller Stand |
| --- | --- | --- |
| VDR | VDR-Kern | Native VDR-Einstellungen per \`kSetup\` öffnen; enthält OSD/EPG/DVB/LNB/CAM/Aufnahmen/Wiedergabe |
| VDR-Suite | Bestehender Core/Agent | Bereich navigierbar; Verweis auf bestehendes Suite-Webfrontend, **noch kein TV-Editor für Core-Einstellungen** |
| Plugins | Jedes installierte VDR-Plugin | Native VDR-Einstellungen öffnen, **dort** Plugins auswählen; keine behauptete direkte Sprung-API in private VDR-Submenüs |
| TV-Oberfläche | VDR-Plugin \`suiteosd\` für Mini-Live-TV; VDR für OSD-Sprache/-Grundwerte | Mini-Live-TV im Aufnahmenbrowser im Suite-Menü umschaltbar; persistent unter \`suiteosd.LivePreview\` via \`SetupStore\` + \`Setup.Save\`. Klassischer VDR-Plugin-Setup-Eintrag zeigt denselben Wert |
| Benutzer & Geräte | VDR-Suite Core | Bereich navigierbar; Konten/Rollen/TV-Kopplungen weiterhin im Suite-Webfrontend, **kein lokales Rechte-Duplikat** |
| System | Laufender VDR-Pluginprozess | Suite-OSD-Version, Kompilations-VDR-Version, lokale Präsenz von TVScraper und epgsearch; kein unbestätigter Backend-Status |

## Bedienung

Das Sidebar-Symbol **Einstellungen** und die Setup-Taste innerhalb des Suite-OSD öffnen das gemeinsame Dashboard. Sechs Kacheln navigieren mit den Pfeilen/OK; Zurück führt von der Detailansicht zum Dashboard und dann zum Suite-Home. Fokus auf der linken Kante klappt die Suite-Sidebar auf.

- VDR → **VDR-Einstellungen öffnen**: Übergabe der existierenden VDR-Menüführung an die VDR-Hauptschleife. Beim Verlassen des nativen Menüs ist das bisherige VDR-Rückkehrverhalten maßgeblich; die Rückkehr unmittelbar zur Suite-Kategorie ist noch nicht real getestet.
- Plugins → **VDR-Setup mit Plugin-Einstellungen öffnen**: Der Anwender wählt dort **Plugins** und das gewünschte installierte Plugin. Das VDR-Plugin entscheidet selbst, ob es eine Setupseite anbietet.
- TV-Oberfläche → **Live-TV-Fenster in Aufnahmen: Ein/Aus**: Schaltet die bisherige \`CanScaleVideo()/ScaleVideo()\`-Nutzung im Aufnahmenbrowser. Kein neuer Player, kein Eingriff in \`vaapivideo\`. **VDR-OSD und Spracheinstellungen öffnen** verzweigt zum nativen VDR-Setup.
- Suite/Benutzer/System: navigierbare Informationen ohne irreführende Schreib-/Admin-Tasten; ein vollständiger TV-Client für diese Daten erfordert einen geprüften lokalen Suite-Core-Vertrag.

## Technischer Speichervertrag

Die neue lokale Einstellung wird über den bereits vorhandenen VDR-Plugin-Setup-Vertrag abgewickelt:
- VDR liest beim Start \`suiteosd.LivePreview\` mittels \`cPlugin::SetupParse()\`.
- Die Suite-TV-Einstellungsseite ruft \`cPlugin::SetupStore("LivePreview", 0|1)\` auf und schreibt über \`Setup.Save()\`. Falls VDR nicht speichern kann, wird der frühere Wert beibehalten und ein konkreter Fehler angezeigt.
- \`cPluginSuiteOsd::SetupMenu()\` stellt den Wert zugleich im nativen VDR-Setup → Plugins → suiteosd bereit; derselbe Schlüssel, kein zweites Format.
- Das Aufnahmenfenster fragt den gespeicherten Schalter beim Öffnen ab. Andere VDR-Funktionen und Ausgabegeräte ohne \`ScaleVideo\` bleiben nutzbar.

## Abnahme und Grenzen

Automatisiert: \`make check-settings-navigation\` verifiziert Kategorien, Aktionen, Auswahl, Back-Pfad und read-only Seiten. GitHub Actions kompiliert das Plugin separat für VDR 2.8.3 und 2.8.4. Es ist noch **kein** Laufzeittest auf einem echten Fernseher, kein visueller Pixelabgleich und keine reale VDR-Setup-Persistenz-/Neustartprobe erfolgt.

Für die zweite Stufe: Nur bereits verifizierte lokale Suite-Core-/Agent-Schnittstellen für tatsächlich gewählte Suite-Einstellungen einbinden, vorhandene Benutzerrechte beibehalten; keine direkte Konfigdateiänderung im Core, keine Public API v1 für lokale VDR-Einstellungen, keine neuen Berechtigungs- oder Aktivierungsgates.
