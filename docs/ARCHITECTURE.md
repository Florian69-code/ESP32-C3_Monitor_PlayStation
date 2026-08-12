# Architecture reelle du projet

Ce document decrit l'implementation actuellement presente dans le depot.

## Objectif

Le firmware s'execute sur un ESP32-C3 equipe d'un OLED SSD1306 72x40. Il mesure le signal PWM du ventilateur d'une console PlayStation, filtre le duty-cycle (cycle de service) et la frequence, affiche l'etat localement et expose une interface Web dans un point d'acces Wi-Fi.

Deux sondes DS18B20 peuvent etre ajoutees sur un bus OneWire. Leurs temperatures, noms, adresses et seuils sont accessibles depuis l'interface Web ainsi que depuis l' écran OLED avec moins d'informations.
Les profils console disponibles sont:

- PS5 FAT, profil par defaut;
- PS4 PRO;
- PS3 FAT.

## Materiel et broches

| Fonction | Broche | Implementation |
| --- | ---: | --- |
| Entree PWM ventilateur | GPIO 2 | Interruption sur chaque changement de niveau |
| Bus OneWire DS18B20 | GPIO 4 | `INPUT_PULLUP`, jusqu'a 2 sondes |
| OLED SDA | GPIO 5 | Bus I2C |
| OLED SCL | GPIO 6 | Bus I2C |
| LED thermique | GPIO 8 | Clignotement selon le seuil PWM chaud |
| Bouton navigation | GPIO 9 | `INPUT_PULLUP`, actif a l'etat bas |

Le signal applique a GPIO 2 doit respecter les niveaux electriques de l'ESP32-C3. Un signal 5 V ne doit pas etre branche directement sur une entree GPIO.

## Arborescence compilee

```text
.
  platformio.ini
  LICENSE.txt
  ps5_fan_pwm_reader.ino
  README.md
  data/
    assets/style.css
    pages/{console,history,home,login,profile,redirect}.html
  docs/
    ARCHITECTURE.md
    3d/
        oled042_stls.zip
        oled042.3mf
        supportEsp32-c3Oled.{3mf,scad,stl}
  src/
    main.cpp
    config.h
    display_ui.{h,cpp}
    pwm_sampler.{h,cpp}
    temperature_manager.{h,cpp}
    app/{app_state,app_logic}.{h,cpp}
    config/{pins,profiles,defaults,log_config,web_config}.h
    network/{wifi_manager,web_ui,web_pages,web_storage}.{h,cpp}
    utils/logger.{h,cpp}
```

`src/main.cpp` appelle `initializeApp()` une fois puis `updateApp()` a chaque iteration. Le fichier `.ino` est conserve comme entree Arduino historique; l'entree PlatformIO utilisee est `src/main.cpp`.

## Modules et responsabilites

### Application et etat

`src/app/app_state.*` contient l'etat partage par la logique, l'OLED et le Web: page courante, profil, mesures PWM filtrees, maximum, historique, statuts Wi-Fi/PWM, seuils des trois profils, configuration des deux sondes et parametres du logger Web.

`src/app/app_logic.*` orchestre le cycle de vie, le bouton, la persistance Preferences, l'acquisition PWM, le rafraichissement temperature, la LED et le serveur Web.

### Ecran OLED

`src/display_ui.*` ne contient que le rendu OLED et les ecrans d'erreur. Les pages `DisplayPage` sont:

1. `SimplePwm`: duty-cycle en pourcentage;
2. `StatusFace`: etat sous forme de texte ou d'icone;
3. `Graph`: historique PWM et maximum;
4. `Details`: PWM, frequence, seuils et Wi-Fi;
5. `Temperature`: valeurs des sondes, ou `N/A`;
6. `Profile`: profil console actif.

Un appui court passe a la page suivante. Un appui long d'au moins 800 ms revient a `SimplePwm`. L'ecran de demarrage dure au maximum 5 secondes et peut etre interrompu.

### Acquisition PWM

`src/pwm_sampler.*` mesure les durees hautes et basses dans une interruption `CHANGE`. `readAndReset()` capture les compteurs de facon atomique et produit `PwmSample`:

- au moins 10 periodes sont necessaires;
- le temps total doit etre valide et superieur ou egal a 1000 microsecondes;
- `dutyPercent = high / (high + low) * 100`;
- `frequencyHz = periods / (totalUs / 1 000 000)`.

Les mesures valides sont filtrees avec 90 % de l'ancienne valeur et 10 % de la nouvelle. Apres 800 ms sans echantillon valide, le signal est perdu et les valeurs courantes sont remises a zero.

### Temperature

`src/temperature_manager.*` utilise OneWire et DallasTemperature: detection au demarrage et sur demande, maximum de deux sondes, resolution 12 bits, lecture toutes les 2 secondes et valeur `N/A` si indisponible. La page Profil permet de selectionner une adresse, un nom et des seuils IDEL/MAX pour chaque sonde; ces reglages sont geres par profil dans Preferences.

### Reseau et interface Web

