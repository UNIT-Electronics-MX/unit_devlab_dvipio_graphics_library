// Sprites rebotando (frutas Raspberry Pi y Eben Upton) para DevLab
// Adaptado de software/apps/sprite_bounce: el original usa libsprite
// (blitting acelerado en ensamblador, transformaciones afines con rotacion)
// que no esta expuesto por esta libreria Arduino. Aqui se logra el mismo
// efecto -sprites RGBA rebotando por la pantalla con transparencia- usando
// drawRGBBitmap() de Adafruit_GFX con una mascara 1-bit de transparencia.
// Los PNG originales (software/assets/raspberry_128x128.png y
// eben_128x128.png) se convirtieron a RGB565 + mascara con un script
// Python; no se incluye rotacion porque GFX no la soporta de forma nativa.

#include <upicodvi.h>
#include "raspberry_128x128.h"
#include "eben_128x128.h"

// Configuracion DevLab (misma que el resto de ejemplos devlab_*)
static const struct dvi_serialiser_cfg devlab_dvi_cfg = {
  .pio = pio0,
  .sm_tmds = {0, 1, 2},
  .pins_tmds = {14, 12, 8},
  .pins_clk = 10,
  .invert_diffpairs = false
};

// 320x240, 16-bit color. DVIGFX16 no soporta doble buffer (no hay RAM para
// dos framebuffers de 16-bit), asi que puede notarse algo de parpadeo.
DVIGFX16 display(DVI_RES_320x240p60, devlab_dvi_cfg);

#define N_SPRITES 10
#define SPRITE_SIZE 32 // tamano en pantalla (se reescala el PNG de 128x128)

struct Ball {
  int x, y;
  int vx, vy;
  bool isEben; // true = eben, false = raspberry
};

Ball balls[N_SPRITES];

int xmin, xmax, ymin, ymax;

void setup() {
  if (!display.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }

  xmin = 0;
  xmax = display.width() - SPRITE_SIZE;
  ymin = 0;
  ymax = display.height() - SPRITE_SIZE;

  for (int i = 0; i < N_SPRITES; i++) {
    balls[i].x = random(xmin, xmax);
    balls[i].y = random(ymin, ymax);
    balls[i].vx = random(1, 4) * (random(2) ? 1 : -1);
    balls[i].vy = random(1, 4) * (random(2) ? 1 : -1);
    balls[i].isEben = (i % 2) == 0;
  }

  display.fillScreen(display.color565(0, 30, 60));
}

// Dibuja el sprite de 128x128 reescalado a SPRITE_SIZE x SPRITE_SIZE,
// respetando la mascara de transparencia (vecino mas cercano).
void drawSprite(int x, int y, bool eben) {
  const uint16_t *img = eben ? eben_128x128 : raspberry_128x128;
  const uint8_t *mask = eben ? eben_128x128_mask : raspberry_128x128_mask;
  const int srcSize = 128;
  const int maskStride = (srcSize + 7) / 8;

  for (int dy = 0; dy < SPRITE_SIZE; dy++) {
    int sy = dy * srcSize / SPRITE_SIZE;
    for (int dx = 0; dx < SPRITE_SIZE; dx++) {
      int sx = dx * srcSize / SPRITE_SIZE;
      uint8_t maskByte = mask[sy * maskStride + (sx >> 3)];
      if (maskByte & (0x80 >> (sx & 7))) {
        display.drawPixel(x + dx, y + dy, img[sy * srcSize + sx]);
      }
    }
  }
}

void loop() {
  uint16_t bg = display.color565(0, 30, 60);

  // Borra la posicion anterior de cada sprite (solo su recuadro, no toda
  // la pantalla, para no perder demasiado tiempo en el redibujado).
  for (int i = 0; i < N_SPRITES; i++) {
    display.fillRect(balls[i].x, balls[i].y, SPRITE_SIZE, SPRITE_SIZE, bg);
  }

  for (int i = 0; i < N_SPRITES; i++) {
    balls[i].x += balls[i].vx;
    balls[i].y += balls[i].vy;
    if (balls[i].x <= xmin || balls[i].x >= xmax) {
      balls[i].vx = -balls[i].vx;
      balls[i].x = constrain(balls[i].x, xmin, xmax);
    }
    if (balls[i].y <= ymin || balls[i].y >= ymax) {
      balls[i].vy = -balls[i].vy;
      balls[i].y = constrain(balls[i].y, ymin, ymax);
    }
    drawSprite(balls[i].x, balls[i].y, balls[i].isEben);
  }

  display.setTextSize(1);
  display.setCursor(4, 4);
  display.setTextColor(display.color565(255, 255, 255));
  display.print("DevLab Sprite Bounce");
}
