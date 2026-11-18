# TrainBoard — RER Vaud / CFF

Firmware embarqué pour une carte électronique custom (format A4, projet **ETRO 4 — EPSIC Lausanne**) représentant le réseau ferroviaire **RER Vaud (CFF)** sur 4 chaînes de LEDs adressables, avec affichage en temps réel du trafic via l'API publique [transport.opendata.ch](https://transport.opendata.ch).

Le cahier des charges complet (contexte, contraintes matérielles, modes de fonctionnement) est dans [`CLAUDE.md`](CLAUDE.md).

## État du projet

Le hardware est **validé** (pinout figé, voir `include/config.h`), et l'architecture logicielle complète a été posée (voir le plan détaillé conservé hors dépôt). Deux chantiers sont actifs en parallèle :

1. **Outil de calibration** (`env:calib`) — **fonctionnel**, utilisé pour mesurer/tagger la correspondance physique LED ↔ gares/lignes sur le vrai hardware.
2. **Application complète** (`env:esp32-s3-devkitm-1`) — squelette en place (dual-core FreeRTOS, modes, pipeline réseau), **tables de données encore vides** (`src/stations.cpp`) en attendant un export propre de l'outil de calibration.

Les deux environnements compilent (`pio run -e calib` / `pio run -e esp32-s3-devkitm-1`).

## Matériel

| Élément | Détail |
|---|---|
| MCU | ESP32-S3-WROOM-1-N4 (4 Mo flash DIO, 0 Mo PSRAM) |
| Alim | USB-C 5V → AMS1117-3.3 → rail 3.3V |
| USB | Natif ESP32-S3 (IO19/IO20), pas d'UART externe |
| 4 bus LED | WS2812B via level-shifter 74AHCT125PW — GPIO 4, 5, 6, 7 |
| Encodeur rotatif | GPIO 10 (A), 11 (B), 12 (clic) |
| Bouton nav | GPIO 8 |
| Afficheur 4×14 segments | HT16K33 I2C — SDA=14, SCL=13, adresse 0x70 |

Détails complets et contraintes électriques/mémoire dans `CLAUDE.md` §2.

## Arborescence

```
include/            headers (config pinout, modèle de données, drivers, modes...)
src/                implémentation (main_app.cpp = app complète, main_calib.cpp = outil de calib)
CSV/                exports de calibration bruts du hardware réel (un fichier par ligne + Gares.txt)
tools/
  calib_web/        outil web (Web Serial, Chrome/Edge) pour piloter le calib_tool et visualiser/éditer
  validate_calib_csv.js   validateur Node des exports CSV avant collage dans stations.cpp
reference/          sketch de calibration d'origine (avant refactor), conservé pour historique
platformio.ini      deux environnements : esp32-s3-devkitm-1 (app) et calib (outil autonome)
```

## Modèle de données (`include/stations.h`)

- `Gare` : code court (≤4 car., affiché sur le 14 segments), nom API, position LED (`bus`,`index`), flag pivot (interrogée sur l'API), niveau PMR.
- `Troncon` : une LED physique de tronçon, associée à **1 à 4 lignes simultanées** (`TronconLigne[4]`), chacune avec **ses propres gares amont/aval**. Ce découpage par ligne (et non par LED) est nécessaire pour modéliser correctement les embranchements/croisements physiques du réseau (ex. jonction multi-lignes près de Renens, fourche Palézieux → Romont/Payerne), où plusieurs lignes traversent la même LED mais divergent juste après.
- `LigneId` (`include/colors.h`) : 13 lignes du réseau (R1–R13, RE33, S5, S30/S40/S41, S52), chacune avec sa couleur officielle ; S30/S40/S41 alternent entre deux couleurs (période 1.5 s, `LIGNE_ALTERNATION_PERIOD_MS`).

`src/stations.cpp` contient les tables `GARES[]`/`TRONCONS[]`, **actuellement vides** — elles sont générées par la commande `EXPORT CPP` de l'outil de calibration puis collées manuellement ici après vérification.

## Outil de calibration (`env:calib`)

But : mesurer sur le hardware réel la correspondance `(bus, index LED)` ↔ gare/ligne, sans rien inventer dans le code. Pilotable :
- physiquement (encodeur = déplacement, boutons = navigation/aperçu),
- et surtout **par commandes série** (port USB natif, 115200 bauds) : `STATION`, `UNTAG`, `LINE <code> START/STOP/ADD/DEL`, `SETBORDERS <ligne> <amont> <aval>`, `LIST [...]`, `GOTO`, `EXPORT CSV/CPP`, `REVIEW`, `STATE`/`SESSIONS`, `CLEAR ALL CONFIRM`.

`SETBORDERS <ligne> <amont> <aval>` permet de corriger les gares amont/aval d'une ligne donnée sur une LED déjà taguée — l'outil d'édition/correction des petites erreurs de saisie demandé en cours de calibration.

### Outil web (`tools/calib_web/index.html`)

Page HTML/JS autonome (Web Serial API — Chrome/Edge uniquement) qui se connecte au port série du calib_tool, affiche l'état courant, la liste des gares/tronçons taggés, et permet :
- le tagging de gares et de lignes sans taper les commandes à la main,
- l'**édition directe** des gares amont/aval d'une ligne sur une LED (bouton ✎) via `SETBORDERS`,
- l'import/export CSV (réplique les tags sur le hardware depuis un fichier).

### Validateur (`tools/validate_calib_csv.js`)

```
node tools/validate_calib_csv.js [dossier_CSV]   # défaut: CSV/
```

Recoupe les fichiers d'export (`Gares.txt` + un fichier par ligne, ou un export CSV unique) avant de lancer `EXPORT CPP` :
- **erreurs bloquantes** : auto-boucle (amont == aval sur une même ligne), conflit entre sources pour la même (bus, index, ligne), code de gare référencé mais non défini ;
- **avertissements** : bornes non résolues (`?`), topologie suspecte par ligne (gare à plus de 2 voisins, tracé non connexe) ;
- **info** : embranchements détectés (plusieurs lignes sur une même LED avec des bornes différentes) — **normal** sur un croisement physique, pas une erreur.

## Build

```
pio run -e calib                # outil de calibration autonome
pio run -e esp32-s3-devkitm-1   # application complète
pio run -e calib -t upload      # flash l'outil de calibration
```

## Modes de l'application complète

`LIVE` (trafic temps réel + retards), `PAX` (affluence voyageurs), `PMR` (accessibilité gares), `EXPLORE` (navigation ligne/gare), `DEMO` (simulation hors-ligne, utilisable sans Wi-Fi), `CAL` (calibration, accessible au boot en maintenant le bouton nav). Détails fonctionnels complets dans `CLAUDE.md` §3.
