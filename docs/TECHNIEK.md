# Fase 1: techniekonderzoek

## Doelapparaat
- iPad 2 (A1395), iOS 9.3.5, Apple A5 (ARMv7, 2 kernen, 1 GHz), 512 MB RAM.
- GPU: PowerVR SGX543MP2. Werkt met OpenGL ES 2.0. Grootste textuur: 2048x2048. Het scherm is 1024x768 en niet-retina.
- Het is een tile-based deferred renderer. Overdraw kost daardoor weinig, maar alpha-blending en veel draw calls zijn wel duur. Elke vertex kost rekenwerk, en de chip heeft weinig geheugenbandbreedte.

## Gekozen route
| Onderdeel | Keuze | Waarom |
|---|---|---|
| Taal | C99 voor de engine en de spellogica, Objective-C voor de iOS-schil | Draait op armv7 zonder extra runtime. Dezelfde C-code is op Linux te testen. |
| Graphics | OpenGL ES 2.0 via GLKit (GLKView/GLKViewController) | Werkt al vanaf iOS 5. Metal is er niet op A5, SwiftUI pas vanaf iOS 13. |
| Bouwen | Theos + L1ghtmann-toolchain + iPhoneOS14.5 SDK op GitHub Actions (`ubuntu-24.04`) | Is bewezen in de eerdere projecten (YFChess, FrontierClassic). Er is geen Mac nodig. |
| Ondertekenen | Nep-ondertekening met ldid, installeren met AppSync Unified | Het apparaat is gejailbreakt. |
| Installeren en testen | `iPadInstaller` (USB), house_arrest voor Documents, syslog_relay voor logs | Rapporten en screenshots van het apparaat komen zo op de pc. |

## Prestatieplan voor 25-30 FPS
- **Vast 30 FPS** (`preferredFramesPerSecond = 30`). Zo blijft de framerate gelijkmatig.
- **Geen texturen in de 3D-wereld.** Alles heeft een kleur per vertex. Ramen, lantaarns en lichten zijn "zelf-lichtgevende" vertices. Dat is het vaste ("gebakken") licht, met donkere randjes onderaan de muren als ambient occlusion.
- **Belichting per vertex** met één zon/maan plus omgevingslicht. Er is geen per-pixel licht, er zijn geen echte schaduwen (alleen blob-schaduwen) en er is geen nabewerking.
- **De stad is in 8x8 chunks gebakken**: per chunk één statische VBO. Chunks vallen weg op afstand en buiten het beeld (frustum culling). Mist verbergt de rand.
- **Vertex van 20 bytes** (positie als float3, normaal als byte4, kleur als ubyte4).
- **Weinig dynamische objecten**: 6, 10 of 14 voetgangers en 4, 7 of 10 verkeersauto's, afhankelijk van de grafische instelling. Ze worden hergebruikt (object pooling): wie ver weg is, verschijnt opnieuw in de buurt.
- **Eenvoudige physics**: cirkels tegen boxen in een raster van 8 m, en een arcade-model voor de voertuigen.
- **De 2D-interface in één batch** met hooguit een paar draw calls per frame.
- **Geheugen**: de stadsgeometrie gaat na het uploaden uit het CPU-geheugen. De achtergrond van het laadscherm is 1408x768 (precies de schermhoogte).

## Testaanpak (eerlijk)
1. De CI op Linux compileert alles met strenge waarschuwingen en AddressSanitizer. Daarna draait hij de hele game met de automatische test op Mesa (llvmpipe) en maakt voorbeeldbeelden. **Dit is geen iPad-test.** Zo vang je alleen crashes en duidelijke beeldfouten vóórdat je naar de iPad gaat.
2. Op de iPad: installeer met `tools/ipad_test.py install`, zet de autotest klaar met `arm`, open de app en haal het rapport op met `fetch`. Het rapport bevat de echte FPS per onderdeel (lopen, fiets, auto, missie, laag/midden/hoog, nacht), de laadtijd en het geheugen, plus screenshots uit de app zelf (glReadPixels).
3. Tijdens het gewone spelen logt de app elke 10 s `SGB-PERF fps=...` in de syslog (`tools/ipad_test.py log`).
