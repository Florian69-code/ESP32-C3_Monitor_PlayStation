#include "src/app/app_logic.h"
#include "src/display_ui.h"

// Afficheur OLED SSD1306 72x40 en mode matériel I2C.
U8G2_SSD1306_72X40_ER_F_HW_I2C display(
    U8G2_R0,
    U8X8_PIN_NONE,
    OLED_SCL_PIN,
    OLED_SDA_PIN);

// Sequence d'initialisation unique du firmware.
void setup() {
  initializeApp();
}

// Boucle cooperative principale du firmware.
void loop() {
  updateApp();
}
