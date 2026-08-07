// Mapa con personajes caminando (animacion de 4 direcciones) para DevLab
// Adaptado de software/apps/tiles_and_sprites: el original usa libtile +
// libsprite (blitting en ensamblador) para mover una camara sobre un mapa
// de 512x256 y animar hasta 100 personajes con 4 direcciones x 4 cuadros de
// animacion, todo tomado del mismo atlas de tiles
// (software/assets/zelda_mini_plus_walk.png, indices de tile 102 en
// adelante). Como esta libreria Arduino no expone libtile/libsprite, aqui
// se reimplementa el blit tile por tile. Se reduce la cantidad de
// personajes (12 en vez de 100) porque el blit por software es mas lento
// que el original en ASM.
//
// Por que DVIGFX8 (paleta 8-bit) y no DVIGFX16:
// Las primeras versiones de este ejemplo usaban DVIGFX16, que NO tiene
// doble buffer -no hay RAM para dos framebuffers de 16-bit-. La salida DVI
// lee ese unico buffer en vivo mientras el sketch lo modifica, asi que
// CUALQUIER objeto que se redibuje cuadro a cuadro puede parpadear por
// pura coincidencia de tiempos entre el CPU escribiendo y el hardware de
// video leyendo, sin importar que tan rapido sea el redibujado -reducir el
// tiempo de dibujo baja la probabilidad pero no elimina el problema-.
// DVIGFX8 si soporta doble buffer real: se dibuja siempre sobre el buffer
// "de atras" y swap() lo intercambia con el de "adelante" en la siguiente
// sincronia vertical, asi que la pantalla nunca muestra un cuadro a medio
// dibujar. El costo es que los sprites/tileset tienen que ser paleta
// indexada (256 colores) en vez de RGB565 directo (conversion ya hecha en
// zelda_walk_pal.h con un script Python; indice 0 = transparente).
//
// Para que sea eficiente ademas de sin parpadeo, se sigue usando la misma
// tecnica de "solo redibujar lo que cambio" (dirty rectangles): en vez de
// repintar las ~370 tiles visibles cada frame, se borra cada personaje
// solo en su rectangulo, el scroll de camara desplaza el contenido ya
// dibujado (memmove) y solo se redibuja la franja nueva expuesta, y se usa
// swap(copy_framebuffer=true) para que el nuevo "back buffer" empiece
// siendo una copia de lo que ya esta en pantalla (no hay que redibujar
// todo desde cero cada vez).

#include <upicodvi.h>
#include "zelda_walk_pal.h"
#include "tilemap.h"

// Configuracion DevLab (misma que el resto de ejemplos devlab_*)
static const struct dvi_serialiser_cfg devlab_dvi_cfg = {
  .pio = pio0,
  .sm_tmds = {0, 1, 2},
  .pins_tmds = {14, 12, 8},
  .pins_clk = 10,
  .invert_diffpairs = false
};

// 320x240, 8-bit paleta, doble buffer real (sin parpadeo).
DVIGFX8 display(DVI_RES_320x240p60, true, devlab_dvi_cfg);

#define TILE_SIZE 16
#define TILESET_COLS (ZELDA_WALK_PAL_WIDTH / TILE_SIZE) // 17
#define TRANSPARENT_INDEX 0

#define MAP_COLS 32   // 512px / 16
#define MAP_ROWS 16   // 256px / 16
#define MAP_WIDTH_PX (MAP_COLS * TILE_SIZE)
#define MAP_HEIGHT_PX (MAP_ROWS * TILE_SIZE)

#define N_CHARACTERS 12
#define CHAR_BASE_TILE 102 // primer tile de personaje (direccion 0, cuadro 0)
#define CHAR_W TILE_SIZE
#define CHAR_H (TILE_SIZE * 2) // torso + piernas

// Rectangulo del texto superpuesto en pantalla (posicion fija). Tambien
// hay que "borrarlo" (repintar mapa debajo) antes de reescribirlo, igual
// que un personaje mas, para que el scroll de camara no arrastre copias
// fantasma del texto.
#define TEXT_X 4
#define TEXT_Y 4
#define TEXT_W 132
#define TEXT_H 10

struct Character {
  int16_t x, y;   // posicion en el mapa (pixeles)
  uint8_t dir;    // 0=abajo 1=derecha 2=arriba 3=izquierda
  uint8_t frame;  // cuadro de animacion 0-3
};

Character chars[N_CHARACTERS];
int16_t camX = 0, camY = 0;
uint32_t frameCtr = 0;

