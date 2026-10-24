# Latarnia morska w płatku śniegu: raport sceny

Wygenerowane przez `build_scene.py` (Blender 5.2 LTS). Wszystkie dane pochodzą ze skryptu i z niezależnej kontroli wyeksportowanego `scene.obj`.

## Obiekty

| Obiekt | Materiał | Trójkąty | Zamknięty (manifold) | Uwagi |
|---|---|---:|:---:|---|
| Snowflake | ICE | 1 260 | tak | 2 powłoki: zewnętrzna plus wnęka powietrzna z normalnymi do wnęki; 0 samoprzecięć, 0 zdegenerowanych trójkątów |
| LH_Tower_White | WALL_WHITE | 512 | tak | cokół i 3 białe pasy |
| LH_Tower_Red | WALL_RED | 384 | tak | 3 czerwone pasy |
| LH_Metal | METAL_DARK | 1 520 | tak | galeria, 8 słupków, barierka (torus), szprosy, dach z iglicą, drzwi, okna |
| LH_Glass | GLASS | 192 | tak | rura szklana o grubości 0.007, w środku powietrze; 0 samoprzecięć |
| LH_Lamp | LAMP_LIGHT | 224 | tak | kula r = 0.04 wewnątrz szyby |
| Rock | ROCK | 240 | tak | płaski wierzch pod latarnią |
| Snow | SNOW | 420 | tak | 10 kulek (po 80 tr.) i 5 gwiazdek (po 44 tr.) |
| **Latarnia razem** | | **2 832** | | limit 4 000 |
| **Scena razem** | | **4 752** | | limit 15 000 |

"Zamknięty" oznacza, że nie ma krawędzi brzegowych, każda krawędź ma dokładnie 2 ściany o zgodnym nawinięciu, nie ma podwójnych wierzchołków, a objętość ze znakiem jest dodatnia (normalne na zewnątrz).

## Odstępy (wymagane ≥ 0.02)

| Para | Min. odstęp |
|---|---:|
| latarnia i skała ↔ ściany wnęki w lodzie | 0.030 |
| wnęka ↔ zewnętrzna powierzchnia rdzenia płatka | 0.064 |
| śnieg ↔ lód | 0.250 |
| szkło ↔ lampa / szkło ↔ metal / lód ↔ latarnia | 0 przecięć |

## Układ (Blender, Z w górę)

- Płatek stoi pionowo w płaszczyźnie XZ, przodem do kamery (−Y). Bbox: x ±1.72, y ±0.55, z ±1.99.
- Rdzeń płatka to sześciokątna bryła o grubości 1.1. W środku jest wnęka powietrzna (sześciokątny graniastosłup z daszkiem). Latarnia stoi więc w powietrzu, a załamanie lód/powietrze i szkło/powietrze liczy się poprawnie.
- Skała: z od −0.78 do −0.58. Latarnia: z od −0.58 do 0.62 (wysokość 1.20).
- Po eksporcie (Forward −Z, Up Y): `(x, y, z)_blender → (x, z, −y)_obj`.

## Kamera

| | Blender (Z w górę) | OBJ / silnik (Y w górę) |
|---|---|---|
| pozycja | (0, −8.5, 2.0) | (0, 2.0, 8.5) |
| patrzy na | (0, 0, 0) | (0, 0, 0) |
| wektor „w górę” | (0, 0, 1) | (0, 1, 0) |
| FOV pionowy | 30° | 30° |
| proporcje | 16:9 (1920×1080) | |

## Materiały w MTL

| Nazwa | Rodzaj | Klucze |
|---|---|---|
| ICE | dielectric | `Kd 0.85 0.93 1.0`, `Ni 1.31`, `illum 7` |
| GLASS | dielectric | `Kd 1 1 1`, `Ni 1.5`, `illum 7` |
| LAMP_LIGHT | diffuse_light | `Kd 1.0 0.85 0.5`, `Ke` = kolor × 15, `illum 0` |
| WALL_WHITE | lambertian | `Kd 0.9 0.9 0.88`, `illum 1` |
| WALL_RED | lambertian | `Kd 0.7 0.08 0.06`, `illum 1` |
| METAL_DARK | metal | `Kd 0.2 0.2 0.22`, `Pr 0.2` (= fuzz), `Pm 1`, `illum 3` |
| ROCK | lambertian | `Kd 0.35 0.33 0.3`, `illum 1` |
| SNOW | lambertian | `Kd 0.95 0.95 0.97`, `illum 1` |

Każdy materiał ma też komentarz `# kind=...` z parametrem RTIOW (ior / fuzz / strength).

## Pliki

`scene.obj` + `scene.mtl`, `snowflake.obj`, `lighthouse.obj`, `rock.obj`, `snow.obj` (każdy z własnym `.mtl`), `scene.blend`, `scene_stats.json`, `preview.png` (podgląd z Cycles), `build_scene.py`.

## Co poprawić, jeśli zostanie zapas trójkątów (~10 000 wolnych)

1. Płatek (1 260 z 6 000): dodać gałązki drugiego rzędu na najdłuższych bocznych gałęziach i fazowanie krawędzi ramion. Kryształ zacznie wtedy lepiej łapać refleksy.
2. Fasetowanie płaskich ścian rdzenia (np. lekkie piramidki / „szlif”), co da więcej ciekawych załamań wokół latarni.
3. Skała: 32 segmenty i więcej pierścieni profilu albo kilka mniejszych kamieni u podstawy.
4. Latarnia: druga (niższa) poręcz galerii, konsole pod galerią, gzyms pod dachem, ramy okien.
5. Do 20 drobnych elementów śniegu: zamiast kulek więcej małych płatków (gwiazdek) z gałązkami.
6. Płaszczyzna podłoża (2 trójkąty), jeśli silnik ma rzucać cienie. Obecnie scena jej nie ma.
