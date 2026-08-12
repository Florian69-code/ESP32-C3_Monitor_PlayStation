// ====================================================
// SUPPORT ESP32-C3 SUR PIEDS + DEUX TROUS DE CROCHETS
// ====================================================

// --- PARAMÈTRES DU BOÎTIER ---
boitier_largeur = 32; // Largeur du boîtier (mm)
boitier_epaisseur = 16; // Épaisseur du boîtier (mm)

// --- PARAMÈTRES DU PASSAGE USB-C ---
largeur_prise_usb = 18; // Largeur d'ouverture USB
hauteur_passage_usb = 30;// Espace sous le boîtier

// --- PARAMÈTRES DU SUPPORT ET PIEDS ---
marge = 1.0; // Jeu d'insertion
epaisseur_paroi = 4.0; // Épaisseur des parois
marge_socle = 12; // Débord des pieds pour la stabilité

// --- PARAMÈTRES DES CROCHETS ---
section_crochet = 5; // Section (5x5mm)
branche_courte = 15; // Longueur insérée
branche_longue = 28; // Longueur d'accroche
jeu = 0.3; // Tolérance d'assemblage

$fn = 40;

// ====================================================
// ASSEMBLAGE
// ====================================================

// 1. Support vertical
support_esp32_simplifie();

// 2. Les 2 crochets à côté
translate([40, 0, 0]) equerre_simple();
translate([40, 25, 0]) equerre_simple();


// ====================================================
// MODULES DE CONCEPTION
// ====================================================

module support_esp32_simplifie() {
    l_int = boitier_largeur + marge;
    e_int = boitier_epaisseur + marge;

    l_ext = l_int + 2 * epaisseur_paroi;
    e_ext = e_int + 2 * epaisseur_paroi;
    h_totale = hauteur_passage_usb + 30;

    difference() {
        union() {
            // Corps du support vertical
            translate([-l_ext/2, -e_ext/2, 0])
                cube([l_ext, e_ext, h_totale]);

            // 4 Pieds simples intégrés aux 4 coins inférieurs
            translate([-(l_ext + marge_socle)/2, -(e_ext + marge_socle)/2, 0])
                cube([l_ext + marge_socle, e_ext + marge_socle, 4]);
        }

        // --- 1. LOGEMENT INTERNE ESP32 ---
        translate([-l_int/2, -e_int/2, hauteur_passage_usb])
            cube([l_int, e_int + 0.1, 30 + 0.1]);

        // --- 2. DÉCOUPE ÉCRAN FACADE AVANT ---
        translate([-(l_int - 4)/2, -e_ext/2 - 0.1, hauteur_passage_usb + 5])
            cube([l_int - 4, epaisseur_paroi + 0.2, 25 + 0.1]);

        // --- 3. DÉCOUPE TOTALEMENT VIDE USB-C (TRAVERSE DE HAUT EN BAS) ---
        // Évide tout le centre jusqu'au sol (0 en Z)
        translate([-largeur_prise_usb/2, -e_ext/2 - 0.1, -0.1])
            cube([largeur_prise_usb, e_ext + 0.2, hauteur_passage_usb + 0.1]);

        // --- 4. TROU HAUT / DROITE (Z = 45mm, X = +7mm) ---
        translate([9, e_ext/2 + 0.1, 50])
            trou_crochet();

        // --- 5. TROU BAS / GAUCHE (Z = 20mm, X = -7mm) ---
        translate([-9, e_ext/2 + 0.1, 35])
            trou_crochet();
    }
}

module equerre_simple() {
    union() {
        cube([section_crochet, branche_courte, section_crochet]);
        translate([0, branche_courte - section_crochet, 0])
            cube([branche_longue, section_crochet, section_crochet]);
    }
}

module trou_crochet() {
    // Trou carré traversant la paroi arrière d'arrière en avant
    translate([-(section_crochet + jeu)/2, -epaisseur_paroi - 0.2, -(section_crochet + jeu)/2])
        cube([
            section_crochet + jeu, 
            epaisseur_paroi + 0.4, 
            section_crochet + jeu
        ]);
}
