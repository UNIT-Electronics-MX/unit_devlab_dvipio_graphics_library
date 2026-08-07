// Mapa estilo Zelda con auto-scroll diagonal para DevLab
// Adaptado de software/apps/tiles: el original usa libtile (blitting de
// tiles 16x16 en ensamblador con alpha) que no esta expuesto por esta
// libreria Arduino. Aqui se logra el mismo efecto -camara recorriendo un
// mapa de tiles en diagonal- dibujando tile por tile con drawPixel() y una
// mascara de transparencia por pixel. El tileset
// (software/assets/oga_zelda_overworld_flat.png) y el mapa
// (software/apps/tiles/map_test_zelda_mini.h) son los mismos datos del
// ejemplo original, solo que el tileset se convirtio de rgab5515 a
// RGB565 + mascara con un script Python.

#include <upicodvi.h>
#include "zelda_tileset.h"
#include "map_test_zelda_mini.h"

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
#define TILESET_COLS (ZELDA_TILESET_WIDTH / TILE_SIZE)
#define TILESET_MASK_STRIDE ((ZELDA_TILESET_WIDTH + 7) / 8)

#define MAP_COLS 64 // 1024px / 16
#define MAP_ROWS 32 // 512px / 16

static int scrollX = 0;
static int scrollY = 0;

// Dibuja un tile de 16x16 (indice en el tileset) en (x,y) de pantalla,
// recortando lo que quede fuera de pantalla y del propio tileset (la
// imagen origen no es un multiplo exacto de 16px), y respetando la
// mascara de transparencia.
void blitTile(int x, int y, uint8_t tileIndex) {
  int tcol = tileIndex % TILESET_COLS;
  int trow = tileIndex / TILESET_COLS;
  int srcX0 = tcol * TILE_SIZE;
  int srcY0 = trow * TILE_SIZE;

  for (int dy = 0; dy < TILE_SIZE; dy++) {
    int py = y + dy;
    int sy = srcY0 + dy;
    if (py < 0 || py >= display.height() || sy >= ZELDA_TILESET_HEIGHT) continue;
    for (int dx = 0; dx < TILE_SIZE; dx++) {
      int px = x + dx;
      int sx = srcX0 + dx;
      if (px < 0 || px >= display.width() || sx >= ZELDA_TILESET_WIDTH) continue;
      uint8_t maskByte = zelda_tileset_mask[sy * TILESET_MASK_STRIDE + (sx >> 3)];
      if (maskByte & (0x80 >> (sx & 7))) {
        display.drawPixel(px, py, zelda_tileset[sy * ZELDA_TILESET_WIDTH + sx]);
      }
    }
  }
}

void drawMap(int sx, int sy) {
  int firstCol = sx / TILE_SIZE;
  int subX = sx % TILE_SIZE;
  int firstRow = sy / TILE_SIZE;
  int subY = sy % TILE_SIZE;
  int colsOnScreen = display.width() / TILE_SIZE + 2;
  int rowsOnScreen = display.height() / TILE_SIZE + 2;

  for (int r = 0; r < rowsOnScreen; r++) {
    int mapRow = (firstRow + r) % MAP_ROWS;
    for (int c = 0; c < colsOnScreen; c++) {
      int mapCol = (firstCol + c) % MAP_COLS;
      uint8_t tileIndex = map_test_zelda_mini[mapRow * MAP_COLS + mapCol];
      blitTile(c * TILE_SIZE - subX, r * TILE_SIZE - subY, tileIndex);
    }
  }
}

void setup() {
  if (!display.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }
}

void loop() {
  drawMap(scrollX, scrollY);

  display.setTextSize(1);
  display.setCursor(4, 4);
  display.setTextColor(display.color565(255, 255, 255));
  display.print("DevLab Tiles");

  scrollX += 3;
  scrollY += 2;
}
