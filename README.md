# Submarine Noir Prototype (C++ + raylib)

Minimalni playable prototip koji spaja:
- **isometric diorama / pseudo-3D volumetric** atmosferu,
- **point-and-click kretanje**,
- **dialog + choices + log**,
- **scene transition (fade)**,
- sve u jednoj C++ aplikaciji.

## Artstyle cilj (tehnički naziv)
Traženi mix se može opisati kao:
- **Painterly Isometric Diorama**
- **Stylized Realism + Analog Horror Grading**
- **2.5D Narrative Isometric Adventure**

Praktično: statični ili polustatični painterly background + interaktivni 2D gameplay layer + postprocess (grain, vignette, chromatic aber., LUT) za analog horror osjećaj.

---

## Što je implementirano (NOVI UPDATE - DALJE+++++ )

### Core gameplay
- Dodan **Vertical cutaway level** (side-scroller feeling) s više katova povezanih stepenicama.
- Dodani **missing keys/keycard privileges** (Level 1 i Level 2) i zaključana vrata/cache objekti.
- Dodani **timed choices** i lock/unlock choice logika ovisno o flagovima, redoslijedu odlazaka i keycard levelu.
- Dodani **branching endings** (više završetaka ovisno o odabirima i tajmingu).
- Dodan **WORLD_BIT (0/1)** indikator koji dokazuje binary state model svijeta (paritet važnih stanja/flagova).
- **Glatkije kretanje**: acceleration + damping + bob animacija lika.
- Dodan mali **click cooldown** da input ne spamma i UI interakcija izgleda profesionalnije.
- Dodan **Main Menu** (Start/Quit) za bolji flow kao prava igra.
- Dodan cinematic letterbox i glitch/flicker lighting pulse.
- 2 scene:
  - `control_room`
  - `engine_corridor`
- Klik na podlogu -> lik ide do ciljne točke.
- Kretanje je sada u **isometric world koordinatama** (screen <-> iso konverzija).
- Walkable zona + **blocker kolizije** (lik ne prolazi kroz zidove/velike objekte).
- Spawn i target validacija su popravljeni da lik ne zapne na startu.
- Hotspot interakcije:
  - dijalog hotspotovi,
  - scene exit hotspotovi.

### Vizualni isometric/3D upgrade
- Dodani **actual texture assets** u `assets/textures` (steel/rust/grate/water) i runtime material decals preko scene geometrije.
- **Submarine atmosfera**: industrijske cijevi/strojni blokovi + viewport “water movement” iza stakla.
- Dodan **faux 3D isometric renderer** (projekcija + volumetrijski prism objekti).
- Scene imaju ručno definiranu geometriju (`IsoPrism`) s top/left/right shadingom i punim zidovima/volumenima (jasniji level design).
- Prostorije su složene kao **L-shape** (2 vidljiva zida), a vanjski zidovi se uvijek crtaju.
- Dodan depth layering: objekti se crtaju ispred/iza lika po world dubini (2.5D osjećaj).
- Dodan **camera control**: RMB drag = pan, MMB drag = rotacija scene (limitirano, nikad van zidova).
- Dodan **zoom in/out** na mouse wheel.
- Dodan floor grid za jači isometric čitljiv output.
- Dodani analog-horror overlay efekti: scanlines + pulsirajući crveni cast.

### Narrative sustav
- Data-driven dijalog čvorovi (`DialogueNode`, `Choice`).
- Choice može:
  - otvoriti drugi dijalog node,
  - završiti razgovor,
  - postaviti gameplay flag.
- Dialog history/log (zadnjih više poruka).

### States i tranzicije
- `FreeRoam`, `Dialogue`, `Transition` state machine.
- Fade in/out prijelaz među scenama.

### Build setup
- CMake projekt.
- Primarni target: **raylib** (`find_package(raylib)` ili `FetchContent`).
- Ako raylib nije dostupan: koristi se **lokalni headless compatibility backend** (`src/raylib_compat.cpp`) tako da build i CI mogu proći i bez mreže/dependencyja.

---

## Assets
- `assets/textures/steel_plate.bmp`
- `assets/textures/rust_panel.bmp`
- `assets/textures/grate_floor.bmp`
- `assets/textures/water_view.bmp`
- `assets/materials_manifest.txt`

