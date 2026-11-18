# Small Giveaway Bike 1 – iOS Legacy Edition

Een kleine, eigen open-wereld stadsgame in GTA-stijl voor de **iPad 2 (A1395, iOS 9.3.5, ARMv7)**.
De game is geschreven in C en Objective-C met **OpenGL ES 2.0** (GLKit). Hij werkt volledig offline. Er zit geen webview, Metal of SwiftUI in.

> **Status (18-11-2026):**
> - **Versie 1** (stad, fiets, auto, 5 missies, menu) is op de echte iPad 2 getest met de automatische test. Resultaat: **stabiel 30 FPS** op laag, midden en hoog, laden in 0,3 s, 21-31 MB geheugen, geen crash. Zie de tabel onderaan.
> - **Versie 2** voegt politie, sterren, een health bar, 10 missies en een missiekeuzescherm toe. Hij voegt ook de **REAL NL-modus** toe: je typt een postcode en rijdt dan door de echte straten met de echte gebouwen. Deze versie is gebouwd en slaagt voor de Linux-test, maar is **nog niet op de iPad getest**. Vooral of het downloaden op iOS 9 lukt, moet de iPad nog laten zien.

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
- **10 missies**: je start ze via de gele markers in de stad, of via het **missiekeuzescherm** (MISSIONS in het hoofdmenu of het pauzemenu). Daar sta je meteen op de goede plek en zit je zo nodig al op een fiets of in een auto:
  Welcome Ride ($100), Pizza Panic ($250), Street Sprint ($300), Joyride Return ($400), The Big Giveaway ($750), Park Run (te voet, $200), Taxi Rush ($350), Alley Shortcut ($350), Cop Dodge ($400) en Grand Tour ($500).
  Speel je een missie opnieuw, dan krijg je de halve beloning.
- **Politie en sterren**: rijd je iemand aan of steel je een auto uit het verkeer, dan krijg je een ster (maximaal 3). Per ster komt er een politieauto met zwaailicht achter je aan, die ook op de minimap staat. Blijf 10 seconden ver weg van de politie, dan verlies je een ster. Sta je stil naast de politie, dan word je **BUSTED**: je betaalt een boete (10% van je geld, minstens $50) en begint opnieuw op het plein.
- **Health bar** onder je geld. Je verliest health door harde botsingen, door aangereden te worden en als de politie je ramt. Na 5 seconden zonder schade herstelt je health vanzelf. Is je health op, dan ben je **KNOCKED OUT** en begin je opnieuw op het plein.
- **REAL NL (aparte modus, knop in het hoofdmenu):** typ een Nederlandse postcode op het eigen toetsenbord, bijvoorbeeld 1012 JS. De app haalt dan een gebied van 500 × 500 m rond die postcode op:
  - **Gebouwen met hun echte hoogte** uit de 3D BAG (TU Delft/3DGI, LoD 1.2).
  - **Straten met straatnamen, water en landgebruik** uit PDOK TOP10NL (Kadaster). Fietspaden zijn rood.
  - De postcode wordt opgezocht via de PDOK Locatieserver.

  Je loopt, fietst of rijdt erdoorheen. Bovenaan zie je de straat waar je bent, en de minimap is gemaakt uit dezelfde data. Er zijn hier geen missies, politie of verkeer: het is vrij rondrijden.

  De eerste keer heb je **Wi-Fi** nodig. Daarna staat het gebied op de iPad, in `Documents/nl/`, en werkt het ook offline. Onder RECENT staan de laatste 5 postcodes.

  iOS 9 vertrouwt het certificaat van de 3D BAG-server niet zelf, omdat het de nieuwere Let's Encrypt-root niet kent. Daarom zitten de officiële rootcertificaten in de app: ISRG Root X1/X2 en DigiCert Global Root G2, met gecontroleerde SHA-256-vingerafdruk. Ze worden alleen als **extra** vertrouwensanker gebruikt. De systeemcontrole en de hostnaamcontrole blijven gewoon aan. Lukt de 3D BAG toch niet, dan valt de app terug op BAG-plattegronden van PDOK met **geschatte** hoogtes. Dat zie je dan ook in beeld staan.

  Bronvermelding: 3D BAG (TU Delft / 3DGI), BAG en TOP10NL (Kadaster, via PDOK). Alles valt onder CC BY 4.0.
