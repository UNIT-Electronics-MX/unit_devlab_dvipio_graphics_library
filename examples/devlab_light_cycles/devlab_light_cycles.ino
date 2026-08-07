// Motos de luz (estilo Tron) para DevLab
// Demo automatica: dos "motos" dejan una estela de luz con efecto neon
// sobre un piso de rejilla, estilo arena de Tron. Giran solas evitando
// chocar contra los bordes y las estelas. Al chocar, la ronda termina,
// se muestra el ganador y el tablero se reinicia.

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

// 320x240, 8-bit indexado, doble buffer (~150KB de RAM, cabe holgado).
// Fondo, rejilla y glow no usan buffers extra: son solo mas dibujos
// sobre el mismo framebuffer, asi que no cuestan RAM adicional.
DVIGFX8 display(DVI_RES_320x240p60, true, devlab_dvi_cfg);

// --- Rejilla lógica del area de juego ---
#define CELL 8
#define GRID_W (320 / CELL)          // 40 columnas
#define PLAY_TOP 16                  // franja superior para el titulo
#define GRID_H ((240 - PLAY_TOP) / CELL) // 28 filas

// 0 = vacio, 1 = estela moto 1, 2 = estela moto 2
uint8_t grid[GRID_H][GRID_W];

// Paleta estilo Tron: piso azul oscuro + rejilla tenue + bordes/motos neon
#define COL_BG        0  // piso azul muy oscuro
#define COL_GRID      1  // lineas de rejilla tenues
#define COL_BORDER    2  // marco brillante cian
#define COL_TEXT      3  // texto blanco
#define COL_P1_GLOW   4  // halo tenue moto 1 (cian)
#define COL_P1        5  // estela moto 1 (cian brillante)
#define COL_P1_HEAD   6  // cabeza moto 1 (blanco-cian)
#define COL_P2_GLOW   7  // halo tenue moto 2 (naranja)
#define COL_P2        8  // estela moto 2 (naranja brillante)
#define COL_P2_HEAD   9  // cabeza moto 2 (amarillo)

enum Dir { UP, DOWN, LEFT, RIGHT };

struct Cycle {
  int gx, gy;         // posicion en celdas
  Dir dir;            // direccion actual
  uint8_t id;          // 1 o 2
  uint8_t glowColor;   // halo exterior tenue
  uint8_t trailColor;  // nucleo de la estela
  uint8_t headColor;   // nucleo de la cabeza
  bool alive;
};

Cycle p1, p2;
unsigned long lastStep = 0;
unsigned long stepInterval = 55; // ms entre movimientos (velocidad)
bool roundOver = false;
unsigned long roundOverAt = 0;
uint8_t roundWinner = 0; // 0 = empate

void setupPalette() {
  display.setColor(COL_BG,      0x0010); // azul marino muy oscuro
  display.setColor(COL_GRID,    0x0193); // azul-teal tenue
  display.setColor(COL_BORDER,  0x07FF); // cian brillante
  display.setColor(COL_TEXT,    0xFFFF); // blanco
  display.setColor(COL_P1_GLOW, 0x0410); // cian oscuro (halo)
  display.setColor(COL_P1,      0x07FF); // cian brillante
  display.setColor(COL_P1_HEAD, 0xFFFF); // blanco
  display.setColor(COL_P2_GLOW, 0x6180); // naranja oscuro (halo)
  display.setColor(COL_P2,      0xFD20); // naranja brillante
  display.setColor(COL_P2_HEAD, 0xFFE0); // amarillo
}

// Dibuja una celda con efecto "neon": un halo tenue que llena la celda
// completa y un nucleo brillante mas pequeño encima. Son 2 fillRect
// (uno de ellos ya pequeño), asi que sigue siendo muy barato de dibujar.
void drawCell(int gx, int gy, uint8_t glow, uint8_t core) {
  int px = gx * CELL;
  int py = PLAY_TOP + gy * CELL;
  display.fillRect(px, py, CELL, CELL, glow);
  display.fillRect(px + 1, py + 1, CELL - 2, CELL - 2, core);
}

// Dibuja el piso estilo Tron: fondo oscuro, rejilla tenue, marco brillante
// y titulo. Se llama una vez por ronda (no en cada frame), asi que su
// costo es insignificante frente al presupuesto de la demo.
void drawBackground() {
  display.fillScreen(COL_BG);

  // Rejilla de piso (cada 4 celdas)
  for (int gx = 0; gx <= GRID_W; gx += 4) {
    display.drawFastVLine(gx * CELL, PLAY_TOP, GRID_H * CELL, COL_GRID);
  }
  for (int gy = 0; gy <= GRID_H; gy += 4) {
    display.drawFastHLine(0, PLAY_TOP + gy * CELL, GRID_W * CELL, COL_GRID);
  }

  // Marco del area de juego
  display.drawRect(0, PLAY_TOP, GRID_W * CELL, GRID_H * CELL, COL_BORDER);

  // Barra de titulo
  display.drawFastHLine(0, PLAY_TOP - 1, 320, COL_BORDER);
  display.setTextColor(COL_BORDER);
  display.setTextSize(1);
  display.setCursor(96, 4);
  display.print("LIGHT CYCLES");
}

