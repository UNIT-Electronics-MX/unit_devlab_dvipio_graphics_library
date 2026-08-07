// Visor de imagenes a pantalla completa para DevLab (inspirado en
// software/apps/vista y software/apps/vista-palette)
//
// IMPORTANTE - por que esto NO es un port 1:1 de "vista":
// El "vista" original transmite imagenes de 640x480 directo desde flash
// externa via DMA crudo sobre el controlador QSPI/SSI (bypaseando el
// framebuffer), usa un binario copy_to_ram con boot_stage2 a medida, y
// carga los datos de imagen desde un .uf2 aparte que se flashea a un
// offset fijo, fuera del sketch. Ninguno de esos mecanismos existe en el
// flujo de subida de Arduino (solo sube un binario, sin DMA crudo ni
// control del linker/boot2), asi que no es portable tal cual.
//
// Lo que si es fiel al espiritu del ejemplo -mostrar fotos reales a
// pantalla completa, una tras otra- es esto: varias imagenes reales
// (software/assets/*.png convertidas a RGB565 a 320x240 con un script
// Python) embebidas normalmente en el sketch (flash interna via XIP, igual
// que cualquier otro array const), mostradas en carrusel.

#include <upicodvi.h>
#include "img_testcard.h"
#include "img_christmas.h"
#include "img_sunflower.h"
#include "img_ferris.h"
#include "img_apollo.h"

// Configuracion DevLab (misma que el resto de ejemplos devlab_*)
static const struct dvi_serialiser_cfg devlab_dvi_cfg = {
  .pio = pio0,
  .sm_tmds = {0, 1, 2},
  .pins_tmds = {14, 12, 8},
  .pins_clk = 10,
  .invert_diffpairs = false
};

// 320x240, 16-bit color. DVIGFX16 no soporta doble buffer (no hay RAM para
// dos framebuffers de 640x480 ni siquiera de 320x240 x2, por eso todas las
// imagenes se dejaron a 320x240).
DVIGFX16 display(DVI_RES_320x240p60, devlab_dvi_cfg);

struct SlideImage {
  const uint16_t *data;
  const char *name;
};

const SlideImage slides[] = {
  { (const uint16_t *)testcard_320x240, "Testcard" },
  { (const uint16_t *)wikimedia_christmas_tree_in_field_320x240, "Christmas Tree" },
  { img_sunflower, "Sunflower" },
  { img_ferris, "Ferris" },
  { img_apollo, "Apollo 8 Moon" },
};
const int N_SLIDES = sizeof(slides) / sizeof(slides[0]);

void setup() {
  if (!display.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }
}

void loop() {
  for (int i = 0; i < N_SLIDES; i++) {
    display.drawRGBBitmap(0, 0, slides[i].data, display.width(), display.height());

    display.setTextSize(1);
    display.setCursor(4, display.height() - 12);
    display.setTextColor(0x0000);
    display.print(slides[i].name);
    display.setCursor(3, display.height() - 13);
    display.setTextColor(0xFFFF);
    display.print(slides[i].name);

    delay(3000);
  }
}
