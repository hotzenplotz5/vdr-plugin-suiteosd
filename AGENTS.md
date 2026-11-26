# SuiteOSD – verbindliche Agenten- und Repository-Regeln

Gilt verbindlich für alle Assistenten und Agenten im gesamten Repository
`hotzenplotz5/vdr-plugin-suiteosd`, einschließlich Entwicklung, Tests, Doku,
Build, Paketierung und Operator-Anweisungen. Keine bloßen Empfehlungen.

Grundlage: [VDR-Suite AGENTS.md](https://github.com/hotzenplotz5/vdr-suite/blob/main/AGENTS.md),
abgeglichen am 26.11.2026 (Blob `55555d95e64159c62dd2b2a7d5c0f56834dc1529`).
Allgemeine Regeln sind auf das native VDR-Plugin übertragen; neuere konkrete
Maintainer-Entscheidungen haben Vorrang vor historischen Implementierungsbeschreibungen.

## 1. Verbindliche Lesepflicht und Nachweis (AG-000)

- Vor substantieller Entwicklung, Diagnose oder einem Test-/Installationsblock
  vollständige aktuelle Root-AGENTS.md, geltende untergeordnete AGENTS.md und
  relevante verlinkte Fachverträge tatsächlich lesen. Bei repositoryübergreifender
  Arbeit auch die dort geltenden Regeln lesen.
- Zu Beginn Repository, Branch, exakten Head, tatsächlich gelesene Dateien
  und Blob-SHA oder geprüften Ref nennen. Niemals GELESEN behaupten ohne
  tatsächlichen vollständigen Abruf. Teiltreffer sind keine vollständige Lektüre.
- Bei relevantem Ref-/Regelwechsel neu abgleichen; unveränderte nachgewiesene
  Regeln nicht unnötig erneut lesen. Fehlende Belege konkret kennzeichnen.
- Lesen ist keine zusätzliche Freigabe, kein CI-Gate und kein Zwischenstopp.
- Je nach Aufgabe vollständig lesen: [Architektur](docs/architecture.md),
  [freigegebenes Design](docs/approved-tv-home-design.md),
  [Webfrontend-Parität](docs/webfrontend-parity.md),
  [Addon-Vertrag](docs/addon-ui-contract.md), [Settings](docs/settings-integration.md),
  [bisherige Core-Verwaltung](docs/settings-core-administration.md),
  [ImageNext](docs/image-next-integration.md), [Roadmap](docs/roadmap.md).
  Abschnitt 5 dieser AGENTS.md legt das neuere Pairing-Ziel verbindlich fest.

## 2. AGENTS.md ausschließlich nach persönlicher Erlaubnis

**Nur Änderungen an AGENTS.md mit meiner persönlichen Erlaubnis oder nach meiner Anordnung!**

Gilt für jede AGENTS.md im Repository. Keine eigenmächtigen Änderungen,
Ergänzungen, Abschwächungen oder vermeintlichen Korrekturen. Widersprüche
anhand tatsächlicher Quellen klären; keine neuen Regeln erfinden.

## 3. Oberste Arbeitsregel – KEIN STOPP bis zum beauftragten Gesamtziel

**Alle notwendigen, bereits beauftragten und ausführbaren Schritte selbstständig
fortführen, bis das vereinbarte Gesamtziel samt erforderlicher Verifikation
erreicht ist.**

- Analyse, Diagnose, Implementierung, Zwischencommit, Push, laufende CI und
  abgeschlossene Teilschritte sind keine Gründe für eine Arbeitsunterbrechung.
  Fortschrittsmeldungen sind keine Übergabe an den Maintainer.
- Nach jedem Teilschritt unmittelbar den nächsten beauftragten Schritt ausführen.
  Keine wiederholte Aufforderung oder erneute Freigabe verlangen. Keine künstlichen
  Gesprächs-, Commit- oder CI-Etappen. Fehler gegen den tatsächlich getesteten
  SHA diagnostizieren, Ursache beheben, gezielt erneut prüfen und weiterarbeiten.
- Keine bloße Diagnose oder Teilimplementierung als Endergebnis ausgeben.
  Keinen Kandidaten zur Abnahme übergeben, solange bekannte, selbst behebbare
  Integrations-, Implementierungs-, Packaging- oder Validierungslücken bestehen.
- Tatsächliche Blocker betreffen nur die konkrete Aktion: fehlender Zugriff,
  belegter Konflikt, konkrete Gefahr, tatsächlich neue Maintainer-Entscheidung
  oder unverzichtbare reale Hardwareprüfung. Alle unabhängigen genehmigten
  Schritte erledigen, autoritative Quellen erneut prüfen und verbleibenden
  Blocker präzise belegen. Keine fiktiven Tool-/Berechtigungshindernisse.
- Während relevanter CI unabhängige genehmigte Arbeiten fortsetzen; vor
  Abschluss einen für die nächste autorisierte Entscheidung erforderlichen
  Lauf erneut prüfen. Keine unnötigen Wartezyklen bei Dokumentationsänderungen.
- Scope bleibt verbindlich: „weiter“ erlaubt keine Nebenprojekte oder
  unbestellten Refactorings. Nach Erreichen des vereinbarten Abnahmeziels
  Ergebnis nachweisen und beenden. Keine neuen Aufgaben erfinden.

## 4. KISS / Usability First – KEINE neuen Sperren

**KEINE NEUEN SPERREN, GATES ODER BERECHTIGUNGSREGELN eigenmächtig einführen.
Ausnahmen nur, wenn tatsächlich notwendig, und ausschließlich nach vorheriger
ausdrücklicher Genehmigung des Maintainers.**

- Gilt für Menüs, Pairing, APIs, Services, Addons, Builds, Tests und Installation.
  Keine künstlichen Vorbedingungen, Capability-/Lease-/Aktivierungs-/
  Authentifizierungs-Zusatzprüfungen, Fail-closed-Fallbacks, deaktivierten
  Funktionen, erzwungenen Einstellungen oder zusätzlichen Freigabeschritte.
- Keine spekulative Sicherheits-, Robustheits- oder Distributionssperre.
  Vorhandene Owner, APIs und Informationen wiederverwenden; keine unnötig
  distributionsspezifisch fest verdrahteten Pfade.
- Bei unvermeidbarer neuer Sperre genau diese Maßnahme vor Coding stoppen:
  nachgewiesenen Schaden, betroffene Funktionen, mildere Alternative und
  Umfang erläutern und persönliche Entscheidung einholen. Schweigen,
  allgemeiner Auftrag und bestandener Test sind keine Zustimmung. Ohne
  ausdrückliches Ja kein Coding, Commit oder Deployment der Sperre.
  Unabhängige beauftragte Arbeit fortführen.
- Bestehende fachlich erforderliche Autorisierung und Schutzvorgaben bleiben
  bestehen; weder umgehen noch eigenmächtig verschärfen.
- Native VDR-Funktionen und zulässige unabhängige Funktionen bleiben auch bei
  Suite-/Agent-/Netz-/Plugin-Ausfall bedienbar. Navigation ist keine
  Capability-Freigabe. Kein Ausblenden ganzer Ansichten, erzwungener Tabwechsel,
  Zurücksetzen gültiger Auswahl oder wiederkehrende Freigabe-/Login-Schleife.
- Letzte erfolgreich geladene, weiterhin zulässig sichtbare Daten erhalten,
  nötigenfalls als veraltet kennzeichnen. Widerruf und explizite Abmeldung
  gemäß bestehendem Security-Vertrag behandeln. Keine Mutation aufgrund alter
  Daten freigeben und ungewisse mutierende Aufträge nie blind wiederholen.
- Kleine Änderung, kleine Lösung. Keine zusätzlichen Backups, pauschalen
  Speicherreserve-/Stage-/CI-Prüfketten oder Neustarts für triviale Updates.
  Neue blockierende Prüfungen nur bei konkretem Schadensrisiko und obiger
  Genehmigung. Tatsächliche technische Fehler verständlich melden.
- Vor jeder Produktcodeänderung Ziel, beobachtbare Abnahme, tatsächlichen
  Owner, kleinste notwendige Änderung und Verifikationsweg bestimmen.
  Diagnose nur soweit entscheidungsrelevant. Neue Dienste, Datenbanken,
  persistente Geschäfts-Zustände, Protokolle, übergreifende Abstraktionen
  und breite Refactorings nur bei belegtem Bedarf und ausdrücklicher
  vorheriger Genehmigung. Keine Nebenprojekte.

## 5. Native Architektur, Public API v1 und Geräte-Pairing

### Zuständigkeiten

- SuiteOSD ist ein natives C++-VDR-Plugin (`suiteosd`), kein Browser und kein
  Ausgabeplugin. Lokale Kanäle, EPG, Timer, Aufnahmen, Replay und native
  Einstellungen verbleiben bei VDR, lokale Suchtimer bei epgsearch.
  Native Funktionen direkt nutzen; kein unnötiger HTTP-Umweg oder Suite-Login.
- Suite-eigene Funktionen (z. B. zentrale Suche, Historie, Empfehlungen,
  Core-Addon-Aktionen) und entfernte Suite-Server über vorhandene Public API v1
  und deren tatsächliche Owner anbinden. Bestehende native Plugin-/Service-
  Schnittstellen für ihre belegten Zuständigkeiten erhalten; lokale Socket-
  Anbindung ist kein Ersatz für Geräte-Pairing und Fernserver-Unterstützung.
- Vor API-Änderungen reale Routen, DTOs, Backend-Kontexte, ID-Zuordnung,
  Grants, Fehler und Readback im Hauptrepo prüfen. Keine erfundenen Endpunkte,
  keine parallele API-/Auth-Architektur, keine direkten Core-Datenbank-/
  Konfigurationsschreibzugriffe, kein zweiter Suchindex und kein TVScraper-
  Vollscan als Ersatz für die bestehende gemeinsame Suche.
- Native IDs nicht ungeprüft auf entfernte Backend-Aufnahmen anwenden.
  Fehlende API-Verträge konkret nachweisen und tatsächlichen Owner einbeziehen,
  keine vermeintliche Ersatzimplementierung als vollständige Parität melden.

### Verbindlicher Authentifizierungsauftrag des Maintainers

**Keine Anmeldemasken in OSD-Ansichten. Geräte-Pairing wie bei Samsung und
VIDAA. Kopplung ausschließlich über die Plugin-Einstellungen. Rechtevergabe
über das Webfrontend. Dauerhafte automatische Anmeldung. Lokale und entfernte
Daemons unterstützen. Mehrere Serverprofile architektonisch vorbereiten.**

- Vor Umsetzung die tatsächlichen Pairing-/Security-Verträge der Samsung-/
  VIDAA-Clients und der VDR-Suite Public API v1 lesen und wiederverwenden.
  Keine neue Authentifizierungsmethode oder eigenmächtige Grants.
- Keine Human-Account-/Passwort-/Browser-Loginmaske in Home, Suche, Details,
  Addons oder Core-Administration. Kein Login beim Öffnen einer Kachel, kein
  Logout beim Verlassen einer Ansicht und kein Human-Login als Fallback.
- Kopplung und Entkopplung ausschließlich im VDR-Plugin-Setup. Serverziel,
  Verbindungs- und Kopplungsstatus dort verständlich darstellen.
- Geräte-Credentials gemäß vorhandenem Security-Vertrag persistent speichern,
  nach VDR-/Plugin-Neustart automatisch wiederverwenden, keine Klartext-
  Benutzerpasswörter und keine Secrets in Logs. Widerruf/Ablauf nach
  bestehendem Vertrag behandeln, Rechte niemals eigenmächtig ausweiten.
- Rechte/Rollen/Grants bleiben beim Core und werden im Webfrontend verwaltet.
  Lokale Root-Rechte ersetzen keine Suite-Autorisierung. Administrative
  Aktionen nur über tatsächlich bestehende, entsprechend autorisierte
  Geräteverträge; keine lokalen Rechteeditoren.
- Konfigurierbares Serverziel, keine Loopback-/Unix-Socket-Pflicht für
  Suite-Server. Verbindungs-, Credential-, Backend- und Datenkontext pro
  Profil sauber trennen; keine fremden Daten oder IDs vermischen.
  Mehrprofilfähigkeit vorbereiten, ohne unbestellte Mehrserver-UI oder
  unnötigen zusätzlichen Persistenz-Stack einzuführen.
- Historische Human-Browser-Login-Beschreibungen in README und Fachverträgen
  sind Altstand, keine künftige Vorgabe. Bei tatsächlicher Umstellung
  betroffene Implementierung, Tests und Doku konsistent aktualisieren.
  Eine AGENTS.md-Änderung allein beweist keine Implementierung.

### SSE geht vor Polling

Für asynchrone Suite-Statusänderungen, Fehler, Abschluss und Readback
vorhandene SSE-/Push-Verträge nutzen. Beim Öffnen/Reconnect einmalig
autoritativen Zustand synchronisieren; verpasste Ereignisse dürfen keinen
zweiten Auftrag auslösen. Keine zyklischen Status-Polling-Kaskaden, künstlichen
Abfragekontingente oder UI-Abbrüche bei weiterlaufenden Jobs. Polling als
Status-Ersatz nur bei belegter technischer Notwendigkeit und vorheriger
Abstimmung. Native OSD-Render-/Animationszyklen und bestehende native
Plugin-Service-Mechanismen sind davon zu unterscheiden.

## 6. OSD-Design, Fernbedienung und Integration

- Freigegebenes TV-Design und reales VDR-Suite-Webfrontend sind Grundlage.
  Tatsächliche Web-Owner und genehmigte Vorlage prüfen. Kein frei erfundenes
  Mockup als fertige Funktion ausgeben; TV-gerechte Lesbarkeit, Artwork,
  Kontrast und Fokus erhalten. Größere Designabweichungen abstimmen.
- Alle interaktiven Elemente und sichtbaren Aktionen per Fernbedienung
  erreichbar: Pfeile, OK, Zurück, Info, Farb- und vereinbarte weitere Tasten.
  Sichtbarer Fokus, vorhersehbare Navigation, Rückkehr zur vorherigen Auswahl,
  scrollbarige Inhalte, Mitwirkenden-Auswahl und Suche vollständig bedienbar.
  Keine Maus-/Tastaturpflicht, keine funktionslosen Buttons.
- Aktiven Owner und reale VDR-Komposition respektieren: passiver Skin ist
  kein interaktives Menü; keine doppelte Key-Verarbeitung. Native Menüs,
  Replay, HbbTV und fremde Plugins behalten ihre Ownership. Beim Verlassen
  Fokus, Video-Skalierung und Wiedergabezustand korrekt wiederherstellen.
- Kein Fork/Patch von VDR-Core oder Ausgabeplugin ohne Auftrag, keine eigenen
  Decoder, Player, HbbTV-Browser oder Transcoder. VDR-/Skin-/OSD-API und
  TVScraper, image_next, epgsearch, Teletext und HbbTV über belegte
  Schnittstellen wiederverwenden. Fehlende optionale Plugins/Artworks/OSD-
  Fähigkeiten lokal verständlich behandeln, native Fallbacks erhalten.
- Keine automatische Skin-Aktivierung oder Attach-/MainMenuHooks-/Keymacros-/
  remote.conf-Änderungen ohne Auftrag. Native Menüwege erhalten.
- Freigegebene Galerie und Grid-/Karussell-Option erhalten; Bildquelle,
  Thumbnail-Service und Wiedergabe verbleiben bei image_next.
- VDR-Locks, Zeigerlebensdauer, Thread-/UI-Ownership beachten. Daten unter
  gültigem Lock kopieren; langsame HTTP-/Datei-/Bildoperationen außerhalb
  von VDR-Locks und ohne unnötige Blockierung der OSD-Bedienung. Ressourcen
  und Worker sauber beenden, bestehende begrenzte Caches erhalten.
- Vendor-/Service-Header nur mit belegter Upstream-Provenienz und Lizenz.
  Gemeinsame Service-ABI-Änderungen gegen beide tatsächlichen Owner prüfen.

## 7. Funktionsparität und tatsächliche Abnahme

- Bewährtes Webfrontend bleibt funktionaler Mindestmaßstab. Vor neuen oder
  migrierten Features kurze Akzeptanzmatrix erstellen: Eingaben, Trefferzahlen,
  Sortierung, Titel/Metadaten, IDs, Paging, Grants, Fehler, Aktionen und Readback.
  Keine zweite Fachlogik; native Ersatzdaten transparent als solche kennzeichnen.
- Je benutzerseitigem Feature knapper evidenzbasierter Konkurrenzvergleich
  (z. B. Kodi/Plex), Ungeprüftes markieren. Keine Erlaubnis für Nebenfeatures.
- Für jede geänderte produktive Aktion tatsächliche Kette prüfen:
  Fernbedienung -> aktives OSD/Menü -> vorhandener Owner -> Wirkung/Readback
  -> sichtbare Darstellung. Nicht anwendbare Stufen benennen. Bei übergreifenden
  Controls auch echte Owner-Erzeugung, Wechsel, Schließen und Replay prüfen.
  Isolierte Mock-/Unit-Aufrufe allein sind kein Integrationsnachweis.
- Gezielte Regressionen für Navigation, Fehler, Wiederherstellung,
  Autorisierung, fehlende optionale Komponenten. Bei Pairing insbesondere
  Neustart, Reconnect, Widerruf, lokale/entfernte Kontexte, keine Loginmaske
  und keine Credential-/ID-Vermischung prüfen.
- Evidenz getrennt ausweisen: UNIT/CONTRACT, PRODUCTION-INTEGRATION, BUILD,
  CI, PACKAGE/INSTALL, REAL-VDR/TV. Ein Commit, HTTP 200, Mock, Build oder
  installiertes Paket allein beweist keine Funktionsabnahme.
- Reale Abnahme mit tatsächlicher VDR-Version/ABI, Ausgabeplugin,
  installierter Plugin-Identität, OSD und Fernbedienung. Lesende Prüfungen
  zuerst; destruktive Aufnahmemutationen oder echte Timeranlage nur
  ausdrücklich autorisiert. Keine bereits belegten unveränderten Tests
  unnötig wiederholen. Paritätsmatrix/Roadmap an sinnvollen Meilensteinen
  aktualisieren, bekannte Implementierungslücken vorher selbst schließen.

## 8. Nachweise statt Behauptungen / GitHub-first

- Niemals Regeln, Nutzerfreigaben, Befehle, Pfade, Statuscodes, Build-/CI-
  Ergebnisse, Deployments, Rechte oder Blocker erfinden. Nutzeranordnung,
  Repository-Regel, Werkzeugbeleg und eigene Schlussfolgerung unterscheiden.
  Fehlende Evidenz als NOT_VERIFIED, Fehlschlag als FAILED kennzeichnen.
- Vor Behauptung fehlender Ausführbarkeit geeignete Tools prüfen und den
  sicheren beauftragten Vorgang versuchen. Fehlender Toolzugriff ist nicht
  automatisch fehlende Kontoberechtigung. Tatsächlichen Fehler benennen.
- GitHub-Connector bevorzugen für Lesen, Schreiben, Commit/Push und Status.
  Lokaler Checkout nur für tatsächliche Builds, Tests, generierte Artefakte,
  Hardware oder nicht anders sicher ausführbare Vorgänge. Keine abgeschnitten
  gelesene Datei vollständig ersetzen.
- Vor jedem Write aktuellen Remote-Head, danach tatsächlichen Commit/Diff
  prüfen. Vor Build/Test/Install Checkout-Root, Branch, SHA, benötigte Fixes,
  reale Make-Targets, Abhängigkeiten, Ausgabepfade, ABI, Rechte und
  Seiteneffekte verifizieren. Nicht aus alten Logs auf neuen Kandidaten schließen.
- Vor langen/abgekoppelten Builds SHA nochmals prüfen; nohup/PID allein ist
  kein Nachweis. Prozessidentität, Log, Exitstatus und Artefakt verifizieren.
- Verifizierte Nachweise (SHA, Hash, Buildparameter, Tests) wiederverwenden.
  Live-Zustand vor riskanter Aktion prüfen. Keine grundlegende Repository-
  Recherche auf den Operator abwälzen.
- Jeden Operator-Block vor Übergabe auf tatsächliche Befehle, Pfade, Targets,
  Wirkung, Fehlerpfade und Abnahme prüfen; unbekannte Hostfakten mit minimaler
  lesender Probe ermitteln statt raten.

## 9. Git, Branches, Commits und CI

- Neue Arbeit auf aktuellem main des korrekten Repos, sofern kein anderer
  bestehender Branch beauftragt. Neue Branches, gestapelte Branches und
  Ausweich-Worktrees nur nach ausdrücklicher Zustimmung. Keine eigenmächtigen
  Branchlöschungen, PR-Zustandsänderungen, Merges, Rebase, Force-Pushes oder
  History-Rewrites. Fremde Arbeit und lokale Änderungen erhalten.
- git fetch, Wechsel auf bestehenden geeigneten Branch und git pull --ff-only
  für beauftragte Arbeit erlaubt. Vor Wechsel git status prüfen; tatsächliche
  Konflikte melden. Kein automatisches reset --hard, git clean, erzwungener
  Checkout oder ungefragtes Stash. Kein künstliches Altbranch-Hindernis.
- Kleine zusammenhängende Commits zeitnah pushen, CI nicht nach jedem Commit
  abwarten. Branchschutz/PR-Regeln einhalten. Nach autorisiertem Merge
  tatsächlichen main-SHA und relevante CI prüfen. Vor Branchlöschung
  Abhängigkeiten wie PRs, Workflows und Paketrezepte prüfen.
- Fokussierte Tests und Build für exakt geänderten C++-/Runtime-Kandidaten.
  Tatsächliche Makefile-/CI-Targets wählen; vorhandene VDR-2.8.3-/2.8.4-
  Buildmatrix und Headless-Tests berücksichtigen. Keine erfundenen Targets.
- Relevante fehlgeschlagene Checks diagnostizieren und beheben; nachgewiesen
  unabhängige Fehler sind kein künstliches Hindernis. Vollständige bestehende
  erforderliche CI für exakten finalen Head bei Ready-for-review, Merge oder
  ausdrücklich verlangter Phasenabnahme. Kein rotes/unverifiziertes
  Produktcode-Merge und keine Umgehung von Branchschutz.
- Doku-/AGENTS.md-only: Inhalt, Links, Format und Diff prüfen. Kein Neubau,
  keine Hardwareabnahme oder CI-Warten allein wegen Dokumentationscommit.
  Automatisch gestartete CI bleibt davon unberührt.
- CI-Status stets mit Workflow, Run-ID/Nummer, exaktem Head und anklickbarem
  Run-Link; queued/running nicht als grün melden. Keine künstlichen Commits
  oder Seiteneffekte zum Erzwingen eines CI-Laufs.

## 10. Build, Paketierung und yaVDR

- Normale Entwicklungs- und Paketbuilds unprivilegiert. Inkrementellen
  C++-Build erhalten, kein routinemäßiges make clean, keine unnötigen neuen
  Checkouts. Reproduzierbare isolierte Paket-/Release-Builds gemäß Vertrag
  dürfen sauber neu bauen und keine fremden Objektdateien übernehmen.
- Dasselbe Plugin nicht unnötig zweimal kompilieren. Vor Build vorhandene
  autorisierte Kandidaten, Produktcode-SHA getrennt vom Doku-HEAD,
  Buildparameter, ABI, Abhängigkeiten und Artefaktintegrität prüfen.
  Verifiziertes Binary/Paket wiederverwenden. Kein Neubau wegen Chatwechsel,
  Doku-Commit, abgebrochener Installation oder bereits korrekt installierter
  Version. Fehlende oder ungeeignete Artefakte konkret belegen; niemals
  ungeprüft umetikettieren.
- Tatsächlich installierte Plugin-Datei, VDR-ABI, Version und Paket-/manuelle
  Herkunft vor Update prüfen. Bestehenden Debian-Paketweg bevorzugen, wenn
  passend; keine unklare Mischinstallation. README, Makefile und debian/
  als tatsächliche Quelle nutzen. Keine fremden Daemon-Installer oder
  Frontend-Stages als Plugin-Rezept erfinden.
- Build und Installation trennen. Keine produktive Installation, kein
  VDR-Neustart und keine Änderung produktiver Konfiguration ohne Auftrag.
- Bei beauftragter yaVDR-Installation bestehende Root-Shell erhalten:
  kein ungefragter Wechsel mittels su, sudo -u, runuser oder nacktes exit.
  Ein ausdrücklich verlangter inkrementeller Root-Build im Installationsfall
  ist die Ausnahme von der unprivilegierten Entwicklungsregel, kein
  allgemeines Root-Build-Gebot.
- Vor GitHub-SSH-Mutationen tatsächliche Remote und SSH-Identität lesend
  prüfen. Keine Schlüssel kopieren, keine spekulativen Rechteänderungen.
  Erfolgreiches ls-remote beweist keine Push-Rechte. Vor yaVDR-SSH-Anweisungen
  den aktuellen VDR-Suite-SSH-Fachvertrag lesen.
- Keine künstlichen Branch-/Ancestor-/CI-Abschluss-Gates für Installation
  eines verifizierten kompatiblen Plugins. Keine CI-Abfragen oder Warte-
  schleifen im Installationsblock. Daten, Aufnahmen, Pairing, Konfiguration
  und andere Plugins erhalten. Echte ABI-Inkompatibilität konkret melden.
- Ausschließlich selbst erzeugte temporäre Test-/Installationspfade
  gezielt bereinigen; bei Installationsfehler Diagnose-/Rollback-Stand
  erhalten. Autorisierte temporäre Worktrees nach Nutzungsende auf
  ungesicherte Änderungen prüfen, genau diesen Worktree entfernen und
  veraltete Metadaten bereinigen. Cleanup im selben erzeugenden Ablauf
  implementieren. Keine fremden Worktrees, zentralen Objekt-Caches,
  Nutzerdateien oder vorhandenen Pakete löschen. Keine Entwickler-Buildreste
  durch Paket-Maintainer-Skripte auf dem Zielhost bereinigen.

## 11. Abschlussbericht und sofort nutzbare Operator-Blöcke

- Kurz: Erledigt (tatsächlicher Commit/SHA/Nachweis), Offen zum Ziel
  (realer Rest/Blocker), Nächste Handlung (ausführen oder erforderlichen
  Operator-Block liefern). Keine bloße Ankündigung noch ausführbarer Arbeit.
- Fix/Update/Installationsvorbereitung: in derselben Antwort Testblock mit
  tatsächlich verifiziertem kleinstem Update-/Installationsweg und
  Versions-/Funktionsprüfung. Operator-Block ist keine automatische
  Installation und ersetzt keine erforderliche Freigabe.
- Ungeklärte Ursache oder nicht verifizierter Installationsweg: sofort
  Diagnoseblock mit kleinster lesender Probe. Doku-only: lesender
  Commit-/Diff-Nachweis statt Build oder Installationsblock.
- Solche Arbeitsantworten enden mit genau einer Überschrift
  `## Testblock` oder `## Diagnoseblock`, gefolgt von genau einem
  gewöhnlichen Bash-Codeblock. Danach keine weiteren Angebote oder Prosa.
  Keine Downloads statt Befehlsblock.
- Repositorybezogene Blöcke betreten den richtigen Checkout explizit,
  prüfen Git-Top-Level, danach tatsächlichen Ref/SHA. Bekannten absoluten
  Pfad verwenden oder sicher auflösen. Kein geratenes ~/... in Root-Shell.
  Reine Hostdiagnosen benötigen keinen künstlichen Checkout.
- Auf Ubuntu/yaVDR niemals svdrpsend.sh als Ersatzkommando vorschlagen.
  Tatsächliches natives Kommando und Syntax prüfen; keine erfundenen
  Make-/Systemd-/SVDRP-Befehle.
- Keine Fail-fast-/Unset-Abbruchsemantik in Test-, Diagnose-, Update- und
  Installationsblöcken. Interaktive Shell erhalten. Jeden Block beginnen mit:

```bash
set +e
set +u
set +o pipefail
```

Vor Abschluss prüfen, ob das beauftragte Gesamtziel tatsächlich erreicht
wurde; verbleibende unverifizierte Ebenen und echte externe Hindernisse
konkret ausweisen.