- **Geld** verdien je met missies en met 18 stapels geld op de stoep, die na 2 minuten terugkomen. Je geld en je voortgang worden bewaard.
- **Minimap** linksboven. Die draait mee met de camera en toont missiepunten en fietsen.
- **Besturing:** een linker joystick om te bewegen of te sturen, een rechter joystick voor de camera, en de knoppen JUMP, RIDE/DRIVE/EXIT, GAS en BRAKE.
- **Instellingen:** Graphics (laag, midden of hoog: zichtafstand 95, 135 of 180 m, 6, 10 of 14 voetgangers, 4, 7 of 10 auto's), tijd van de dag (schemer, nacht of dag), camerasnelheid, camera-Y omkeren, FPS tonen en voortgang wissen.

## Prestaties: zo is het gebouwd
Het spel draait op vaste 30 FPS. De wereld heeft geen texturen, alleen kleuren per vertex met vast ("gebakken") licht en zelf-lichtgevende ramen. Er zijn alleen blob-schaduwen en geen nabewerking. De stad zit in 64 statische VBO's met frustum- en afstandsculling en mist. Voetgangers en auto's worden hergebruikt (object pooling). De physics zijn simpel (cirkels tegen boxen).
In de Linux-test tekent een frame **ongeveer 50-90 draw calls en 11-26 duizend driehoeken**, afhankelijk van de grafische instelling. De FPS-cijfers uit de Linux-test zeggen **niets** over de iPad: daar is de tijd gesimuleerd.

## Getest
**Op de echte iPad 2, versie 1, automatische test (18-11-2026, 10:43):**

| Onderdeel | Gemiddeld | Systeemlog, 10 s-vensters zonder screenshot |
|---|---|---|
| Lopen | 27,5 fps* | 30,0 fps, traagste frame 29,7 fps |
| Fiets | 29,2 fps* | 30,0 / 29,7 |
| Auto | 29,2 fps* | 30,0 / 29,8 |
| Missie (Welcome Ride gehaald, +$100) | 28,6 fps* | - |
| Kwaliteit laag / midden / hoog | 29,1 / 29,0 / 28,9 fps* | hoog: 30,0 / 29,9 (23 duizend driehoeken, 81 draw calls) |
| Nacht (hoog) | 28,9 fps* | - |

\* Bij het gemiddelde telt het frame mee waarin de test een PNG-screenshot opsloeg. Dat ene frame duurde ongeveer 200 ms. Vanaf versie 2 telt de test die frames niet meer mee. De systeemlog, waarin geen screenshots vallen, laat steeds 30 FPS zien.
Verder: laden 0,3 s, geheugen ongeveer 21-31 MB, geen crash. In-, uitstappen en springen werkten. Screenshots staan in `device_results/`.
Wat er na de iPad-test is aangepast: muren aan de schaduwkant waren bijna zwart, dus er is meer omgevingslicht. **De touch-besturing met je vingers is nog niet door een mens getest.**

**Linux-CI (Mesa, geen iPad), versie 2:** alles compileert zonder waarschuwingen en AddressSanitizer vindt niets. De autotest doorloopt onder meer de REAL NL-modus met postcode 1012JS: dat geeft 703 gebouwen met echte hoogtes (de hoogste is 35 m), 179 wegen en grachten, laden en bouwen in 2,7 s, en tijdens het rijden de straatnaam "Warmoesstraat". Het gebied heeft wel ongeveer 290 duizend vertices, veel meer dan de verzonnen stad. Of de iPad 2 dat op 30 FPS haalt, moet de iPad-test nog uitwijzen.

Verder doorloopt de autotest:
- lopen, fiets en auto, en missie 1 (gehaald)
- 2 sterren: er komen 2 politieauto's
- schade: health gaat van 100 naar 70
- missiekeuze: Grand Tour start met de speler al in een auto

## Testen op de iPad
1. Sluit de iPad aan, ontgrendel hem en open Phœnix.
2. `py -3.12 tools/ipad_test.py install SmallGiveawayBike1.ipa`
3. `py -3.12 tools/ipad_test.py arm`. Dit zet de automatische test klaar.
4. Open **Giveaway Bike** op de iPad en laat hem ongeveer 90 seconden met rust. Hij speelt dan zelf: lopen, springen, fiets, auto, een missie, laag/midden/hoog en nacht. Daarna gaat hij terug naar het menu.
   Wil je ook de log zien, start dan tegelijk `py -3.12 tools/ipad_test.py log 90`.
5. `py -3.12 tools/ipad_test.py fetch`. Dit haalt `device_results/autotest_report.txt` (echte FPS per onderdeel, laadtijd, geheugen) en 16 screenshots op.
6. Speel daarna zelf, om de touch-besturing te testen.

## Opnieuw bouwen
Push naar `main` in de private repo. GitHub Actions bouwt dan twee artifacts:
- `SmallGiveawayBike1`: de `.ipa` en `.deb`
- `preview`: Linux-screenshots en het rapport

Ophalen gaat met:
```
gh run download -n SmallGiveawayBike1
```