Ako koristiš Blender: exportaj texture atlase u ovaj folder i zamijeni datoteke istim imenima za instant upgrade rendera.

## Pokretanje

### Standard (raylib)
```bash
cmake -S . -B build
cmake --build build -j
./build/submarine_noir
```

### Windows PowerShell (Ninja / MinGW)
```powershell
cmake -S . -B build
cmake --build build -j
.\build\submarine_noir.exe
```

Napomena: na Windowsu je izlazni fajl `submarine_noir.exe`, ne `./build/submarine_noir`.

### Quick helper skripte
```powershell
# Windows PowerShell (auto-fix stale cache, build, run)
.\scripts\build.ps1

# Ako želiš prisilno clean build
.\scripts\build.ps1 -Clean
```

```bash
# Linux/macOS headless smoke build
./scripts/build.sh
```

### Text-only asset workflow (no binary commit needed)
Ako ti platforma blokira binarne datoteke, **ne commitaj texture** nego ih generiraj lokalno:

```bash
python3 ./scripts/generate_textures.py
```

Windows PowerShell:
```powershell
python .\scripts\generate_textures.py
```

`build.sh` i `build.ps1` već automatski pokreću generator prije builda.

### Binary assets fallback (ZIP bundle)
Ako ti alat ne želi pushati/commitati više `.bmp` fajlova pojedinačno, možeš koristiti ZIP bundle:

```bash
# Spakiraj sve texture u jedan artefakt
./scripts/pack_assets.sh

# Raspakiraj bundle natrag u assets/textures
./scripts/unpack_assets.sh
```

Bundle lokacija: `assets/bundles/textures_bundle.zip`.

### Offline/CI fallback (bez raylib)
Ako okruženje ne može instalirati/povući raylib, projekt se i dalje kompilira uz headless backend.
Isti build koraci vrijede, ali runtime je no-op render (korisno za provjeru da je kod i arhitektura ispravna).

Možeš i ručno forsirati compat mod (čak i kad raylib postoji):
```bash
cmake -S . -B build -DFORCE_RAYLIB_COMPAT=ON
cmake --build build -j
```

---

## Kontrole
- **LMB**: kretanje / interakcija / odabir dialogue choice
- **RMB (drži i pomiči miš)**: pomicanje kamere (pan)
- **MMB / pritisak wheel-a (drži i pomiči miš)**: rotacija scene (limitirana, samo isometric scene)
- **Mouse Wheel**: zoom in/out
- **ESC**: izlaz (u real raylib buildu)

---

## Kako dalje do “Disco-like” kvalitete (besplatno)

1. **Asset pipeline**
   - Blender blockout (isometric kamera)
   - render pass (base, AO, depth)
   - paintover u Krita/GIMP
   - export: background / foreground / mask

2. **2.5D dubina**
   - depth map + parallax layeri
   - occlusion foreground preko lika

3. **Horror postprocess**
   - LUT color grading (hladno plava + topla praktična svjetla)
   - film grain
   - vignette
   - blagi jitter/chromatic aber.

4. **Sustavi za “top game”**
   - save/load
   - inventory + checks
   - quest graph
   - reputation/sanity meters
   - audio zones + dynamic ambience

---

## Plan za sljedeći patch (na tvoj "DALJE")

0. **Audio (sljedeći patch)**
   - Dodati real ambient loopove: submarine machine hum, pressure creaks, distant sonar pings.
   - Zone-based muffling i kratki glitch audio stingeri kod flickera.

1. **Vizualni upgrade (sljedeći korak)**
   - Učitavanje ručno nacrtanih painterly background/foreground layera po sceni (stil reference slike).
   - Soft shadow pass i AO fake overlay za realističniji “diorama” feeling.
   - Uvesti depth mask occlusion da lik prolazi iza objekata.
   - Dodati volumetrijsku maglu i cinematic light shafts.

2. **Pathfinding upgrade**
   - Zamijeniti "direct walk" sa waypoint/A* navigacijom preko walkmesh triangulacije.

3. **Narrative tooling**
   - Prebaciti dijalog iz hardcode C++ u JSON datoteke.
   - Dodati uvjete (`requiresFlag`) i skill check roll.

4. **UX i produkcija**
   - Save/load u lokalni JSON.
   - Scene registry i data folder struktura (`assets/scenes/*`).

