// Fondo de bosque con parallax (dos capas de tiles) para DevLab
// Adaptado de software/apps/tiles_parallax: el original usa libtile/libsprite
// (blitting de tiles 16x16 en ensamblador con alpha) que no esta expuesto
// por esta libreria Arduino. Aqui se logra el mismo efecto -dos capas de
// tilemap desplazandose a distinta velocidad- dibujando tile por tile a
// mano con drawPixel(), usando una mascara de transparencia por pixel.
// El tileset (software/assets/platformer_in_the_forest.png) y los dos
// tilemaps (tilemap_background.h / tilemap_foreground.h) son los mismos
// datos del ejemplo original en software/apps/tiles_parallax/, solo que el
// tileset se convirtio de rgab5515 a RGB565 + mascara con un script Python.

#include <upicodvi.h>
#include "platformer_tileset.h"
#include "tilemap_background.h"
#include "tilemap_foreground.h"

// Configuracion DevLab (misma que el resto de ejemplos devlab_*)
static const struct dvi_serialiser_cfg devlab_dvi_cfg = {
  .pio = pio0,
  .sm_tmds = {0, 1, 2},
  .pins_tmds = {14, 12, 8},
  .pins_clk = 10,
  .invert_diffpairs = false
};

// 320x240, 16-bit color. DVIGFX16 no soporta doble buffer.
DVIGFX16 display(DVI_RES_320x240p60, devlab_dvi_cfg);

#define TILE_SIZE 16
#define TILESET_COLS (PLATFORMER_TILESET_WIDTH / TILE_SIZE)   // 17
#define TILESET_MASK_STRIDE ((PLATFORMER_TILESET_WIDTH + 7) / 8)

#define BG_COLS 32  // 512px / 16
#define BG_ROWS 16  // 256px / 16
#define FG_COLS 64  // 1024px / 16
#define FG_ROWS 16  // 256px / 16

static int bgScrollX = 0;
static int fgScrollX = 0;
static uint32_t frameCount = 0;

// Dibuja un tile de 16x16 (indice en el tileset) en (x,y) de pantalla,
// recortando lo que quede fuera y respetando la mascara de transparencia.
void blitTile(int x, int y, uint8_t tileIndex) {
  int tcol = tileIndex % TILESET_COLS;
  int trow = tileIndex / TILESET_COLS;
  int srcX0 = tcol * TILE_SIZE;
  int srcY0 = trow * TILE_SIZE;

  for (int dy = 0; dy < TILE_SIZE; dy++) {
    int py = y + dy;
    if (py < 0 || py >= display.height()) continue;
    int sy = srcY0 + dy;
    for (int dx = 0; dx < TILE_SIZE; dx++) {
      int px = x + dx;
      if (px < 0 || px >= display.width()) continue;
      int sx = srcX0 + dx;
      uint8_t maskByte = platformer_tileset_mask[sy * TILESET_MASK_STRIDE + (sx >> 3)];
      if (maskByte & (0x80 >> (sx & 7))) {
        display.drawPixel(px, py, platformer_tileset[sy * PLATFORMER_TILESET_WIDTH + sx]);
      }
    }
  }
}

// Dibuja una capa de tilemap completa, con scroll horizontal ciclico.
void drawLayer(const uint8_t *tilemap, int mapCols, int mapRows, int scrollX) {
  int firstCol = scrollX / TILE_SIZE;
  int subX = scrollX % TILE_SIZE;
  int colsOnScreen = display.width() / TILE_SIZE + 2;
  int rowsOnScreen = display.height() / TILE_SIZE + 1;

  for (int row = 0; row < rowsOnScreen && row < mapRows; row++) {
    for (int c = 0; c < colsOnScreen; c++) {
      int mapCol = (firstCol + c) % mapCols;
      uint8_t tileIndex = tilemap[row * mapCols + mapCol];
      blitTile(c * TILE_SIZE - subX, row * TILE_SIZE, tileIndex);
    }
  }
}

void setup() {
  if (!display.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }

  // Se limpia una sola vez aqui, no en loop(): DVIGFX16 no tiene doble
  // buffer, la salida DVI lee el framebuffer en vivo mientras se dibuja.
  // Un fillScreen() en cada frame se ve como un flash de color cubriendo
  // toda la pantalla mientras las capas (mas lentas, pixel por pixel) lo
  // vuelven a tapar. Las dos capas ya repintan todo el area visible cada
  // frame, asi que no hace falta limpiar de nuevo.
  // Cielo de fondo (mismo color que el original: 13,36,17 en RGB565).
  display.fillScreen(display.color565(0x68, 0xB4, 0x88));
}

void loop() {
  drawLayer(tilemap_background, BG_COLS, BG_ROWS, bgScrollX);
  drawLayer(tilemap_foreground, FG_COLS, FG_ROWS, fgScrollX);

  display.setTextSize(1);
  display.setCursor(4, 4);
  display.setTextColor(display.color565(255, 255, 255));
  display.print("DevLab Parallax");

  fgScrollX += 1;
  if (frameCount & 1) bgScrollX += 1; // capa de fondo se mueve mas lento
  frameCount++;
}
