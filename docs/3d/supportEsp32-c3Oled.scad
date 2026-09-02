// ====================================================
// SUPPORT ESP32-C3 SUR PIEDS + DEUX TROUS DE CROCHETS
// ====================================================

// --- PARAMÈTRES DU BOÎTIER ---
boitier_largeur = 28; // Largeur du boîtier (mm)
boitier_epaisseur = 8; // Épaisseur du boîtier (mm)

// --- PARAMÈTRES DU PASSAGE USB-C ---
largeur_prise_usb = 18; // Largeur d'ouverture USB
hauteur_passage_usb = 40;// Espace sous le boîtier

// --- PARAMÈTRES DU SUPPORT ET PIEDS ---
marge = 1.0; // Jeu d'insertion
epaisseur_paroi = 3.0; // Épaisseur des parois
marge_socle = 4; // Débord des pieds pour la stabilité

// --- PARAMÈTRES DES CROCHETS ---
section_crochet = 3; // Section (4x4mm)
branche_courte = (epaisseur_paroi*2) + 4; // Longueur insérée
branche_longue = 26; // Longueur d'accroche
jeu = 0.3; // Tolérance d'assemblage
position_depart_trou_crochet = 13;

$fn = 40;

// ====================================================
// ASSEMBLAGE
// ====================================================

// 1. Support vertical
support_esp32_simplifie();

// 2. Les 2 crochets à côté
translate([40, 0, 0]) piece_en_u();
translate([40, 25, 0]) piece_en_u();


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

            // 2 Pieds simples
            translate([-(l_ext + marge_socle)/2, -(e_ext + marge_socle)/2, 0])
                cube([10.5, e_ext + marge_socle, 4]);
            
            translate([9, -(e_ext + marge_socle)/2, 0])
                cube([10.5, e_ext + marge_socle, 4]);
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
            cube([largeur_prise_usb, e_ext + 0.2, hauteur_passage_usb + 0.2]);

        // --- 4. TROU HAUT / DROITE ---
        translate([position_depart_trou_crochet, e_ext/2 + 0.1, 50])
            trou_crochet();

        // --- 5. TROU HAUT / GAUCHE ---
        translate([position_depart_trou_crochet-branche_longue, e_ext/2 + 0.1, 50])
            trou_crochet();
        
        // --- 6. TROU BAS / DROITE ---
        translate([position_depart_trou_crochet, e_ext/2 + 0.1, 10])
            trou_crochet();
        
        // --- 7. TROU BAS / GAUCHE ---
        translate([position_depart_trou_crochet-branche_longue, e_ext/2 + 0.1, 10])
            trou_crochet();
    }
}

module piece_en_u() {
    union() {
        // 1. Première branche courte (votre premier bloc)
        cube([section_crochet, branche_courte, section_crochet]);
        
        // 2. Branche longue (la base du U)
        translate([0, branche_courte - section_crochet, 0])
            cube([branche_longue, section_crochet, section_crochet]);    
        
        // 3. Deuxième branche courte (le retour pour former le U)
        translate([branche_longue - section_crochet, 0, 0])
            cube([section_crochet, branche_courte, section_crochet]);
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
