# Small Giveaway Bike 1 – iOS Legacy Edition

Een kleine, eigen open-wereld stadsgame in GTA-stijl voor de **iPad 2 (A1395, iOS 9.3.5, ARMv7)**.
De game is geschreven in C en Objective-C met **OpenGL ES 2.0** (GLKit). Hij werkt volledig offline. Er zit geen webview, Metal of SwiftUI in.

> **Status (18-11-2026):** de IPA bouwt (armv7, iOS 9.0+) en de hele game draait zonder crash in de Linux-test (CI, Mesa).
> **Op de echte iPad is hij nog NIET getest.** De iPad was tijdens het bouwen niet via USB aangesloten.
> Hoe snel en hoe goed hij op de iPad 2 draait, weten we pas na de iPad-test (zie onderaan).

## Bestanden
| Wat | Waar |
|---|---|
| De app | `SmallGiveawayBike1.ipa` |
| Logo (origineel van de gebruiker) | `assets/logo_source.jpg` |
| Uitgeknipt logo, achtergrond, iconen | `Resources/` (gemaakt met `tools/make_assets.py`) |
| Engine en spel (C) | `Game/` (wiskunde, meshes, stad, modellen, spellogica) |
| Renderer, menu's, HUD, besturing | `Render/` |
| iOS-schil (GLKit, touch, opslag) | `App/` |
| Linux-testopstelling | `Tests/preview_linux.c`, `tools/linux_assets.py` |
| iPad-test | `tools/ipad_test.py` |
| Techniekonderzoek (fase 1) | `docs/TECHNIEK.md` |
| Bouwen | `.github/workflows/build.yml` (private repo `ikbenyfer/SmallGiveawayBike1`) |

## Installeren
```
py -3.12 tools/ipad_test.py install SmallGiveawayBike1.ipa
```
Hiervoor gebruikt het script de bestaande `iPadInstaller`. Op de iPad moeten Phœnix (jailbreak) en AppSync Unified actief zijn. De app heet op het beginscherm **Giveaway Bike**.

## Wat zit erin
- **Intro en menu:** het logo komt in beeld vanuit zwart. Daarna verschijnt de steeg-achtergrond, met een flikkerende lamp, zwevende stofjes en een zachte rode gloed. Onderaan staat een laadbalk die soepel meeloopt, met tips. Daarna volgt het hoofdmenu **PLAY / SETTINGS / CREDITS** in dezelfde stijl.
- **De stad** (404 x 404 m, 7 x 7 blokken) heeft wegen met kruispunten, zebrapaden en middenstrepen. Er zijn flats en kantoren met verlichte ramen, winkelstraten met luifels (waaronder een pizzeria), steegjes met containers en graffiti, een buitenwijk met huizen, tuinen en hekken, parken met vijver en podium, parkeerplaatsen, een garage met tankstation en een plein met het gouden Giveaway Bike-standbeeld. Rondom de stad staan een bomenrand en een hek.
- **Lopen en springen.** Je kunt **fietsen** en **autorijden**, en in- en uitstappen met de knop RIDE/DRIVE/EXIT. Je kunt ook een auto uit het verkeer pakken: de bestuurder loopt dan weg.
- **Voetgangers** lopen over de stoep, springen opzij voor voertuigen en vallen om als je ze raakt (geen geweld). Het **verkeer** rijdt op rijstroken, slaat af op kruispunten en remt voor obstakels.
- **5 missies**, die je start via de gele markers op de minimap:
  Welcome Ride (fiets, $100), Pizza Panic (bezorgen met tijdslimiet, $250), Street Sprint (fietsrace, $300), Joyride Return (rode auto naar de garage, $400) en The Big Giveaway (5 cadeaus verzamelen, $750).
  Speel je een missie opnieuw, dan krijg je de halve beloning.
- **Geld** verdien je met missies en met 18 stapels geld op de stoep, die na 2 minuten terugkomen. Je geld en je voortgang worden bewaard.
- **Minimap** linksboven. Die draait mee met de camera en toont missiepunten en fietsen.
- **Besturing:** een linker joystick om te bewegen of te sturen, een rechter joystick voor de camera, en de knoppen JUMP, RIDE/DRIVE/EXIT, GAS en BRAKE.
- **Instellingen:** Graphics (laag, midden of hoog: zichtafstand 95, 135 of 180 m, 6, 10 of 14 voetgangers, 4, 7 of 10 auto's), tijd van de dag (schemer, nacht of dag), camerasnelheid, camera-Y omkeren, FPS tonen en voortgang wissen.

## Prestaties: zo is het gebouwd
Het spel draait op vaste 30 FPS. De wereld heeft geen texturen, alleen kleuren per vertex met vast ("gebakken") licht en zelf-lichtgevende ramen. Er zijn alleen blob-schaduwen en geen nabewerking. De stad zit in 64 statische VBO's met frustum- en afstandsculling en mist. Voetgangers en auto's worden hergebruikt (object pooling). De physics zijn simpel (cirkels tegen boxen).
In de Linux-test tekent een frame **ongeveer 50-90 draw calls en 11-26 duizend driehoeken**, afhankelijk van de grafische instelling. De FPS-cijfers uit de Linux-test zeggen **niets** over de iPad: daar is de tijd gesimuleerd.

## Getest
| Fase | Linux-CI (Mesa, geen iPad) | Echte iPad 2 |
|---|---|---|
| 2. Minimale app, 3. ARMv7-build | Bouwt; het binaire bestand is armv7 met minimaal iOS 9.0 | **nog niet getest** |
| 4. 3D-speler, 5. camera en touch | Lopen en springen werken in de autotest. Touch kan alleen op het apparaat getest worden. | **nog niet getest** |
| 6. Fiets, auto | Opstappen, rijden en afstappen werken in de autotest | **nog niet getest** |
| 7. Stad, 8. NPC's en verkeer | Wordt getekend, zonder crash en zonder AddressSanitizer-fouten | **nog niet getest** |
| 9. Missies | Welcome Ride gehaald, +$100 | **nog niet getest** |
| 10. Optimalisatie (FPS) | Niet meetbaar op Linux | **nog niet gemeten** |

## Testen op de iPad
1. Sluit de iPad aan, ontgrendel hem en open Phœnix.
2. `py -3.12 tools/ipad_test.py install SmallGiveawayBike1.ipa`
3. `py -3.12 tools/ipad_test.py arm`. Dit zet de automatische test klaar.
4. Open **Giveaway Bike** op de iPad en laat hem ongeveer 70 seconden met rust. Hij speelt dan zelf: lopen, springen, fiets, auto, een missie, laag/midden/hoog en nacht. Daarna gaat hij terug naar het menu.
   Wil je ook de log zien, start dan tegelijk `py -3.12 tools/ipad_test.py log 90`.
5. `py -3.12 tools/ipad_test.py fetch`. Dit haalt `device_results/autotest_report.txt` (echte FPS per onderdeel, laadtijd, geheugen) en 13 screenshots op.
6. Speel daarna zelf, om de touch-besturing te testen.

## Opnieuw bouwen
Push naar `main` in de private repo. GitHub Actions bouwt dan twee artifacts:
- `SmallGiveawayBike1`: de `.ipa` en `.deb`
- `preview`: Linux-screenshots en het rapport

Ophalen gaat met:
```
gh run download -n SmallGiveawayBike1
```