5. **Submarine content slice**
   - 5 prostorija: control, crew, medbay, engine, ballast.
   - 1 kratki horror event chain s 3 ishoda.

6. **Packaging/dev ergonomics**
   - CMake Presets za Windows/Linux profile.
   - release artifact + portable content folder layout.

7. **Audio + assets integration**
   - Učitati stvarne teksture/sprite sheetove i ambient audio loopove (machine hum, pressure creaks, sonar).
   - Dodati per-scene audio mix i trigger-based stingers za timed odluke.

---

## Claude AI za razgovor s likovima (preporuka)
Za NPC razgovore nakon ovog prototipa, najpraktičniji izbor je:
- **Claude 3.7 Sonnet**: najbolji balans kvalitete dijaloga, konteksta i stabilnosti tona likova.
- **Claude 3.5 Haiku**: jeftinija/briša opcija za sporedne NPC-e i ambient replike.

Predložena integracija (sljedeći patch):
1. NPC profil + world state + inventory + flags šalješ kao JSON kontekst.
2. Model vraća samo jednu repliku + opcionalno `setFlag` akcije.
3. Dodati safety pravilo: NPC ne otkriva "tajne" bez trust/permission flagova.

Napomena: u ovom patchu nije dodan online API poziv (da build ostane offline-friendly), ali struktura state/flags je pripremljena za to.

## Napomena o AI slikama i 2.5D
Da, moguće je besplatno:
- generiranje slika lokalno (npr. Stable Diffusion toolchain),
- depth map ekstrakcija,
- slojevi + parallax + occlusion u raylibu.

Za stabilnu kvalitetu je i dalje najbolje:
- **Blender blockout + ručni paintover + AI kao pomoćni alat**, ne jedini izvor.

---


## Windows quick fix for errors from your log
Ako vidiš grešku `Parse error ... "<<<<<<<"` u `CMakeLists.txt`, u fajlu su ostali merge conflict markeri.

1) Provjeri marker linije:
```powershell
Select-String -Path .\CMakeLists.txt -Pattern '<<<<<<<|=======|>>>>>>>'
```

2) Ako nešto vrati, otvori `CMakeLists.txt` i obriši conflict blok (`<<<<<<<`, `=======`, `>>>>>>>`) pa ostavi samo jednu finalnu verziju.

3) Očisti build i ponovno konfiguriraj:
```powershell
Remove-Item -Recurse -Force .\build -ErrorAction SilentlyContinue
cmake -S . -B build
cmake --build build -j
.\build\submarine_noir.exe
```

Napomena:
- izvršni fajl je `submarine_noir.exe` (nije `submarine_noir.execmake`).
- Ne upisuj README komentare u isti red s komandama (npr. `./build/submarine_noir# ...`).

Ako je PowerShell policy blokirao skriptu (`running scripts is disabled`), pokreni helper skriptu ovako:
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Clean
```

## Windows build output explained
Ako vidiš ovo u PowerShellu:
- `CMake Deprecation Warning at build/_deps/raylib-src/...` -> to je warning iz **upstream raylib CMake-a**, nije greška u ovom projektu.
- `ninja: no work to do.` -> build je već aktualan, pa Ninja nema što ponovno kompilirati.

Za prisilni rebuild:
```powershell
cmake --build build --clean-first -j
.\build\submarine_noir.exe
```

Ako želiš uvijek svježu konfiguraciju:
```powershell
cmake --fresh -S . -B build
cmake --build build -j
.\build\submarine_noir.exe
```

## Troubleshooting (Windows CMake cache mismatch)
Ako si projekt premjestio iz jednog foldera u drugi (npr. `Downloads` -> `Documents`), stari `build/CMakeCache.txt` će sadržavati krivu putanju i CMake će javiti grešku "source does not match the source used to generate cache".

Rješenja:

```powershell
# Opcija A: obriši stari build folder pa ponovno konfiguriraj
Remove-Item -Recurse -Force .\build
cmake -S . -B build
cmake --build build -j
.\build\submarine_noir.exe
```

```powershell
# Opcija B: koristi helper skriptu (automatski detektira stale cache)
.\scripts\build.ps1
```

```powershell
# Opcija C: CMake fresh configure
cmake --fresh -S . -B build
cmake --build build -j
.\build\submarine_noir.exe
```

