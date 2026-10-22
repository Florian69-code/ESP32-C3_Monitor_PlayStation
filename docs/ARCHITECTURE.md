# Architecture reelle du projet

Ce document decrit l'implementation actuellement presente dans le depot.

## Objectif

Le firmware s'execute sur un ESP32-C3 equipe d'un OLED SSD1306 72x40. Il mesure le signal PWM du ventilateur d'une console PlayStation, filtre le duty-cycle (cycle de service) et la frequence, affiche l'etat localement et expose une interface Web dans un point d'acces Wi-Fi.

Deux sondes DS18B20 peuvent etre ajoutees sur un bus OneWire. Leurs temperatures, noms, adresses et seuils sont accessibles depuis l'interface Web ainsi que depuis l' écran OLED avec moins d'informations.
Les profils console disponibles sont:

- PS5 FAT, profil par defaut;
- PS4 PRO;
- PS3 FAT.
- PS5 SLIM, PS4 FAT, PS3 SLIM (valeurs de depart copiees des modeles proches, a calibrer).

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
    session_stats.{h,cpp}
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

`src/temperature_manager.*` utilise OneWire et DallasTemperature.

- Detection au demarrage et sur demande (`scanSensors()`): maximum de deux adresses, resolution 12 bits. Un scan manuel lit aussi une fois les temperatures (lecture bloquante d'environ 750 ms) pour les afficher a cote des adresses.
- Affectation: chaque emplacement (sonde 1 / sonde 2) utilise l'adresse configuree; si aucun emplacement n'est configure, l'ordre de decouverte du bus est utilise. `detected` est vrai si la sonde repond.
- Lecture non bloquante dans `update()`: requete de conversion toutes les 2 secondes, lecture des valeurs environ 800 ms plus tard. Valeur `N/A` si indisponible.
- Les connexions et deconnexions sont journalisees uniquement lors d'un changement d'etat.
- `isValidAddress()` valide une adresse de 16 caracteres hexadecimaux.

La page Profil permet de selectionner une adresse, un nom et des seuils IDLE/MAX pour chaque sonde; ces reglages sont geres par profil dans Preferences.

### Reseau et interface Web

`src/network/wifi_manager.*` configure uniquement un point d'acces (`WIFI_AP`) a l'adresse `192.168.4.1`; il ne gere pas de connexion a un reseau externe.

`web_ui.*` declare les routes, verifie l'authentification et applique les formulaires. `web_pages.*` charge les templates et injecte l'etat. `web_storage.*` monte LittleFS et sert les fichiers statiques; en cas d'indisponibilite, les pages HTML et CSS de secours sont generees en C++.

Points d'attention sur les templates (`data/pages`):

- le remplacement de jetons `{{...}}` est global: un meme jeton ne doit pas servir a deux valeurs differentes (`{{ACTIVE_PROFILE}}` est reserve a la navigation, le nom du profil de l'accueil utilise `{{PROFILE_NAME}}`);
- les scripts inline partagent la portee globale: un `const` declare deux fois provoque une erreur de syntaxe qui bloque le second script; les scripts generes en C++ sont encapsules dans une fonction anonyme;
- l'accueil affiche l'etat des deux sondes via `{{SENSORS_STATUS}}`; la page Profil affiche le graphique des temperatures (`/api/temperature/status`);
- le fallback C++ de la page Profil n'inclut pas le graphique des temperatures.

Validation des formulaires Profil: adresse de sonde vide ou de 16 caracteres hexadecimaux, nom de 16 caracteres maximum limite a lettres, chiffres, espace, `-` et `_`, adresses differentes pour les deux sondes, seuils IDLE <= MAX. L'etat runtime n'est modifie qu'apres validation complete. Le mot de passe et les cookies ne sont pas ecrits dans les logs.

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
7. Gestion de la conversion des temperatures (requete toutes les 2 secondes, lecture apres 800 ms).
8. Gestion de la LED thermique et rendu OLED.

## Interface Web

### Parametres par defaut

- SSID AP: `PlastationPS5`;
- mot de passe AP et mot de passe Web: definis dans `src/config/web_config.h`;
- URL: `http://192.168.4.1`;
- port HTTP: `80`;
- cookie: `ps5-fan_auth=authorized`.

### Routes

| Methode | Route | Role |
| --- | --- | --- |
| GET | `/` | Login si non authentifie, accueil sinon |
| GET/POST | `/login` | Formulaire et verification du mot de passe |
| GET | `/history` | Graphiques (historique PWM et sondes) |
| GET | `/live` | Page Live: tendance PWM/sondes (5 a 60 min), statistiques et resume de session de jeu |
| GET | `/api/live?range=300\|900\|1800\|3600` | Tendance (1 point / 5 s), min/max/moyenne et sessions de jeu en JSON |
| GET | `/api/history` | PWM courant, maximum, frequence, historique et etat des sondes configurees (nom, detection, temperature) en JSON |
| GET | `/api/status` | Etat live en JSON: uptime, profil, PWM, frequence, sondes et alertes (surchauffe, ventilateur au maximum) |
| GET | `/api/temperature/scan` | Nouvelle detection; JSON des sondes du bus (adresse, temperature) |
| GET | `/api/temperature/status` | Etat des deux sondes configurees en JSON (utilise par le graphique) |
| GET | `/profile` | Profils, seuils et sondes |
| POST | `/profile/apply` | Sauvegarde puis redemarrage |
| GET | `/profile/apply` | Redirection de compatibilite vers `/profile` |
| POST | `/profile/reset` | Remise a zero puis redemarrage |
| GET | `/profile/reset` | Redirection de compatibilite vers `/profile` |
| GET | `/console` | Logs et parametres |
| POST | `/console/settings` | Sauvegarde des logs (refresh 1, 2, 5 ou 10 s) puis redemarrage |
| GET | `/assets/style.css` | CSS LittleFS ou fallback compile |
| GET | `/favicon.ico` | Reponse vide `204` |

Toutes les routes fonctionnelles sauf la connexion exigent le cookie d'authentification. Les API renvoient `401`; les pages redirigent vers `/login`.

## Persistance NVS

Namespace Preferences: `ps5fan`.

Les cles principales sont `page`, `console_profile`, `icon_mode`, `thr_offset`, `log_level`, `log_refresh`, les seuils PWM `p5_*`, `p4_*`, `p3_*` (`_i_cool`, `_i_hot`, `_g_cool`), les seuils temperature du profil `<profil>_t_idle` et `<profil>_t_max`, ainsi que les reglages de sondes `<profil>_s1_*` et `<profil>_s2_*` (`addr`, `name`, `t_idle`, `t_max`).

Les cles ESP32 sont limitees a 15 caracteres: l'offset utilise `thr_offset`, et non l'ancien nom `threshold_offset`; les anciennes cles `temperature_idle` (16 caracteres) et `temperature_max` ne sont plus utilisees. Une chaine vide ecrite avec `putString` retourne 0: ce n'est pas une erreur.

La remise a zero reecrit les seuils PWM par defaut des trois profils, supprime les reglages de sondes et de seuils temperature, remet l'etat runtime a zero et redemarre l'ESP32-C3.

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
- Les sondes sont detectees au demarrage ou a la demande et lues periodiquement; un scan manuel bloque la boucle environ 750 ms.