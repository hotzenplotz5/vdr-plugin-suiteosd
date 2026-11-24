# Core-Administration direkt am Fernseher (Suite-OSD 0.8.0)

## Ziel und Autorität

Die gemeinsame Suite-Einstellungsansicht kann erste echte Backend-, Addon-, Benutzer- und Geräteänderungen direkt aus dem nativen VDR-OSD vornehmen. Alle produktiven Daten bleiben ausschließlich im bestehenden VDR-Suite Core, Backend-Agent oder Security-Owner. Es existiert weder eine zweite Datenbank noch ein VDR-root-Superuser.

VDR-Kernfunktionen (Kanäle, EPG, Timer, Aufnahmen und VDR-/Plugin-Konfiguration) verwenden weiter die nativen VDR-Schnittstellen. Nur für Suite-eigene Adminfunktionen werden die **bestehenden** authentifizierten Suite-Core-HTTP-Routen wiederverwendet; keine neue Public-API-v1-Route.

## Anmelde- und Sicherheitsvertrag

Die TV-Ansicht verwendet die bereits vorhandene Human-Account-Anmeldung POST /api/security/browser-sessions mit HTTP Basic, erhält ein vdr_suite_session-Cookie und einen CSRF-Token. Dieser Token wird bei allen späteren geschützten POSTs als X-CSRF-Token übermittelt. Es wird **keine** Device-Credential-Identität als Human-Administrator eingesetzt.

Ein lokaler VDR-Prozess erhält durch seine Betriebssystemrechte keine zusätzlichen Suite-Core-Berechtigungen. Account- und Device-Administrationsrechte bleiben vollständig beim Security-Owner. Der Server verwaltet unverändert Grants, starke ETags, CSRF, letzte-Administrator-Regeln und Revisionskonflikte.

Die Zugangsdaten werden mit einer eingeblendeten Fernbedienungstastatur eingegeben. Das Passwort wird verdeckt angezeigt, nach dem Anmeldeversuch aus dem UI-Zustand gelöscht und nicht in der Plugin-Konfiguration gespeichert. Beim Verlassen des Einstellungsbereichs wird die Browser-Sitzung über POST /api/security/browser-sessions/logout abgemeldet.

Die Core-Adresse ist im originalen VDR-Plugin-Setup unter suiteosd.CoreURL konfigurierbar. Vorgabe ist http://127.0.0.1:18080, entsprechend dem verifizierten Core-Runtime-Default. Für die Übertragung von Human-Anmeldedaten akzeptiert der Client HTTP nur auf Loopback oder HTTPS mit normaler Zertifikatsprüfung. Kein anonymer Administrations- oder lokaler Datenbankzugriff.

## Implementierte TV-Funktionen

| Kategorie | Bearbeitbare Funktionen | Bestehender Owner-Vertrag |
| --- | --- | --- |
| Backends | Autorisierte Backendliste anzeigen, konkretes Backend wählen | GET /api/v1/backends |
| Addons | Media-Tools-(Rectools-)Aktivierung für gewähltes Backend bestätigen und ändern | GET/POST /api/v1/addons/rectools/activation mit expectedRevision |
| Encoding | Import H.264 CRF und Shrink H.265 CRF einstellen; bestehende Presets erhalten; für neue Aufträge speichern | GET/POST /api/v1/addons/media-tools/encoding mit 16-stelliger Revision |
| Benutzer | Bestehende Konten sehen, Konto aktivieren/deaktivieren, Anzeigenamen ändern | GET /api/v1/accounts; GET/POST /api/v1/accounts/{accountId} mit frischem If-Match |
| Geräte | Gekoppelte TV-/App-Geräte mit eindeutiger ID sehen und gezielt widerrufen | GET /api/v1/devices; GET/POST /api/v1/devices/{deviceId}/lifecycle mit frischem If-Match |

Für Account- oder Device-Mutationen wird das autoritative ETag **unmittelbar vor dem einzelnen POST** gelesen; es gibt kein automatisches Retry bei ungewissem Ergebnis. Bestätigungen werden direkt im TV-OSD verlangt. Core-Readback ist notwendig, bevor Erfolg angezeigt wird.

## Bedienung

Einstellungen → VDR-Suite → Backend und Media Tools im Core verwalten öffnet bei Bedarf die Human-Anmeldung. Die Fernbedienung nutzt Pfeile und OK für die Bildschirmtastatur, Grün wechselt Benutzer/Passwort, Blau Groß-/Kleinschreibung, Gelb löscht, Rot meldet an und Zifferntasten geben Zahlen direkt ein. Nach erfolgreicher Anmeldung kann das Backend gewählt werden. In der Backendansicht aktiviert OK nach Nachfrage Media Tools; Links/Rechts ändern die CRF-Werte, und die Aktion „Encoding speichern“ schreibt diese an den Core.

Einstellungen → Benutzer & Geräte öffnet dieselbe authentifizierte Verwaltungsoberfläche. Blau wechselt zwischen Konto- und Geräteliste. Im Kontodetail aktiviert/deaktiviert Rot nach Bestätigung, Grün bearbeitet den Anzeigenamen. Im Gerätedetail kann Rot einen Widerruf mit eindeutiger Geräte-ID und zweiter Bestätigung anstoßen. Gelb meldet in Listen ab; Zurück führt zur übergeordneten Ansicht.

## Abnahme

Automatische Tests: Core-Admin-Tastatur/Navigation, HTTP-Loopback-Mock mit Human Basic, Session-Cookie, CSRF, Addon-CAS, Encoding-Revision, Account-ETag und Device-Lifecycle-ETag. Kompilierung auf GitHub Actions gegen unveränderte VDR-Header 2.8.3 und 2.8.4. Diese Tests dürfen **nicht** als reale Server- oder TV-Abnahme ausgegeben werden.

Vor Produktivnutzung noch offen: manuelle Anmeldung auf dem konkreten yaVDR, Echtzeit-/Bilddarstellung und Fernbedienungs-Usability, Core-URL und Reverse-Proxy-Konfiguration, fehlgeschlagene/abgelaufene Sitzungen und ein ausdrücklich freigegebener End-to-End-Mutationstest mit nichtproduktiven Testkonten/Devices.

## Nicht abgeschlossene Parität

Nicht in dieser Version enthalten sind Account-Erstellung/Passwortverwaltung, Konto-/Device-Grants mit serverseitig angebotenen Permission-Katalogen, Genehmigung wartender Gerätekopplungen, Credential-Rotation, weitere Backend- und Addon-Konfigurationen, vollständige Cursor-Pagination über 100 Einträge, vollständige Unicode-Tastatur, SSE-Statusupdates und generische Plugin-/Addon-Settings. All diese Funktionen bleiben über das existierende Suite-Webfrontend bedienbar.

Das ist ein erster **echter vertikaler Verwaltungs-Slice**, nicht die Behauptung einer vollständig portierten Suite-Administration.