// Dibuja un tile de 16x16 del atlas zelda_walk_pal en (x,y) de pantalla,
// recortado ademas al rectangulo [clipX0,clipY0)-(clipX1,clipY1). Escribe
// directo al buffer de indices de color (display.getBuffer()), que con
// DVIGFX8 SIEMPRE es el "back buffer" actual (drawPixel() haria lo mismo,
// pero con overhead de limites/rotacion por cada pixel). Asume rotation==0.
void blitTileClipped(int x, int y, int tileIndex,
                      int clipX0, int clipY0, int clipX1, int clipY1) {
  int tcol = tileIndex % TILESET_COLS;
  int trow = tileIndex / TILESET_COLS;
  int srcX0 = tcol * TILE_SIZE;
  int srcY0 = trow * TILE_SIZE;

  int dyStart = max(0, max(-y, clipY0 - y));
  int dyEnd = min(TILE_SIZE, min(clipY1 - y, ZELDA_WALK_PAL_HEIGHT - srcY0));
  int dxStart = max(0, max(-x, clipX0 - x));
  int dxEnd = min(TILE_SIZE, min(clipX1 - x, ZELDA_WALK_PAL_WIDTH - srcX0));
  if (dxStart >= dxEnd || dyStart >= dyEnd) return;

  uint8_t *buf = display.getBuffer();
  int dispW = display.width();

  for (int dy = dyStart; dy < dyEnd; dy++) {
    int sy = srcY0 + dy;
    const uint8_t *srcRow = &zelda_walk_pal[sy * ZELDA_WALK_PAL_WIDTH];
    uint8_t *dstRow = &buf[(y + dy) * dispW];
    for (int dx = dxStart; dx < dxEnd; dx++) {
      uint8_t idx = srcRow[srcX0 + dx];
      if (idx != TRANSPARENT_INDEX) dstRow[x + dx] = idx;
    }
  }
}

void blitTile(int x, int y, int tileIndex) {
  blitTileClipped(x, y, tileIndex, 0, 0, display.width(), display.height());
}

// Redibuja el fondo de mapa (usando camX/camY actuales) dentro del
// rectangulo de pantalla dado. Se usa tanto para "borrar" personajes/texto
// (redibujando el mapa debajo) como para rellenar la franja que queda
// expuesta al desplazar el buffer por el scroll de camara.
void redrawMapRect(int rx0, int ry0, int rx1, int ry1) {
  rx0 = max(rx0, 0);
  ry0 = max(ry0, 0);
  rx1 = min(rx1, (int)display.width());
  ry1 = min(ry1, (int)display.height());
  if (rx0 >= rx1 || ry0 >= ry1) return;

  int firstCol = (camX + rx0) / TILE_SIZE;
  int lastCol = (camX + rx1 - 1) / TILE_SIZE;
  int firstRow = (camY + ry0) / TILE_SIZE;
  int lastRow = (camY + ry1 - 1) / TILE_SIZE;

  for (int mapRow = firstRow; mapRow <= lastRow; mapRow++) {
    if (mapRow < 0 || mapRow >= MAP_ROWS) continue;
    int tileScreenY = mapRow * TILE_SIZE - camY;
    for (int mapCol = firstCol; mapCol <= lastCol; mapCol++) {
      if (mapCol < 0 || mapCol >= MAP_COLS) continue;
      int tileScreenX = mapCol * TILE_SIZE - camX;
      uint8_t tileIndex = tilemap[mapRow * MAP_COLS + mapCol];
      blitTileClipped(tileScreenX, tileScreenY, tileIndex, rx0, ry0, rx1, ry1);
    }
  }
}

// Desplaza el contenido ya dibujado del framebuffer por (dx,dy) (solo uno
// de los dos es distinto de cero: la camara se mueve en un solo eje a la
// vez) y rellena con mapa fresco la franja que queda expuesta.
void scrollFramebuffer(int dx, int dy) {
  uint8_t *buf = display.getBuffer();
  int w = display.width();
  int h = display.height();

  if (dx > 0) { // camara a la derecha: contenido se desliza a la izquierda
    for (int y = 0; y < h; y++) {
      uint8_t *row = &buf[y * w];
      memmove(row, row + dx, w - dx);
    }
    redrawMapRect(w - dx, 0, w, h);
  } else if (dx < 0) {
    int d = -dx;
    for (int y = 0; y < h; y++) {
      uint8_t *row = &buf[y * w];
      memmove(row + d, row, w - d);
    }
    redrawMapRect(0, 0, d, h);
  }

  if (dy > 0) { // camara hacia abajo: contenido se desliza hacia arriba
    memmove(buf, buf + dy * w, (h - dy) * w);
    redrawMapRect(0, h - dy, w, h);
  } else if (dy < 0) {
    int d = -dy;
    memmove(buf + d * w, buf, (h - d) * w);
    redrawMapRect(0, 0, w, d);
  }
}

