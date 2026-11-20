# Small Giveaway Bike 1 – iOS Legacy Edition

Een kleine, eigen open-wereld stadsgame in GTA-stijl voor de **iPad 2 (A1395, iOS 9.3.5, ARMv7)**.
De game is geschreven in C en Objective-C met **OpenGL ES 2.0** (GLKit). Hij werkt volledig offline. Er zit geen webview, Metal of SwiftUI in.

> **Status (18-11-2026):**
> - **Versie 1** (stad, fiets, auto, 5 missies, menu) is op de echte iPad 2 getest met de automatische test. Resultaat: **stabiel 30 FPS** op laag, midden en hoog, laden in 0,3 s, 21-31 MB geheugen, geen crash. Zie de tabel onderaan.
> - **Versie 2** voegt politie, sterren, een health bar, 10 missies en een missiekeuzescherm toe. Hij voegt ook de **REAL NL-modus** toe: je typt een postcode en rijdt dan door de echte straten met de echte gebouwen. Deze versie is gebouwd en slaagt voor de Linux-test, maar is **nog niet op de iPad getest**. Vooral of het downloaden op iOS 9 lukt, moet de iPad nog laten zien.

## Downloads (Releases)
Na elke wijziging op `main` maakt GitHub Actions automatisch een nieuwe release (`v1.2.<buildnummer>`). Die release bevat:
- **iPad:** `SmallGiveawayBike1.ipa` (en een `.deb`)
- **Windows:** `SmallGiveawayBike1-Windows.zip`
- **Linux:** `SmallGiveawayBike1-Linux.tar.gz`
- **Android:** `SmallGiveawayBike1-Android.apk`

## v4.5.0: stemmen en cutscenes

- **Cutscenes voor elke missie:** voordat een missie begint, staat je contact voor je en vertelt hij of zij de opdracht. Je ziet zwarte filmbalken, camerashots over de schouder en ondertitels. Tik (of druk op spring/actie) om over te slaan.
- **Personages met een eigen stem:** de Giveaway Guy, Mama Rosa, Coach Dre, Rico, Big T, Nina, de politie, de verkopers en jijzelf.
- **Tijdens de missies:** je contact praat mee bij belangrijke checkpoints, bij het alarm, als de kluis open is, en bij gehaald of mislukt.
- **Losse stemmen in de stad:**
  - Je zegt iets bij instappen, als de politie komt of je ze kwijt bent, bij gearresteerd worden en als je geld oppakt.
  - Voetgangers roepen als er geschoten wordt.
  - Verkopers groeten je als je binnenkomt.
  - Bij de overval van de GIVEAWAY MART zeg je "Empty the register!" en de kassamedewerker reageert.
- Alle gesproken tekst staat ook als ondertitel in beeld. De muziek gaat zachter als er iemand praat.
- De stemmen zijn lokaal gemaakt met Kokoro TTS (`tools/make_voices.py`).

## v4.0.0: winkels van binnen + GIVEAWAY MART

- Je loopt nu echt naar binnen in de winkels: BLOCK ARMS (wapens), GIVEAWAY BURGER, BLOCK GYM, DRIP en BLING. Loop door de deur-marker en je staat in de winkel.
- Binnen staat een verkoper achter de toonbank. Loop naar de gekleurde marker bij de toonbank, dan opent het winkelmenu. Met de groene marker bij de deur ga je weer naar buiten.
- Nieuw: de **GIVEAWAY MART**, een supermarkt met schappen, koelingen, groente en fruit en een kassa. Op de kaart is het de groene stip met een $.
- Je kunt de kassa van de GIVEAWAY MART elk moment overvallen: loop naar de rode marker bij de kassa en je krijgt **$10.000** en 2 politiesterren. De kassa vult zich weer als je de winkel uitloopt en opnieuw naar binnen gaat.
- Binnen kan de politie je niet arresteren, maar ze wachten buiten bij de deur.
- BLOCK SPRAY blijft een drive-in.

