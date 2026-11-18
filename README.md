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
- **19 missies**: je start ze alleen via het **missiekeuzescherm** (MISSIONS in het hoofdmenu of het pauzemenu); in de stad staan geen missie-cirkels meer. Daar sta je meteen op de goede plek en zit je zo nodig al op een fiets of in een auto:
  Welcome Ride ($100), Pizza Panic ($250), Street Sprint ($300), Joyride Return ($400), The Big Giveaway ($750), Park Run (te voet, $200), Taxi Rush ($350), Alley Shortcut ($350), Cop Dodge ($400) en Grand Tour ($500).
  Nieuw (missie 12-19):
  - NIGHT DELIVERY: pakketjes bezorgen op de fiets, $450.
  - CASH COLLECTOR: 6 geldzakken zoeken, $600.
  - DOWNTOWN DASH: te voet, $350.
  - SPEED TRAP: autorace, $700.
  - CORNER STORE STICK-UP: overval op de fietsenwinkel, wapen nodig, $1500.
  - GETAWAY DRIVER: de bankploeg wegrijden, $2500.
  - MOST WANTED: 3 sterren afschudden, $1500.
  - TOUR DE STAD: grote fietsrace, $800.

  Het missiescherm heeft nu 2 pagina's (< PREV / NEXT >).
  Speel je een missie opnieuw, dan krijg je de halve beloning.
- **Politie en sterren**: rijd je iemand aan of steel je een auto uit het verkeer, dan krijg je een ster (maximaal 3). Per ster komt er een politieauto met zwaailicht achter je aan, die ook op de minimap staat. Blijf 10 seconden ver weg van de politie, dan verlies je een ster. Sta je ongewapend (of in een auto) stil naast de politie, dan word je **BUSTED**. Met een wapen in de hand arresteren ze je niet, maar schieten ze: je betaalt een boete (10% van je geld, minstens $50) en begint opnieuw op het plein.
- **Health bar** onder je geld. Je verliest health door harde botsingen, door aangereden te worden en als de politie je ramt. Na 5 seconden zonder schade herstelt je health vanzelf. Is je health op, dan ben je **KNOCKED OUT** en begin je opnieuw op het plein.
- **PEGI 18: wapens en een bankoverval.** Alles in dezelfde blokkerige stijl, zonder bloed. Wie geraakt wordt, valt om.
  - **BLOCK ARMS** (rode marker, in de winkelstraat naast het plein): loop naar binnen en je kunt kopen:

    | Item | Prijs |
    |---|---|
    | PISTOL | $250 |
    | SMG | $900 |
    | SHOTGUN | $700 |
    | Kogels voor al je wapens | samen per pak |
    | Kogelwerend vest | $300 |

    Het vest vangt 75% van de schade op tot het op is. Je wapens, kogels en vest worden bewaard.
  - **Schieten:** de knop **FIRE** richt vanzelf op het dichtstbijzijnde doel in je kijkrichting. Met **GUN** wissel je van wapen, en "FISTS" is geen wapen. Rechtsboven zie je je kogels. Schieten in de buurt van mensen, iemand raken of de politie raken geeft sterren. Mensen in de buurt rennen weg.
  - **Agenten te voet:** vanaf 2 sterren stappen er agenten (blauw uniform, pet) uit de politieauto's. Ze rennen naar je toe en schieten terug. Je kunt politieauto's ook kapotschieten; die blijven dan donker staan.
  - **De Giveaway Bank** staat aan het plein, naast het standbeeld: een stenen gebouw met zuilen en een gouden bord. Aan de achterkant zit de kluisdeur, met een rode alarmlamp.
  - **Missie 11: THE BIG HEIST** ($5000, je hebt een wapen nodig):
    1. Ga naar de kluisdeur achter de bank en blijf 15 seconden staan terwijl de kluis wordt opengeboord. Bovenin zie je een balk. Het alarm gaat meteen af, met 3 sterren, en de politie komt, ook te voet.
    2. Schud de politie af en breng het geld naar de garage, je schuilplaats.