void drawCharacter(const Character &ch, int camXforDraw, int camYforDraw) {
  int screenX = ch.x - camXforDraw;
  int screenY = ch.y - camYforDraw;
  int tileIndex = CHAR_BASE_TILE + (ch.dir << 2) + ch.frame;
  // El personaje ocupa 2 tiles de alto (torso+piernas), como en el original.
  blitTile(screenX, screenY, tileIndex);
  blitTile(screenX, screenY + TILE_SIZE, tileIndex + TILESET_COLS);
}

void setup() {
  if (!display.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }

  // Paleta: indice 0 se usa como "transparente" al dibujar sprites, pero
  // tambien es un color valido de fondo (se le asigna el mismo tono que el
  // original usaba de relleno). Los indices 1-254 vienen del PNG original
  // cuantizado a 8-bit; el 255 se reserva aparte para texto en blanco.
  display.setColor(0, 0x40, 0x90, 0x50);
  for (int i = 1; i <= 254; i++) display.setColor(i, zelda_walk_pal_palette[i]);
  display.setColor(255, 0xFFFF); // blanco, para la UI de texto

  for (int i = 0; i < N_CHARACTERS; i++) {
    chars[i].x = random(8, MAP_WIDTH_PX - 24);
    chars[i].y = random(0, MAP_HEIGHT_PX - 32);
    chars[i].dir = random(4);
    chars[i].frame = 0;
  }

  // Mapa completo inicial (unica vez que se redibuja TODA la pantalla) +
  // personajes, y primer swap con copia de framebuffer y paleta para que
  // el nuevo "back buffer" arranque siendo una copia de lo que se ve.
  redrawMapRect(0, 0, display.width(), display.height());
  for (int i = 0; i < N_CHARACTERS; i++) drawCharacter(chars[i], camX, camY);
  display.swap(true, true);
}

void loop() {
  // 1) Borrar cada personaje y el texto en su posicion actual (repintando
  //    solo el mapa debajo), antes de mover nada.
  for (int i = 0; i < N_CHARACTERS; i++) {
    int sx = chars[i].x - camX;
    int sy = chars[i].y - camY;
    redrawMapRect(sx, sy, sx + CHAR_W, sy + CHAR_H);
  }
  redrawMapRect(TEXT_X, TEXT_Y, TEXT_X + TEXT_W, TEXT_Y + TEXT_H);

  // 2) Mover camara en un patron ciclico: derecha, abajo, izquierda, arriba.
  const int CAMERA_SPEED = 3;
  int phase = frameCtr % 200;
  int16_t oldCamX = camX, oldCamY = camY;
  if (phase < 50) camX += CAMERA_SPEED;
  else if (phase < 100) camY += CAMERA_SPEED;
  else if (phase < 150) camX -= CAMERA_SPEED;
  else camY -= CAMERA_SPEED;
  camX = constrain(camX, 0, (int16_t)(MAP_WIDTH_PX - display.width()));
  camY = constrain(camY, 0, (int16_t)(MAP_HEIGHT_PX - display.height()));
  scrollFramebuffer(camX - oldCamX, camY - oldCamY);

  // 3) Actualizar y dibujar personajes en su posicion nueva.
  const int CHAR_SPEED = 2;
  for (int i = 0; i < N_CHARACTERS; i++) {
    Character &ch = chars[i];
    if ((frameCtr & 0x3) == 0) ch.frame = (ch.frame + 1) & 0x3;
    if (random(0, 0x100) == 0) {
      ch.frame = 0;
      ch.dir = random(4);
    }
    if (ch.dir == 1) ch.x += CHAR_SPEED;
    else if (ch.dir == 3) ch.x -= CHAR_SPEED;
    if (ch.dir == 0) ch.y += CHAR_SPEED;
    else if (ch.dir == 2) ch.y -= CHAR_SPEED;
    ch.x = constrain(ch.x, 8, MAP_WIDTH_PX - 24);
    ch.y = constrain(ch.y, 0, MAP_HEIGHT_PX - 32);

    drawCharacter(ch, camX, camY);
  }

  display.setTextSize(1);
  display.setCursor(TEXT_X, TEXT_Y);
  display.setTextColor(255); // sin color de fondo, para no dejar un recuadro
  display.print("DevLab Tiles+Sprites");

  // 4) Mostrar todo lo dibujado de una sola vez (sin desgarros/parpadeo) y
  //    dejar el siguiente back buffer listo como copia de esto.
  display.swap(true, true);

  frameCtr++;
}
