# Digitalni vrt — plan i sljedeći koraci

## Vizija
Statična, maksimalistička prezentacija koja je dostupna bez tuđih API-ja. Svaki opis mora biti
samostalan i vrijedan čitanja, a kompletna struktura mora voditi posjetitelja od “raja” do “pakla”
bez da izgubi kontekst.

## Već implementirano
- Jednostranični “Hero” s objašnjenjem pristupa (neovisnost, disciplina, iskrenost).
- Primarna tablespace navigacija za gotove radove (do 3 stupca).
- Vertikalna mapa “Raja → Pakao” s punim popisom tema, mikro-manuskripata i eseja.
- Memoria Maris: 9 činova × 4 scene, svaki čin personificiran.
- Memoria Maris poglavlje s lokalnim audio/cover placeholderima i dvo-stupčastom analizom.
- Filozofski blok (Schopenhauer, Mainländer, moja filozofija).
- “Svi projekti” podstranica s tri stupca dubine.
- Footer s linkovima na blog i store.
- Web Animations API prototip: synesthetic player (sunset + bijeli zec) vezan uz audio output.
- Dinamičan visinomjer koji mapira scroll na metrički raspon (bez hard-coded brojeva).

## To-do (sljedeće faze)
1. **Web Animations API (bez 3rd party)**
   - Synesthetic Player: tipografija (italic/bold/size) sinkronizirana s audio outputom.
2. **Raja → Pakao vizual**
   - “Unrolling Earth” koncept (ako izvedivo samo HTML/CSS/JS bez dodatnih datoteka).
3. **Manuskripti i mikro-dosjei**
   - Zrakoperka: profile likova, didaskalije, motivacije (linkovi u tekstu).
   - Memoria Maris: svaka scena dobiva kratki tekst i referencu na realitetnu terapiju.
4. **Eseji i kritike**
   - “Roditelji XY” (cjeloviti esej), “Cenzura”, “Anatomija bijele laži”, “The Substance”.
5. **Alati**
   - Word Processor App: lokalna konverzija (client-side) ili jasna dokumentacija flowa.

## Potrebni alati (bez third-party ograničenja)
- HTML/CSS (layout, typography, gradients)
- Vanilla JS (scroll metrički raspon, animacije, audio sync)
- Web Animations API + Audio API
- SVG (ikone i jednostavni grafički prikazi)

## Bilješke
- Sve ostaje statično dok se ne definira slobodan (self-hosted) JS.
- Svaki opis mora funkcionirati kao samostalna mikro-priča.