- **Auto's kopen en tunen.**
  - **GIVEAWAY MOTORS** is een autodealer met een oranje marker. Hij staat op het terrein links van de winkelstraat, met 3 auto's in de etalage. Loop erin en je kunt kopen:

    | Auto | Prijs | Topsnelheid |
    |---|---|---|
    | CITY HATCH | $1.500 | 68 km/h |
    | RHINO V8 (muscle car) | $6.000 | 90 km/h |
    | VORTEX GT (sportwagen) | $9.000 | 100 km/h |
    | BOLT X (supercar) | $20.000 | 122 km/h |

    Gekochte auto's zijn van jou. Ze worden bewaard en staan op het terrein van de dealer. Er passen maximaal 4 in je garage.
  - **BLOCK CUSTOMS** heeft een blauwe marker en is de linker deur van de garage bij het tankstation. Rijd er langzaam in met een auto en je kunt aanpassen:

    | Onderdeel | Prijs | Wat het doet |
    |---|---|---|
    | Lak | $200 | 10 kleuren |
    | Motor | $1.000 / $2.500 / $5.000 (3 niveaus) | +8% topsnelheid en +15% acceleratie per niveau |
    | Handling | $800 / $1.600 / $3.200 (3 niveaus) | beter sturen en remmen |
    | Neon onder de auto | $500 | 5 kleuren |
    | Spoiler | $400 | |

    Op je eigen auto blijft alles bewaard. In de test haalde de BOLT X 123 km/h, en na de motor op niveau 3 153 km/h.
- **REAL NL (aparte modus, knop in het hoofdmenu):** rijd door het echte Nederland.
  - **Ingebouwd en offline: heel Heemskerk en Beverwijk**, inclusief Wijk aan Zee, de duinen en het strand. Dat zijn 58.107 gebouwen met hun echte hoogte (3D BAG), alle wegen met straatnamen, water, groen en 1.897 postcodes, samen maar 2 MB in de app (`Resources/nl_hb.pak`). Je start via de knoppen rechts (Beverwijk centrum, de stations, Heemskerk centrum, Wijk aan Zee, de Bazaar) of je typt een postcode uit die gemeentes. Daar is geen internet voor nodig.
  - **Streaming terwijl je rijdt:** de wereld bestaat uit tegels van 250 × 250 m. Rondom je liggen er altijd 3 × 3 klaar. Komt er een nieuwe tegel in beeld, dan laadt een achtergrond-thread die, zodat het spel niet hapert. Binnen Heemskerk/Beverwijk komt de tegel uit de ingebouwde kaart. Daarbuiten komt hij uit je downloads, of anders van internet (3D BAG en PDOK), en wordt hij daarna bewaard. Zonder internet zie je buiten de regio "NO MAP DATA HERE".
  - **Andere postcodes:** typ een postcode en tik op PLAY. Je gaat erheen en het land laadt bij terwijl je rijdt.
  - **Downloadmodus:** DOWNLOAD bewaart een gebied van 1,25 × 1,25 km (25 tegels) rond een postcode. Groningen was zo bijvoorbeeld 162 kB. Dat gebeurt op de achtergrond, met een wachtrij, en daarna kun je het offline spelen. Rechts staat de lijst met gedownloade gebieden: tik om te spelen, of tik op X en dan op ? om te verwijderen.
  - **Grote kaart:** tik op de minimap. Je kunt slepen, in- en uitzoomen met + en −, en met ME terug naar jezelf. In REAL NL tik je een plek aan en ga je er met **GO HERE** meteen heen. Heemskerk en Beverwijk staan er helemaal op; tegels die je elders hebt bezocht, komen er vanzelf bij. In de verzonnen stad toont de grote kaart de hele stad.

  De onderdelen die de bouw en de bronvermelding vastleggen:
  - `tools/build_region_pack.py` maakt het pakket opnieuw (op de pc, met internet).
  - Bronvermelding: 3D BAG (TU Delft / 3DGI), BAG en TOP10NL (Kadaster, PDOK), gemeentegrenzen en postcodes (PDOK). Alles valt onder CC BY 4.0.
  - iOS 9 kent de rootcertificaten van 3D BAG niet. Daarom zitten ISRG Root X1/X2 en DigiCert Global Root G2 in de app als **extra** vertrouwensanker; de normale controle blijft aan. Lukt 3D BAG toch niet, dan valt de app terug op PDOK BAG met geschatte hoogtes.
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

**Linux-CI (Mesa, geen iPad), versie 2:** alles compileert zonder waarschuwingen en AddressSanitizer vindt niets. De autotest gaf voor REAL NL:
- **Ingebouwd:** Beverwijk, Breestraat: 9 tegels uit het pakket, 1649 gebouwen, klaar in 1,5 s. Na 600 m reizen lagen de nieuwe tegels binnen 0,2 s klaar.
- **Grote kaart:** werkt.
- **Online (1012JS, Amsterdam):** tegels komen van internet en laden onderweg bij.
- **Downloadmodus:** Groningen 9711LM, 25 tegels, 0 mislukt, 162 kB. Daarna offline gespeeld (9 tegels uit opslag, 1607 gebouwen) en weer verwijderd.

Let op: 9 tegels zijn ongeveer **300 tot 480 duizend vertices**, veel meer dan de verzonnen stad. Of de iPad 2 dat op 25 tot 30 FPS haalt, is **nog niet gemeten**. Zo niet, dan moeten de ramen of de laadafstand omlaag.

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
