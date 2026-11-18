# Cahier des Charges Logiciel — TrainBoard (RER Vaud / CFF)

## 1. Contexte et Objectif du Projet
Développement du firmware embarqué pour une carte électronique custom au format A4 (**TrainBoard**, projet ETRO 4 — EPSIC Lausanne).  
Le système représente le réseau ferroviaire **RER Vaud (CFF)** à l'aide de 4 chaînes de LEDs adressables (SMD pour les tronçons/trains et THT pour les gares), d'un afficheur alphanumérique 4 digits 14 segments et d'une interface de navigation physique (encodeur rotatif + bouton).

L'objectif est d'afficher en temps réel l'état du trafic ferroviaire suisse via l'API publique **Transport Opendata** (`transport.opendata.ch`), tout en offrant plusieurs modes de visualisation (trafic live, affluence voyageurs, accessibilité PMR, consultation par ligne/gare et mode démonstration hors-ligne).

---

## 2. Architecture Matérielle Validée (100 % Fonctionnelle)

### 2.1 Microcontrôleur et Environnement de Build
* **MCU :** `ESP32-S3-WROOM-1-N4` (4 Mo Flash interne en mode **DIO**, **0 Mo de PSRAM**).
* **Alimentation :** Entrée USB-C 5 V + régulateur LDO `AMS1117-3.3` sortant un rail `3.3 V` stable.
* **Port USB :** USB natif de l'ESP32-S3 (`IO19` / `IO20`) sans puce UART externe (`ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`).
* **Plateforme cible :** PlatformIO (VS Code) utilisant le core Arduino-ESP32 v3.x (`pioarduino`) avec `board_build.arduino.memory_type = dio_qspi`.

### 2.2 Table de Brochage (Pinout Définitif)

| Périphérique | Composant / Interface | GPIO ESP32-S3 | Contraintes & Notes Techniques |
| :--- | :--- | :--- | :--- |
| **Bus LED 0** | WS2812B via `74AHCT125PW` (3.3 V → 5 V) | `GPIO 4` | Jusqu'à 250 LEDs par bus (pilote RMT natif) |
| **Bus LED 1** | WS2812B via `74AHCT125PW` | `GPIO 5` | Jusqu'à 250 LEDs par bus |
| **Bus LED 2** | WS2812B via `74AHCT125PW` | `GPIO 6` | Jusqu'à 250 LEDs par bus |
| **Bus LED 3** | WS2812B via `74AHCT125PW` | `GPIO 7` | Jusqu'à 250 LEDs par bus |
| **Encodeur A** | Encodeur rotatif mécanique Bourns | `GPIO 10` | `INPUT_PULLUP` + anti-rebond logiciel (~5 ms) |
| **Encodeur B** | Encodeur rotatif mécanique Bourns | `GPIO 11` | `INPUT_PULLUP` |
| **Clic Encodeur** | Bouton poussoir intégré à l'encodeur | `GPIO 12` | `INPUT_PULLUP` (actif à `LOW`) |
| **Bouton Nav** | Bouton poussoir `BNav2` | `GPIO 8` | `INPUT_PULLUP` (actif à `LOW`) |
| **I2C SDA** | Afficheur 4 digits 14 segments (`HT16K33`) | `GPIO 14` | Bus I2C à `50 kHz` (`Wire.setClock(50000)`) |
| **I2C SCL** | Afficheur 4 digits 14 segments (`HT16K33`) | `GPIO 13` | Adresse I2C `0x70`, alimentation 100 % en **3.3 V** |

