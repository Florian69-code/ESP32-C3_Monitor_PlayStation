# PS5 Fan PWM Reader

Lecteur et moniteur de ventilateur PlayStation base sur ESP32-C3, OLED 72x40 et interface Web locale.

Le firmware mesure le signal PWM du ventilateur, calcule le duty-cycle et la frequence, filtre les mesures et affiche l'etat de la console. Un point d'acces Wi-Fi permet de consulter l'historique, regler les profils et suivre les logs sans dependance a Internet.

## Fonctionnalites

- Mesure PWM par interruption sur GPIO 2.
- Filtrage exponentiel du duty-cycle (cycle de vie) et de la frequence.
- Six pages OLED: PWM, etat, historique, details, temperatures et profil.
    - Historique (graphique) de 72 points, affichage OLED et graphique Web.
- Profils PS5 FAT, PS4 PRO et PS3 FAT.
- LED thermique sur GPIO 8.
- Jusqu'a deux sondes DS18B20 sur GPIO 4.
- Detection, nommage et seuils individuels des sondes par profil.
- Configuration Web persistante en NVS.
- Console de logs de `TRACE` a `ERROR`.
- Pages LittleFS avec fallback HTML/CSS compile.

## Materiel

| Fonction | GPIO |
| --- | ---: |
| Signal PWM ventilateur | 2 |
| Sondes DS18B20 OneWire | 4 |
| OLED SDA | 5 |
| OLED SCL | 6 |
| LED thermique | 8 |
| Bouton navigation | 9 |

Le bouton est en `INPUT_PULLUP`: le relier a la masse. Ne jamais appliquer directement un signal 5 V sur un GPIO ESP32-C3.

## Mise en route

1. Installer Visual Studio Code avec les extensions PlatformIO IDE et C/C++.
2. Connecter la carte ESP32-C3 en USB.
3. Ouvrir le dossier du projet.
4. Compiler le firmware:

```bash
platformio run
```

5. Construire et televerser le filesystem Web:

```bash
platformio run -t buildfs
platformio run -t upload
platformio run -t uploadfs
```

Sous Windows, utiliser au besoin:

```powershell
& "C:\Users\User\.platformio\penv\Scripts\platformio.exe" run
```

## Connexion Web

Apres le demarrage, connecter l'ordinateur ou le telephone au reseau:

- SSID: `PlastationFan`
- Mot de passe Wi-Fi: `Pl@ystati0nf@n`
- Adresse: `http://192.168.4.1`
- Mot de passe de la page Web: `S0ny`

Ces informations peuvent être modifié en éditant le fichier /src/config/web_config.h.
D'autres informatons WEB peuvent être modifié sur cette page.

L'interface WEB propose les pages :
- Accueil
- Historique PWM
- Temperature
- Profil
    - Les changements de profil, seuils PWM, sondes, icône, entrainent un redemarrage.
    - Possibilité de remettre les valeurs par défaut.
- Console

## Utilisation de l'OLED

- Appui court: page suivante.
- Appui long, au moins 800 ms: retour a la page PWM.
- `PWM - KO`: aucun echantillon PWM valide depuis 800 ms.
- `WIFI - KO`: le point d'acces n'a pas pu etre cree.
- Page Temperature: valeurs des sondes detectees, ou `N/A`.

Le signal PWM est valide apres au moins 10 periodes. Le duty-cycle est filtre avec 90 % de l'ancienne valeur et 10 % de la nouvelle. Un point d'historique est ajoute toutes les 10 secondes.

## Sondes DS18B20

Les sondes sont recherchees au demarrage sur GPIO 4 et peuvent etre rescanees depuis Profil. Prevoir une resistance de pull-up de 4,7 kOhm sur le bus OneWire.

Depuis la page Profil, il est possible de selectionner l'adresse, le nom, le seuil IDEL et le seuil MAX de chaque sonde. Ces reglages sont geres par profil dans Preferences. La lecture est actualisee toutes les 2 secondes et deux sondes maximum sont gerees.

## Organisation du projet

- `src/app/`: orchestration et etat applicatif;
- `src/display_ui.*`: rendu OLED;
- `src/pwm_sampler.*`: mesure PWM;
- `src/temperature_manager.*`: detection et lecture DS18B20;
- `src/network/`: Wi-Fi, HTTP, templates et LittleFS;
- `src/config/`: broches, timings, profils et parametres;
- `src/utils/`: logger circulaire en RAM;
- `data/`: pages HTML et CSS LittleFS;
- `docs/ARCHITECTURE.md`: description technique detaillee.
- `docs/3d/`: fichier pour impression 3D.

## Depannage rapide

Si les pages Web ne correspondent pas au firmware, reconstruire puis televerser LittleFS avec `uploadfs`. Si le PWM reste absent, verifier le niveau electrique, la masse commune et GPIO 2. Si aucune temperature n'apparait, verifier GPIO 4, la resistance de 4,7 kOhm et l'adresse choisie dans Profil.

La configuration persistante peut etre remise a zero depuis Profil. Cette operation restaure les seuils des trois profils, supprime la configuration des sondes et redemarre la carte.

## Boitier et achats

- Boitier 3D: https://makerworld.com/fr/models/2264801-esp32-c3-0-42-oled-case
    - docs/3d/oled042_stls.zip
    - docs/3d/oled042.3mf
- Support boitier 3D :
    - docs/3d/supportEsp32-c3Oled.{3mf,scad,stl}
- ESP32-C3 OLED: https://amzn.to/3RfY5Lw
- Alternative: https://s.click.aliexpress.com/e/_c4oLGiCb
- Impression 3D: https://jlcpcb.com/fr/?from=CabriDIY_JLCPCB