`src/network/wifi_manager.*` configure uniquement un point d'acces (`WIFI_AP`) a l'adresse `192.168.4.1`; il ne gere pas de connexion a un reseau externe.

`web_ui.*` declare les routes, verifie l'authentification et applique les formulaires. `web_pages.*` charge les templates et injecte l'etat. `web_storage.*` monte LittleFS et sert les fichiers statiques; en cas d'indisponibilite, les pages HTML et CSS de secours sont generees en C++.

`src/utils/logger.*` conserve un buffer circulaire en RAM avec les niveaux `TRACE`, `DEBUG`, `INFO`, `WARN` et `ERROR`. Les logs sont rendus dans `/console`.

## Cycle d'execution

### Initialisation

1. Initialisation serie et logger.
2. Configuration du bouton et de la LED.
3. Ouverture de Preferences dans le namespace `ps5fan`.
4. Chargement de la page, du profil, des seuils, des sondes et des parametres de logs.
5. Initialisation OLED et ecran de demarrage.
6. Creation du point d'acces Wi-Fi.
7. Montage LittleFS, declaration des routes et demarrage HTTP.
8. Demarrage du sampler PWM et du gestionnaire DS18B20.

### Boucle principale

1. Lecture du bouton et gestion des appuis court/long.
2. Maintenance Wi-Fi et traitement HTTP.
3. Tick applicatif toutes les 100 ms.
4. Lecture, validation et filtrage du PWM.
5. Detection de perte du signal apres 800 ms.
6. Ajout d'un point dans l'historique toutes les 10 secondes si le signal est valide.
7. Mise a jour des temperatures toutes les 2 secondes.
8. Gestion de la LED thermique et rendu OLED.

## Interface Web

### Parametres par defaut

- SSID AP: `PlastationFan`;
- mot de passe AP: `Pl@ystati0nf@n`;
- URL: `http://192.168.4.1`;
- port HTTP: `80`;
- mot de passe Web: `S0ny`;
- cookie: `ps-fan_auth=authorized`.

### Routes

| Methode | Route | Role |
| --- | --- | --- |
| GET | `/` | Login si non authentifie, accueil sinon |
| GET/POST | `/login` | Formulaire et verification du mot de passe |
| GET | `/history` | Historique PWM |
| GET | `/api/history` | PWM courant, maximum, frequence, historique et temperatures en JSON |
| GET | `/api/temperature/scan` | Detection et JSON des sondes DS18B20 |
| GET | `/profile` | Profils, seuils et sondes |
| POST | `/profile/apply` | Sauvegarde puis redemarrage |
| GET | `/profile/apply` | Redirection de compatibilite vers `/profile` |
| POST | `/profile/reset` | Remise a zero puis redemarrage |
| GET | `/profile/reset` | Redirection de compatibilite vers `/profile` |
| GET | `/console` | Logs et parametres |
| POST | `/console/settings` | Sauvegarde des logs puis redemarrage |
| GET | `/assets/style.css` | CSS LittleFS ou fallback compile |
| GET | `/favicon.ico` | Reponse vide `204` |

Toutes les routes fonctionnelles sauf la connexion exigent le cookie d'authentification. Les API renvoient `401`; les pages redirigent vers `/login`.

## Persistance NVS

Namespace Preferences: `ps5fan`.

Les cles principales sont `page`, `console_profile`, `icon_mode`, `thr_offset`, `log_level`, `log_refresh`, les seuils PWM `p5_*`, `p4_*`, `p3_*`, les seuils temperature `*_t_idle` et `*_t_max`, ainsi que les adresses, noms et seuils des sondes (`*_s1_*` et `*_s2_*`).

Les cles ESP32 sont limitees a 15 caracteres: l'offset utilise `thr_offset`, et non l'ancien nom `threshold_offset`.

La remise a zero reecrit les seuils par defaut des trois profils, supprime les reglages de sondes, remet l'etat runtime a zero et redemarre l'ESP32-C3.

## Configuration et build

Les broches sont dans `src/config/pins.h`, les timings et ratios dans `src/config/defaults.h`, les seuils dans `src/config/profiles.h` et les parametres Web dans `src/config/web_config.h`.

```bash
platformio run
platformio run -t buildfs
platformio run -t upload
platformio run -t uploadfs
```

Sous Windows, si PlatformIO n'est pas dans le PATH:

```powershell
& "C:\Users\User\.platformio\penv\Scripts\platformio.exe" run
& "C:\Users\User\.platformio\penv\Scripts\platformio.exe" run -t buildfs
```

Apres une modification de `data/`, reconstruire et televerser LittleFS en plus du firmware.

## Limites connues

- Un seul environnement PlatformIO est defini et aucune suite de tests native automatisee n'est fournie.
- L'authentification repose sur un mot de passe compile et un cookie simple: elle est adaptee a un reseau local isole, pas a Internet.
- Les logs sont conserves en RAM et sont perdus au redemarrage.
- Les sondes sont detectees au demarrage ou a la demande et lues periodiquement.