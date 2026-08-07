// Foto de la luna a resolucion completa (640x480, 1-bit) para DevLab
// Adaptado de software/apps/moon: el original transmite el bitmap
// directamente al codificador TMDS cuadro a cuadro; aqui simplemente se
// vuelca la misma imagen (software/assets/moon_1bpp_640x480.h, copiada tal
// cual a este ejemplo) al framebuffer 1-bit de la libreria Arduino una sola
// vez, y se hace un pequeno efecto de "parpadeo de estrellas" encima para
// que la demo no sea una imagen totalmente estatica.

#include <upicodvi.h>
#include "moon_1bpp_640x480.h"

// Configuracion DevLab (misma que el resto de ejemplos devlab_*)
static const struct dvi_serialiser_cfg devlab_dvi_cfg = {
  .pio = pio0,
  .sm_tmds = {0, 1, 2},
  .pins_tmds = {14, 12, 8},
  .pins_clk = 10,
  .invert_diffpairs = false
};

// 640x480, 1-bit, sin doble buffer: es una imagen fija, no hace falta.
DVIGFX1 display(DVI_RES_640x480p60, false, devlab_dvi_cfg);

void setup() {
  if (!display.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }

  // La imagen ya viene empaquetada 1bpp, MSB primero, filas alineadas a
  // byte -- exactamente el formato que espera drawBitmap() de Adafruit_GFX.
  display.drawBitmap(0, 0, (const uint8_t *)moon_1bpp_640x480,
                      640, 480, 1);
}

void loop() {
  // Pequeno efecto de "titileo" en el cielo: prende/apaga pixeles sueltos
  // en la franja superior oscura, simulando estrellas.
  for (int i = 0; i < 40; i++) {
    int x = random(640);
    int y = random(60); // franja de cielo oscuro sobre la luna
    display.drawPixel(x, y, random(4) == 0 ? 1 : 0);
  }
  delay(80);
}
