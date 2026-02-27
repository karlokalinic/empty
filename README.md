# Submarine Noir Prototype (C++ + raylib)

Minimalni playable prototip koji spaja:
- **isometric diorama / painterly horror** atmosferu,
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

## Što je implementirano

### Core gameplay
- 2 scene:
  - `control_room`
  - `engine_corridor`
- Klik na podlogu -> lik ide do ciljne točke.
- Walkable zona definirana poligonom.
- Hotspot interakcije:
  - dijalog hotspotovi,
  - scene exit hotspotovi.

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

### Offline/CI fallback (bez raylib)
Ako okruženje ne može instalirati/povući raylib, projekt se i dalje kompilira uz headless backend.
Isti build koraci vrijede, ali runtime je no-op render (korisno za provjeru da je kod i arhitektura ispravna).

---

## Kontrole
- **LMB**: kretanje / interakcija / odabir dialogue choice
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

1. **Vizualni upgrade**
   - Uvesti textured background/foreground layere po sceni.
   - Dodati shader stack: grain + vignette + blagi CRT/noise pass.

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

---

## Napomena o AI slikama i 2.5D
Da, moguće je besplatno:
- generiranje slika lokalno (npr. Stable Diffusion toolchain),
- depth map ekstrakcija,
- slojevi + parallax + occlusion u raylibu.

Za stabilnu kvalitetu je i dalje najbolje:
- **Blender blockout + ručni paintover + AI kao pomoćni alat**, ne jedini izvor.