### 2.3 Contraintes Électriques et Mémoire
* **Budget courant USB :** Le rail 5 V alimente jusqu'à plusieurs centaines de LEDs WS2812B. La luminosité globale doit être **bridée par logiciel** (ex. plafond à `40/255` par canal RGB) pour éviter toute chute de tension USB.
* **Contrainte RAM (sans PSRAM) :** Les réponses JSON brutes de `transport.opendata.ch` peuvent dépasser 50 Ko. Le parsing doit impérativement utiliser le **filtrage de flux (`DeserializationOption::Filter` d'ArduinoJson)** et des tableaux statiques (`char[]`), sans fragmentation mémoire (`String`).
* **Afficheur 14 segments :** Module monochrome (couleur matérielle fixe), piloté directement par registres I2C (`0x21`, `0xA0`, `0xEA`, `0x81`).

---

## 3. Spécifications Fonctionnelles (Features)

### 3.1 Architecture Logicielle (Dual-Core FreeRTOS)
* **Core 0 (Tâche Réseau & Données) :** Gestion du Wi-Fi, synchronisation horaire NTP (`Europe/Zurich`), requêtes HTTP/HTTPS périodiques vers l'API CFF, parsing JSON et mise à jour d'une structure d'état partagée (protégée par Mutex).
* **Core 1 (Tâche Temps Réel & IHM) :** Lecture de l'encodeur et des boutons sans latence, rafraîchissement des 4 bus LED via RMT et gestion du défilement de texte sur l'afficheur 14 segments.

### 3.2 Charte Graphique des Lignes RER Vaud
Chaque ligne du réseau possède un code couleur officiel dédié (liste et couleurs mises à jour lors de la session de calibration — remplace la charte initiale) :
* **R1** : Orange (`#FF6600`)
* **R2** : Rouge (`#E60000`)
* **R3** : Bleu foncé (`#003399`)
* **R4** : Bleu clair (`#0099FF`)
* **R7** : Jaune (`#FFCC00`)
* **R8** : Vert clair (`#66CC33`)
* **R9** : Vert foncé (`#006633`)
* **R13** : Violet (`#9933CC`)
* **RE33** : Marron (`#8B4513`)
* **S5** : Bordeaux (`#800020`)
* **S30 / S40 / S41** : Alternance Rose (`#FF3399`) / Cyan (`#00CCCC`), période ~1.5 s (cf. `LIGNE_ALTERNATION_PERIOD_MS`)
* **S52** : Blanc (`#FFFFFF`)

### 3.3 Modes de Fonctionnement
1. **Mode `LIVE` (Trafic Temps Réel) :**
   * Positionnement des trains en circulation sur les LEDs correspondantes selon l'heure actuelle et les horaires réels/théoriques.
   * Signalisation visuelle des **retards** (ex. clignotement lent pour `+1` à `+4 min`, rapide ou alternance rouge pour `>= +5 min` ou train supprimé).
   * Afficheur 14 segments : affichage de l'heure courante (`HH:MM`) ou défilement des perturbations majeures.
2. **Mode `PAX` (Fréquentation / Affluence Voyageurs) :**
   * Coloration des gares/tronçons selon les prévisions d'occupation de l'API CFF (`capacity1st` / `capacity2nd`) : Vert (faible), Orange (moyenne), Rouge (forte).
3. **Mode `PMR` (Accessibilité Gares) :**
   * Affichage statique du niveau d'accessibilité des gares THT (plain-pied autonome, assistance requise, non accessible) basé sur la base de données embarquée.
4. **Mode `EXPLORE` (Sélection Ligne ou Gare) :**
   * Navigation via l'encodeur parmi les lignes (`R1`..`RE33`) ou les gares du réseau ; le clic isole l'élément sélectionné sur la carte et affiche le prochain départ + retard sur le 14 segments.
5. **Mode `DEMO` (Simulation Hors-Ligne — Présentation / Examen) :**
   * Mode autonome sans dépendance Wi-Fi faisant circuler des trains virtuels sur toutes les lignes avec simulation de retards, permettant de démontrer 100 % des fonctionnalités devant un jury même sans accès Internet.
6. **Mode `CAL` (Calibration & Diagnostic) :**
   * Accessible au démarrage (maintien de `BNav2` au boot) : pilotage manuel `(bus, index_led)` affiché sous forme `BXXX` sur l'écran pour relever l'index physique de chaque LED sur le PCB, et affichage de l'adresse IP.

### 3.4 Connectivité & Robustesse
* **Configuration Wi-Fi :** Portail captif (`TrainBoard-Setup`) au démarrage si aucun réseau connu n'est disponible dans les 15 secondes, avec sauvegarde des identifiants en mémoire non-volatile (`Preferences` / NVS).
* **Tolérance aux pannes :** En cas de coupure Wi-Fi ou d'indisponibilité de l'API, maintien de l'affichage sur les dernières données en cache (ou bascule automatique en mode `DEMO` horaire) avec indication discrète sur l'afficheur.

---

## 4. Points Ouverts & Questions à Trancher avant Implémentation

Ces points volontairement ouverts doivent être précisés pour figer le comportement exact du firmware :

1. **Topologie exacte des 4 bus LED (`IO4` à `IO7`) :**
   * Combien de LEDs réelles sont soudées sur chacun des 4 bus ?
   * Les LEDs de gares (THT) sont-elles intercalées en série au milieu des LEDs de voie (SMD) sur un même bus, ou regroupées sur un bus dédié ?
   * *Impact :* Définit la structure du fichier `stations.h` (mapping `Gare -> (Bus, Index)` et `Tronçon -> liste d'index`).

2. **Représentation d'un train en mouvement vs. en gare :**
   * Lorsqu'un train circule entre deux gares (ex. Morges → Renens), allume-t-on une seule LED qui avance par interpolation temporelle, ou l'ensemble du tronçon entre les deux gares ?
   * Comment gérer la superposition si deux lignes différentes (ex. `R1` et `RE33`) partagent le même tronçon physique de LEDs sur le PCB ? (Priorité à une ligne, alternance temporelle toutes les 1.5 s, ou mélange de couleurs ?)

3. **Périmètre des gares interrogées sur l'API CFF :**
   * L'API `transport.opendata.ch/v1/stationboard` fonctionne gare par gare (limite de requêtes par minute / temps de réponse ~400 ms par requête).
   * Quelles sont les **gares pivots** exactes à interroger (ex. Lausanne, Renens, Morges, Yverdon, Palézieux, Vevey, Nyon) pour couvrir tout le réseau représenté sur le PCB sans saturer le quota API ?

4. **Ergonomie précise des boutons (`IO12` et `IO8`) :**
   * Quel comportement exact attribuer à l'appui court vs. appui long sur le **clic encodeur (`IO12`)** et sur **`BNav2` (`IO8`)** ? (Ex. : `IO12` = valider/entrer, `IO8` court = changer de mode, `IO8` long = réglage luminosité ou bascule mode `DEMO`).

5. **Interface Web de configuration (Optionnel) :**
   * Souhaite-t-on héberger une petite page web locale sur l'ESP32 (accessible via son IP) pour modifier la luminosité, forcer un mode, ou éditer la table de correspondance des LEDs sans reflasher le code ?

6. **Contraintes spécifiques de l'évaluation ETRO 4 :**
   * Y a-t-il des livrables ou exigences imposées par le professeur (ex. diagramme d'états / UML obligatoire, logs série formatés pour le débogage, consommation électrique maximale mesurée, ou gestion d'un événement spécifique) ?