## Android-versie
Dit is dezelfde game voor Android-telefoons en -tablets (Android 5.0 of nieuwer, ARM 32- en 64-bit). Hij is gebouwd met SDL2 en OpenGL ES 2, net als op de iPad.
- **Installeren:** download de `.apk` uit de release op je telefoon en open hem. Android vraagt dan om **installeren uit onbekende bronnen** toe te staan.
- **Besturing:** dezelfde touch-knoppen als op de iPad. Een Bluetooth- of USB-controller werkt ook, net als op de pc, inclusief PS4- en Xbox-controllers. De **terug-knop** van Android werkt als pauze/terug.
- **Beeld:** breedbeeld (16:9, 20:9 enzovoort) vult het hele scherm. Je ziet dan meer van de stad opzij; de menu's staan in het midden.
- **Opslag:** in de app zelf. Verwijder je de app, dan ben je je saves kwijt.
- **Online en REAL NL** werken ook, via wifi of mobiel internet.

## Breedbeeld (16:9) op Windows, Linux en Android
- De game werkt nu op elk breed scherm. Het venster op de pc start als **1280x720 (16:9)**. Je kunt het vergroten, of met **F11** volledig scherm maken.
- In het spel zie je meer van de stad opzij. De knoppen, de minikaart en het geld staan aan de randen van het scherm.
- De menu's en winkels zijn gemaakt voor 4:3. Ze staan in het midden, en de achtergrond loopt door tot de rand.
- Op de iPad (4:3) verandert er niets.

## Linux-versie
Dit is de 64-bit versie van dezelfde game, gebouwd met SDL2 en OpenGL.
- **Eenmalig installeren:** `sudo apt install libsdl2-2.0-0 libsdl2-mixer-2.0-0 libcurl4`
- **Starten:** pak het bestand uit met `tar xzf SmallGiveawayBike1-Linux.tar.gz` en start daarna `./SmallGiveawayBike1`.
- **Besturing:** hetzelfde als op Windows.
- **Opslag:** `~/.local/share/SmallGiveawayBike1`

## Windows-versie
Er is ook een **Windows-versie** (64-bit), met dezelfde game en alle onderdelen: de stad, de missies, de politie, wapens, auto's, REAL NL met Heemskerk + Beverwijk, streaming en downloads, muziek en geluid.

**Installeren:** download `SmallGiveawayBike1-Windows.zip` (zie Releases), pak de zip uit en start `SmallGiveawayBike1.exe`. De map `data` en de twee `.dll`-bestanden moeten naast de exe blijven staan.

**Besturing:**

| Toets | Wat |
|---|---|
| W A S D of pijltjes | lopen; in een voertuig: W gas, S rem/achteruit, A/D sturen |
| Muis | rondkijken (in het spel wordt de muis vastgehouden; Esc geeft hem terug) |
| Linkermuisknop | schieten |
| Spatie | springen |
| E of F | in- en uitstappen |
| Q | ander wapen |
| M of Tab | grote kaart (slepen = schuiven, scrollwiel = zoomen) |
| Esc | pauze / terug |
| F11 | volledig scherm |

Menu's bedien je met de muis. Een postcode typ je op het toetsenbord.

**Opslag:** voortgang, instellingen en downloads staan in `%APPDATA%\SmallGiveawayBike1`. Daar staat ook `log.txt`, handig als er iets misgaat.

**Hoe hij gebouwd is:** de Windows-versie gebruikt dezelfde C-code, met SDL2 (venster, invoer), SDL2_mixer (geluid), desktop-OpenGL 2.1 en WinHTTP (kaartdata). Hij wordt gebouwd in GitHub Actions met MinGW (`Win/`, job "Windows-versie").

