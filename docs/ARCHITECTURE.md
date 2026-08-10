# Architecture actuelle du projet

Ce document decrit l'implementation reelle du firmware a date, pour eviter les confusions entre cible de refacto et code effectivement deployable.

## Objectif

L'ESP32-C3 lit un signal PWM de ventilateur console sur GPIO 2, calcule duty/frequence, affiche l'etat sur OLED 72x40, et expose une interface Web locale en mode point d'acces WiFi.

Consoles supportees par profil:

- PS5 FAT
- PS4 PRO
- PS3 FAT

## Materiel

- ESP32-C3
- Ecran OLED I2C 72x40 (SSD1306)
- 1 bouton de navigation
- 1 LED de feedback thermique
- 0, 1 ou 2 sondes DS18B20 (TO-92) sur GPIO 4 avec pull-up 4.7k si présentes

Broches par defaut:

- GPIO 2: PWM input
- GPIO 5: OLED SDA
- GPIO 6: OLED SCL
- GPIO 9: bouton (`INPUT_PULLUP`, actif a l'etat bas)
- GPIO 8: LED
- GPIO 4: bus OneWire pour DS18B20

## Structure effective du depot

```text
.
  platformio.ini
  ps5_fan_pwm_reader.ino
  README.md
  data/
    assets/style.css
    pages/login.html
    pages/home.html
    pages/history.html
    pages/profile.html
    pages/console.html
    pages/redirect.html
  docs/ARCHITECTURE.md
  src/
    main.cpp
    config.h
    display_ui.h
    display_ui.cpp
    pwm_sampler.h
    pwm_sampler.cpp
    app/
      app_state.h
      app_state.cpp
      app_logic.h
      app_logic.cpp
    config/
      pins.h
      profiles.h
      defaults.h
      log_config.h
      web_config.h
    network/
      wifi_manager.h
      wifi_manager.cpp
      web_storage.h
      web_storage.cpp
      web_pages.h
      web_pages.cpp
      web_ui.h
      web_ui.cpp
    utils/
      logger.h
      logger.cpp
```

## Modules et responsabilites

### 1) Module app

Fichiers:

- `src/app/app_state.h`
- `src/app/app_state.cpp`
- `src/app/app_logic.h`
- `src/app/app_logic.cpp`

Responsabilites:

- Etat central (`AppState`): page courante, profil actif, valeurs PWM filtrees, historique, statuts WiFi/PWM, mode icone, niveaux logs Web.
- Initialisation globale (`initializeApp`): debug serie, logger, bouton/LED, chargement Preferences, init OLED, init WiFi/AP, init WebServer, demarrage sampler PWM.
- Boucle principale (`updateApp`): bouton, WiFi, HTTP, acquisition PWM, filtrage, historisation, rendu OLED.

### 2) Module display

Fichiers:

- `src/display_ui.h`
- `src/display_ui.cpp`

Responsabilites:

- Rendu OLED uniquement.
- Pages affichees: `SimplePwm`, `StatusFace`, `Graph`, `Details`, `Profile`.
- Ecrans d'erreur: WiFi KO, PWM absent.
- Lecture des seuils actifs via getters (`getActiveIdleCoolMax`, etc.).

### 3) Module sensors

Fichiers:

- `src/pwm_sampler.h`
- `src/pwm_sampler.cpp`

Responsabilites:

- Mesure PWM via interruption `CHANGE`.
- Accumulation durees high/low, nombre de periodes.
- Production echantillon `PwmSample { valid, dutyPercent, frequencyHz }`.

### 4) Module network

Fichiers:

- `src/network/wifi_manager.h`
- `src/network/wifi_manager.cpp`
- `src/network/web_ui.h`
- `src/network/web_ui.cpp`
- `src/network/web_pages.h`
- `src/network/web_pages.cpp`
- `src/network/web_storage.h`
- `src/network/web_storage.cpp`

Responsabilites:

- Creation AP WiFi local (`WIFI_AP`, IP 192.168.4.1).
- Declaration des routes HTTP.
- Authentification login par mot de passe + cookie de session.
- Rendu des pages via templates LittleFS, avec fallback HTML/CSS compile.
- Exposition API JSON de l'historique PWM (`/api/history`).

### 5) Module config

Fichiers:

- `src/config/pins.h`
- `src/config/profiles.h`
- `src/config/defaults.h`
- `src/config/log_config.h`
- `src/config/web_config.h`
- `src/config.h` (agrégateur)

Responsabilites:

- Centraliser broches, timings, filtres, seuils profils, parametres WiFi/Web, et niveaux logs.

### 6) Module temperature

Fichiers:

- `src/temperature_manager.h`
- `src/temperature_manager.cpp`

Responsabilites:

- Detection automatique des sondes DS18B20 sur le bus OneWire GPIO 4.
- Lecture periodique de la temperature avec fallback `N/A` si aucune valeur n'est disponible.
- Persistance des adresses, noms et seuils IDEL/MAX par profil et par sonde.
- Exposition JSON d'etat via `/api/temperature/scan` et `/api/history`.

### 7) Module utils

Fichiers:

- `src/utils/logger.h`
- `src/utils/logger.cpp`
- `src/temperature_manager.cpp`
- `src/temperature_manager.h`

Responsabilites:

- Logger central avec buffer circulaire en memoire.
- Niveaux `TRACE` a `ERROR`.
- Exposition HTML des logs pour la page `/console`.

## Flux d'execution

### Setup

1. Init serie (si active), init logger.
2. Init bouton + LED.
3. Ouverture `Preferences` namespace `ps5fan`.
4. Chargement des reglages persistants (page, profil, seuils, icones, logs).
5. Init OLED + ecran de boot (5s max, interrompable au bouton).
6. Init WiFi AP.
7. Init routes Web + `WebServer.begin()`.
8. Init capteur PWM.

### Loop

1. Gestion bouton (appui court/long).
2. Maintenance WiFi + `webServer.handleClient()`.
3. Cadencement affichage (`DISPLAY_REFRESH_MS = 100`).
4. Lecture et filtrage PWM.
5. Detection perte signal (`SIGNAL_LOST_MS = 800`).
6. Ajout historique (`GRAPH_UPDATE_MS = 10000`).
7. Gestion clignotement LED selon seuils.
8. Lecture/refresh des sondes DS18B20 et mise a jour de l'etat runtime.
9. Rendu page OLED courante.

## Affichage OLED

Pages applicatives:

1. `SimplePwm`
2. `StatusFace`
3. `Graph`
4. `Details`
5. `Temperature`
6. `Profile`

Regles:

- Appui court: page suivante.
- Appui long (>= 800ms): retour `SimplePwm`.
- Si WiFi non initialisé: ecran `WIFI - KO`.
- Si signal PWM absent: ecran `PWM - KO`.
- La page `Temperature` affiche les valeurs des sondes detectees sur tout l'ecran, avec `N/A` si aucune valeur n'est disponible.

## Web: routes et auth

### Parametres reseau

- SSID: `PlastationFan`
- Password AP: `Pl@ystati0nf@n`
- URL: `http://192.168.4.1`
- Port: `80`
- Password login: `S0ny`
- Cookie session: `ps-fan_auth=authorized`

### Routes

- `GET /`
- `GET /login`
- `POST /login`
- `GET /history`
- `GET /api/history`
- `GET /profile`
- `POST /profile/apply`
- `POST /profile/reset`
- `GET /api/temperature/scan`
- `GET /console`
- `POST /console/settings`
- `GET /assets/style.css`
- `GET /favicon.ico`

Toutes les pages sauf login exigent auth cookie (sinon redirection HTTP 303 vers `/login`).

## Persistance (NVS / Preferences)

Namespace: `ps5fan`.

Cles utilisees:

- `page`
- `console_profile`
- `threshold_offset`
- `icon_mode`
- `log_level`
- `log_refresh`
- `p5_i_cool`, `p5_i_hot`, `p5_g_cool`
- `p4_i_cool`, `p4_i_hot`, `p4_g_cool`
- `p3_i_cool`, `p3_i_hot`, `p3_g_cool`

## LittleFS et templates

Fichiers servis:

- `/assets/style.css`
- `/pages/login.html`
- `/pages/home.html`
- `/pages/history.html`
- `/pages/profile.html`
- `/pages/console.html`
- `/pages/redirect.html`

Si LittleFS ne monte pas, le firmware bascule sur des pages fallback generees en C++ (`web_pages.cpp`).

## Ecart entre cible historique et implementation actuelle

Points importants:

- Le projet est deja modulaire (app/network/config/utils), mais certains dossiers "cible" n'existent pas encore (`display/`, `sensors/`, `storage/` en sous-repertoires dedies).
- Le module de persistance n'est pas isole dans un composant `storage`; `Preferences` est utilise directement dans `app_logic.cpp` et `web_ui.cpp`.
- L'interface Web complete est deja presente (login, profil, historique, console), donc ce n'est plus une etape future.

## Build et verification

Commandes recommandees:

```bash
platformio run
platformio run -t buildfs
platformio run -t upload
platformio run -t uploadfs
```

Verification minimale apres changement de `src/` ou `data/`:

1. Build firmware.
2. Build filesystem LittleFS.
3. Upload firmware + filesystem.
4. Test manuel login, menu, historique, profil, console.

## Conclusion

L'architecture actuelle est exploitable, modulaire et cohérente avec l'objectif du projet. Le prochain gain de maintenabilite viendra surtout de l'isolation de la persistance dans un module dedie, puis du raffinement des conventions de separation display/sensor/storage.