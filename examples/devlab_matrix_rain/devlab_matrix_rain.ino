// Matrix Rain - Lluvia de caracteres estilo "Matrix", para DevLab
//
// Cada columna deja caer caracteres aleatorios: el ultimo caracter en
// caer (la "cabeza") se dibuja en blanco brillante, los que quedan
// arriba se recolorean a verde, y la cola se borra a medida que avanza.
// Cada columna cae a su propia velocidad para un efecto organico.
//
// Solo se dibujan los cambios de cada frame (cabeza nueva, recolorear
// la anterior, borrar la cola) en vez de redibujar toda la pantalla,
// asi que es muy barato de renderizar y funciona con doble buffer real
// (usa el mismo patron anti-parpadeo que devlab_light_cycles).

#include <upicodvi.h>
#include <string.h>

// Configuracion DevLab (misma que el resto de ejemplos devlab_*)
static const struct dvi_serialiser_cfg devlab_dvi_cfg = {
  .pio = pio0,
  .sm_tmds = {0, 1, 2},
  .pins_tmds = {14, 12, 8},
  .pins_clk = 10,
  .invert_diffpairs = false
};

// 320x240, 8-bit indexado, doble buffer (~150KB de RAM, cabe holgado)
DVIGFX8 display(DVI_RES_320x240p60, true, devlab_dvi_cfg);

// Celda de caracter: fuente por defecto de Adafruit_GFX es 5x7 con avance
// de 6x8 px a textSize 1.
#define COL_W 6
#define ROW_H 8
#define COLS (320 / COL_W)  // 53
#define ROWS (240 / ROW_H)  // 30

#define COL_BG    0  // negro
#define COL_HEAD  1  // blanco/verde palido (cabeza de la gota)
#define COL_TRAIL 2  // verde clasico (estela)

// Que caracter hay dibujado en cada celda (0 = vacio). Sirve para poder
// recolorear una celda sin tener que recordar/generar el mismo caracter
// aparte, y para saber que borrar cuando la cola avanza.
char gridChar[ROWS][COLS];

struct Column {
  int gy;                 // fila actual de la cabeza (puede ser negativa
                           // o >= ROWS mientras la gota entra/sale)
  int trailLen;            // longitud de la estela visible
  unsigned long lastStep;  // ultimo millis() en que avanzo
  unsigned long interval;  // ms entre pasos (velocidad de caida)
};

Column col[COLS];

void setupPalette() {
  display.setColor(COL_BG, 0x0000);    // negro
  display.setColor(COL_HEAD, 0xE7FF);  // blanco con tinte verde
  display.setColor(COL_TRAIL, 0x07E0); // verde puro
}

void resetColumn(int i) {
  col[i].gy = -random(1, 16);
  col[i].trailLen = random(6, 20);
  col[i].interval = random(40, 110);
  col[i].lastStep = millis() + random(0, 600); // escalona el arranque
}

void drawChar(int gy, int gx, char c, uint8_t color) {
  display.setTextColor(color);
  display.setCursor(gx * COL_W, gy * ROW_H);
  display.print(c);
}

void eraseCell(int gy, int gx) {
  display.fillRect(gx * COL_W, gy * ROW_H, COL_W, ROW_H, COL_BG);
}

char randomChar() {
  // Rango con digitos, letras y simbolos (mezcla tipo "codigo")
  return (char)(48 + random(79)); // '0'..'~' aprox
}

void stepColumn(int i) {
  int newHead = col[i].gy + 1;
  int tailRow = newHead - col[i].trailLen;

  if (tailRow > ROWS) {
    resetColumn(i);
    return;
  }

  // Borra la celda que la cola deja atras
  if (tailRow >= 0 && tailRow < ROWS && gridChar[tailRow][i] != 0) {
    eraseCell(tailRow, i);
    gridChar[tailRow][i] = 0;
  }

  // La cabeza anterior pasa a ser estela verde (mismo caracter, solo
  // cambia el color, asi que no deja residuos de otra forma de glifo)
  if (col[i].gy >= 0 && col[i].gy < ROWS && gridChar[col[i].gy][i] != 0) {
    drawChar(col[i].gy, i, gridChar[col[i].gy][i], COL_TRAIL);
  }

  // Nueva cabeza brillante
  if (newHead >= 0 && newHead < ROWS) {
    char c = randomChar();
    gridChar[newHead][i] = c;
    drawChar(newHead, i, c, COL_HEAD);
  }

  col[i].gy = newHead;
}

void setup() {
  randomSeed(analogRead(A0));

  if (!display.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }

  setupPalette();
  display.swap(false, true); // duplica la paleta en ambos buffers

  memset(gridChar, 0, sizeof(gridChar));
  for (int i = 0; i < COLS; i++) resetColumn(i);

  display.setTextWrap(false);
  display.setTextSize(1);
  display.fillScreen(COL_BG);
  display.swap(true); // arranca ambos buffers en negro
}

void loop() {
  unsigned long now = millis();
  bool changed = false;

  for (int i = 0; i < COLS; i++) {
    if (now - col[i].lastStep >= col[i].interval) {
      col[i].lastStep = now;
      stepColumn(i);
      changed = true;
    }
  }

  if (changed) {
    display.swap(true); // promueve el frame y mantiene ambos buffers en sync
  }
}