**Getest op een Windows 11-pc (AMD Radeon):** de volledige automatische test liep goed:
- de stad, de missies, de politie en de overval
- de autodealer en BLOCK CUSTOMS
- REAL NL: ingebouwd, online via internet, downloaden en offline spelen
- gemiddeld 60 FPS

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
| Bouwen | `.github/workflows/build.yml` (iOS-app, Windows-, Linux- en Android-versie en Linux-test) |
| Android | `Android/` (Gradle, ndk-build, manifest, `SGBActivity.java`) + `Win/main_win.c` met `__ANDROID__` |
| Windows-versie | `Win/` (SDL2, toetsenbord/muis, WinHTTP) |

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
  - **Schieten:** de knop **FIRE** richt vanzelf (auto-aim): **eerst op politie** (agenten en politieauto's), en alleen als die er niet is op gewone mensen. In SETTINGS zet je **AUTO AIM** uit. Dan schiet je recht vooruit in de kijkrichting van de camera en raak je alleen wat precies in die lijn staat. Met **GUN** wissel je van wapen, en "FISTS" is geen wapen. Rechtsboven zie je je kogels. Schieten in de buurt van mensen, iemand raken of de politie raken geeft sterren. Mensen in de buurt rennen weg.
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
    | STREET 600 (motor) | $4.500 | 97 km/h |
    | DRAGON 1000R (superbike) | $14.000 | 130 km/h |

    Motoren tellen bij missies als motorvoertuig (net als een auto) en niet als fiets. Bij BLOCK CUSTOMS kun je ze niet aanpassen.

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
- **Muziek en geluid.**
  - Tijdens het spelen draait je eigen nummer "Boulevard Drift" (`Resources/music.mp3`) in een lus. In het hoofdmenu staat de muziek stil.
  - **Geluidseffecten**, allemaal zelf gemaakt met synthese (`tools/make_sounds.py`, geen samples van anderen):
    - schoten (pistool, SMG, shotgun), een leeg magazijn, en de schoten van de politie
    - een sirene die harder klinkt naarmate de politie dichterbij komt
    - mensen die gillen als ze geraakt of aangereden worden, of wegrennen
    - de motor (toonhoogte en volume volgen je snelheid) en het tikken van de fietsketting
    - botsingen, in- en uitstappen, springen en geld oppakken
    - checkpoints, missie geslaagd of mislukt, BUSTED en KNOCKED OUT
    - het bankalarm, de kassa bij aankopen en menuklikjes
  - Geluiden verder weg klinken zachter.
  - In SETTINGS zet je **MUSIC** en **SOUND EFFECTS** los aan of uit.
  - De app gebruikt de iOS-audiomodus "ambient". Daardoor hoor je niets als de iPad op stil staat (schakelaar of bedieningspaneel), en loopt eventuele andere muziek gewoon door.
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
- **Controllers (Bluetooth of bedraad), op alle platformen:**
  - Op de **iPad** werken MFi-controllers (iOS 9). Koppel ze via Instellingen > Bluetooth, of sluit ze aan.
  - Op **Windows en Linux** werken Xbox-, PlayStation- en Switch Pro-controllers en andere SDL-gamepads.
  - De linker stick is om te lopen en te sturen, de rechter stick voor de camera. Standaard zijn de knoppen:
    - A = springen, Y = in- en uitstappen, B = rennen, X = ander wapen
    - RT = schieten of gas, LT = remmen
    - BACK = kaart, START = pauze
  - In menu's stuur je met de linker stick een cursor: A = tikken, B = terug, RB = snellere cursor.
  - Met een controller verdwijnen de touch-knoppen. Raak je het scherm aan, dan komen ze terug.
  - **CONTROLS** (in Settings, rechtsboven): stel elke knop zelf in. Op de pc kun je daar ook alle toetsen instellen; dubbele toetsen worden omgewisseld. RESET DEFAULTS zet alles terug.
- **Opslagslots:** PLAY opent een keuzescherm met 3 slots.
  - Per slot zie je je geld, hoeveel missies je gehaald hebt en hoe groot je huis is.
  - Een leeg slot start een nieuw spel. DELETE wist een slot; daarvoor tik je twee keer.
  - Een oude save, van voor de slots, staat automatisch in slot 1.
  - Instellingen gelden voor alle slots samen.
- **Missies op volgorde:** je begint met missie 1. De volgende missie gaat pas open als je de vorige gehaald hebt. Gehaalde missies kun je altijd opnieuw spelen.
- **Winkels in de stad** (elk met een eigen icoontje op de kaart; loop in de gekleurde ring):
  - **GIVEAWAY BURGER** (oranje): eten en drinken, van $20 tot $150. Dat geeft gezondheid, uithoudingsvermogen en een ENERGY-boost, waarmee je een tijdje rent zonder moe te worden.
  - **BLOCK GYM** (lichtblauw): voorgoed trainen.
    - **CARDIO:** je maximale uithoudingsvermogen gaat van 100 naar 130, 165 en 200.
    - **TOUGHNESS:** je krijgt 10, 20 of 30% minder schade.
    - Een personal trainer geeft je direct een energie-boost.
  - **DRIP** (roze): 7 outfits om je speler een andere look te geven. Wat je eenmaal gekocht hebt, kun je gratis wisselen.
  - **BLING** (goud): luxe spullen die in je huis verschijnen: een gouden beeld, een aquarium, een arcadekast, een neonbord en discolampen.
  - **BLOCK SPRAY** (blauw, een drive-in aan de oostrand): rijd erin met een voertuig.
    - Overspuiten kost $400 per ster, daarna ben je de politie kwijt.
    - Repareren en wassen kost $150.
    - Een nieuwe kleur kost $300.
- **Rennen en uithoudingsvermogen:**
  - Houd **RUN** ingedrukt (op Windows/Linux: Shift) om te rennen. Normaal lopen gaat 15 km/h, rennen 27 km/h.
  - Rennen kost uithoudingsvermogen: de gele balk onder je gezondheid. Is die leeg, dan ben je buiten adem en kun je pas weer rennen als hij terug op 30% staat.
- **Eigen huis** (paars huisje op de kaart, aan de oostrand van de stad):
  - **Binnen:** loop naar de voordeur (paarse ring) om naar binnen te gaan. Binnen staan een bed, een bank met tv en een bureau met laptop.
    - In het bed herstel je je gezondheid en je uithoudingsvermogen.
    - Op de laptop koop je upgrades.
    - De gele deur naast de tv gaat naar de garage.
  - **Groter maken:** van SMALL HOUSE naar FAMILY HOUSE ($25.000, met keuken en veranda) en daarna naar VILLA ($75.000, met glazen gevel, balkon, eettafel en boekenkast).
  - **Ondergrondse garage:** rijd de oprit in (paarse ring bij de garage-ingang).
    - De garage begint met 1 plek en groeit tot 4 plekken ($10.000, $20.000, $35.000).
    - Je gekochte auto's en motoren staan hier geparkeerd. Wat niet past, blijft bij GIVEAWAY MOTORS staan.
    - Rijd naar het groene EXIT-bord om weer naar buiten te gaan.
  - Als de politie je zoekt, kun je niet naar binnen.
- **GOLD RESERVE HEIST** (missie 20): de zwaarste klus. Je moet 30 seconden in de kluis blijven, met een alarm van 3 sterren. Daarna rijd je het goud langs twee hoeken van de stad en schud je de politie af bij de garage, binnen 5 minuten. De eerste keer krijg je $15.000, daarna $10.000 per keer.
- **THE ULTIMATE HEIST** (missie 21): de allermoeilijkste klus, binnen 7 minuten.
  1. Eerst beroof je de kassa van de fietsenwinkel (2 sterren).
  2. Dan moet je 45 seconden de kluis van de bank openboren (3 sterren).
  3. Daarna race je met een auto langs alle vier de hoeken van de stad.
  4. Tot slot schud je alle politie af en verstop je de buit bij de garage.

  De eerste keer krijg je $30.000, daarna $25.000 per keer.
- **THE IMPOSSIBLE JOB** (missie 22): de allerzwaarste klus, alleen voor legendes. Je hebt 9 minuten.
  1. Beroof de kassa van de fietsenwinkel (10 seconden).
  2. Boor 60 seconden de bankkluis open, terwijl je 3 sterren hebt.
  3. Race met een auto langs 6 punten door de hele stad.
  4. Ga terug de bank in voor de goudstaven (nog eens 30 seconden, opnieuw alarm).
  5. Schud alle politie af en verstop je bij de garage.

  De eerste keer krijg je $100.000, daarna $80.000 per keer.
- **ONLINE (multiplayer, peer-to-peer, max. 4 spelers)** – iPad en Windows kunnen samen spelen, er is geen server nodig.
  - **HOST A GAME** (hoofdmenu → ONLINE) start de stad als host. Wil je samen in REAL NL spelen, kies dan in het pauzemenu **HOST ONLINE**: iedereen komt dan op dezelfde postcode.
  - **Zelfde wifi:** het spel van de host verschijnt vanzelf in de lijst; tik erop om mee te doen.
  - **Via internet:** de host ziet in het pauzemenu een **uitnodigingscode** (XXXXX-XXXXX). De ander typt die code in en tikt op JOIN. De app opent zelf poort 27016 (UDP) via UPnP op de router. Lukt dat niet, zet die poort dan handmatig door naar het apparaat van de host.
  - Je ziet de anderen met hun naam en gezondheid boven het hoofd, lopend, op de fiets of in hun (getunede) auto. Schieten op elkaar werkt ook (PvP). Verkeer, voetgangers en politie zijn per speler apart.
  - **LEAVE ONLINE** in het pauzemenu stopt het samenspelen. Windows-test met twee vensters: `--mptest-host` en `--mptest-join` (elk met `--appdata <map>`).
- **Geld** verdien je met missies en met 18 stapels geld op de stoep, die na 2 minuten terugkomen. Je geld en je voortgang worden bewaard.
- **Minimap** linksboven. Die draait mee met de camera en toont missiepunten en fietsen.
- **Besturing:** een linker joystick om te bewegen of te sturen, een rechter joystick voor de camera, en de knoppen JUMP, RIDE/DRIVE/EXIT, GAS en BRAKE.
- **Instellingen:** Music aan/uit, Sound effects aan/uit, Auto aim aan/uit, Graphics (laag, midden of hoog: zichtafstand 95, 135 of 180 m, 6, 10 of 14 voetgangers, 4, 7 of 10 auto's), tijd van de dag (schemer, nacht of dag), camerasnelheid, camera-Y omkeren, FPS tonen en voortgang wissen.

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
Verder: laden 0,3 s, geheugen ongeveer 21-31 MB, geen crash. In-, uitstappen en springen werkten. (De testbeelden van het apparaat zelf staan bewust niet in deze publieke repo.)
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
5. `py -3.12 tools/ipad_test.py fetch`. Dit haalt het rapport (echte FPS per onderdeel, laadtijd, geheugen) en de screenshots op naar `device_results/`. Die map staat in `.gitignore` en komt dus niet in de repo.
6. Speel daarna zelf, om de touch-besturing te testen.

## Opnieuw bouwen
Push naar `main`. GitHub Actions bouwt dan de iOS-app, de Windows-versie en de Linux-test:
- `SmallGiveawayBike1`: de `.ipa` en `.deb`
- `SmallGiveawayBike1-Windows`: de Windows-zip
- `preview`: Linux-screenshots en het rapport

Ophalen gaat met:
```
gh run download -n SmallGiveawayBike1
```