void resetRound() {
  memset(grid, 0, sizeof(grid));

  p1.gx = GRID_W / 4;
  p1.gy = GRID_H / 2;
  p1.dir = RIGHT;
  p1.id = 1;
  p1.glowColor = COL_P1_GLOW;
  p1.trailColor = COL_P1;
  p1.headColor = COL_P1_HEAD;
  p1.alive = true;

  p2.gx = (GRID_W * 3) / 4;
  p2.gy = GRID_H / 2;
  p2.dir = LEFT;
  p2.id = 2;
  p2.glowColor = COL_P2_GLOW;
  p2.trailColor = COL_P2;
  p2.headColor = COL_P2_HEAD;
  p2.alive = true;

  grid[p1.gy][p1.gx] = 1;
  grid[p2.gy][p2.gx] = 2;

  drawBackground();
  drawCell(p1.gx, p1.gy, p1.glowColor, p1.headColor);
  drawCell(p2.gx, p2.gy, p2.glowColor, p2.headColor);
  display.swap(true); // promueve el estado inicial a ambos buffers

  roundOver = false;
  roundWinner = 0;
}

void dirDelta(Dir d, int &dx, int &dy) {
  switch (d) {
    case UP:    dx = 0;  dy = -1; break;
    case DOWN:  dx = 0;  dy = 1;  break;
    case LEFT:  dx = -1; dy = 0;  break;
    case RIGHT: dx = 1;  dy = 0;  break;
  }
}

bool cellFree(int gx, int gy) {
  if (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H) return false;
  return grid[gy][gx] == 0;
}

// IA simple: sigue recto mientras pueda, si el camino se cierra
// prueba girar a la izquierda o derecha (lo que este libre), con
// un pequeño azar para que no siempre gire igual y el recorrido
// no se vea robotico.
Dir chooseDir(Cycle &c) {
  Dir left, right;
  switch (c.dir) {
    case UP:    left = LEFT;  right = RIGHT; break;
    case DOWN:  left = RIGHT; right = LEFT;  break;
    case LEFT:  left = DOWN;  right = UP;    break;
    case RIGHT: left = UP;    right = DOWN;  break;
    default:    left = c.dir; right = c.dir; break;
  }

  int dx, dy;
  dirDelta(c.dir, dx, dy);
  bool straightFree = cellFree(c.gx + dx, c.gy + dy);

  dirDelta(left, dx, dy);
  bool leftFree = cellFree(c.gx + dx, c.gy + dy);

  dirDelta(right, dx, dy);
  bool rightFree = cellFree(c.gx + dx, c.gy + dy);

  // Giro aleatorio ocasional aunque el camino recto este libre,
  // solo para dar vueltas y hacer la demo mas vistosa.
  if (straightFree && random(100) < 8) {
    if (leftFree && rightFree) return random(2) ? left : right;
    if (leftFree) return left;
    if (rightFree) return right;
  }

  if (straightFree) return c.dir;
  if (leftFree && rightFree) return random(2) ? left : right;
  if (leftFree) return left;
  if (rightFree) return right;
  return c.dir; // sin salida, se estrellara
}

void stepCycle(Cycle &c) {
  if (!c.alive) return;

  c.dir = chooseDir(c);

  // Convierte la cabeza actual en estela (glow + nucleo mas tenue)
  drawCell(c.gx, c.gy, c.glowColor, c.trailColor);

  int dx, dy;
  dirDelta(c.dir, dx, dy);
  int nx = c.gx + dx;
  int ny = c.gy + dy;

  if (!cellFree(nx, ny)) {
    c.alive = false;
    return;
  }

  c.gx = nx;
  c.gy = ny;
  grid[c.gy][c.gx] = c.id;
  drawCell(c.gx, c.gy, c.glowColor, c.headColor);
}

void showWinnerBanner() {
  int bx = 60, by = PLAY_TOP + (GRID_H * CELL) / 2 - 15, bw = 200, bh = 30;
  display.fillRect(bx, by, bw, bh, COL_BG);
  display.drawRect(bx, by, bw, bh, COL_BORDER);
  display.setTextColor(COL_TEXT);
  display.setTextSize(2);
  display.setCursor(bx + 15, by + 7);
  if (roundWinner == 1) {
    display.print("GANA MOTO 1");
  } else if (roundWinner == 2) {
    display.print("GANA MOTO 2");
  } else {
    display.print("EMPATE");
  }
}

void setup() {
  randomSeed(analogRead(A0));

  if (!display.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }

  setupPalette();
  // La paleta solo se definio en el buffer 'back' actual; la duplicamos
  // en el otro buffer una sola vez para que ambos usen los mismos colores
  // y no parpadee al alternar entre ellos.
  display.swap(false, true);
  resetRound();
}

void loop() {
  if (roundOver) {
    if (millis() - roundOverAt > 2500) {
      resetRound();
    }
    return;
  }

  if (millis() - lastStep < stepInterval) return;
  lastStep = millis();

  stepCycle(p1);
  stepCycle(p2);

  if (!p1.alive || !p2.alive) {
    if (!p1.alive && !p2.alive) roundWinner = 0;
    else if (!p1.alive) roundWinner = 2;
    else roundWinner = 1;

    display.swap(true);
    showWinnerBanner();
    display.swap(true);

    roundOver = true;
    roundOverAt = millis();
    return;
  }

  display.swap(true);
}
