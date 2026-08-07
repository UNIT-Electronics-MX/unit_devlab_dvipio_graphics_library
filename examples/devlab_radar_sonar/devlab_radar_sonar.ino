// Radar / Sonar giratorio, para DevLab
//
// Pantalla de radar clasica: aros concentricos, cruceta central, un haz
// que gira con una cola que se va apagando, y unos "blips" (objetivos)
// que se iluminan cada vez que el haz pasa sobre ellos y se apagan poco
// a poco hasta la siguiente pasada.
//
// Sigue el mismo patron que devlab_tron_mountains: cada frame se dibuja
// completo desde cero sobre el buffer trasero (fondo + aros + cola del
// haz + blips) y se promueve con un solo swap(), asi que no hay parpadeo
// aunque se redibuje toda la escena cada vez.

#include <upicodvi.h>

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

// --- Geometria de la pantalla ---
const int cx = 160, cy = 124;
const int R = 100;

// --- Paleta ---
#define COL_BG      0
#define COL_GRID    1  // aros y cruceta, verde tenue
#define COL_TEXT    2  // texto, blanco-verdoso

#define TRAIL_BASE   3
#define TRAIL_LEVELS 8   // 0 = mas tenue, TRAIL_LEVELS-1 = mas brillante

#define BLIP_BASE   (TRAIL_BASE + TRAIL_LEVELS)
#define BLIP_LEVELS 6    // 0 = apagado, BLIP_LEVELS-1 = brillante

uint16_t dimColor(uint16_t color, float factor) {
  uint8_t r = ((color >> 11) & 0x1F) * (1.0 - factor);
  uint8_t g = ((color >> 5) & 0x3F) * (1.0 - factor);
  uint8_t b = (color & 0x1F) * (1.0 - factor);
  return (r << 11) | (g << 5) | b;
}

void setupPalette() {
  display.setColor(COL_BG, 0x0000);
  display.setColor(COL_GRID, 0x0220);   // verde muy tenue
  display.setColor(COL_TEXT, 0xB7FF);   // blanco-verdoso

  uint16_t trailColor = 0x07E0; // verde radar
  for (int L = 0; L < TRAIL_LEVELS; L++) {
    // L=0 -> muy apagado, L=TRAIL_LEVELS-1 -> brillo total
    float factor = 0.92 * (1.0 - (float)L / (TRAIL_LEVELS - 1));
    display.setColor(TRAIL_BASE + L, dimColor(trailColor, factor));
  }

  uint16_t blipColor = 0xFD00; // naranja-rojo
  for (int L = 0; L < BLIP_LEVELS; L++) {
    float factor = 1.0 - (float)L / (BLIP_LEVELS - 1);
    display.setColor(BLIP_BASE + L, dimColor(blipColor, factor));
  }
}

// --- Haz giratorio ---
float sweepAngle = 0;        // grados, 0 = arriba, avanza en sentido horario
const float sweepStep = 3.0; // grados por frame
const int trailLines = 30;   // lineas que forman la cola detras del haz
const float trailSpacing = 2.0; // grados entre cada linea de la cola

// --- Blips (objetivos) ---
#define N_BLIPS 5
struct Blip {
  float angle;   // grados, posicion fija en el radar
  int radius;    // distancia al centro, px
  float glow;    // 0..1, brillo actual (decae con el tiempo)
};
Blip blips[N_BLIPS];

float angleDiff(float a, float b) {
  float d = fmod(a - b + 540.0, 360.0) - 180.0;
  return fabs(d);
}

void setup() {
  randomSeed(analogRead(A0));

  if (!display.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }

  setupPalette();
  display.swap(false, true); // duplica la paleta en ambos buffers

  for (int i = 0; i < N_BLIPS; i++) {
    blips[i].angle = random(0, 3600) / 10.0;
    blips[i].radius = random(20, R - 10);
    blips[i].glow = 0;
  }

  display.fillScreen(COL_BG);
  display.swap(true);
}

void drawBackground() {
  display.fillScreen(COL_BG);

  // Aros concentricos
  display.drawCircle(cx, cy, R, COL_GRID);
  display.drawCircle(cx, cy, (R * 2) / 3, COL_GRID);
  display.drawCircle(cx, cy, R / 3, COL_GRID);

  // Cruceta
  display.drawFastHLine(cx - R, cy, R * 2, COL_GRID);
  display.drawFastVLine(cx, cy - R, R * 2, COL_GRID);

  // Titulo y lectura de angulo
  display.setTextColor(COL_TEXT);
  display.setTextSize(1);
  display.setCursor(4, 4);
  display.print("RADAR SCAN");

  display.setCursor(4, 228);
  display.print("BRG: ");
  display.print((int)sweepAngle);
  display.print("deg");
}

void drawSweep() {
  for (int i = trailLines - 1; i >= 0; i--) {
    float a = sweepAngle - i * trailSpacing;
    int level = map(trailLines - 1 - i, 0, trailLines - 1, 0, TRAIL_LEVELS - 1);
    float rad = radians(a - 90);
    int ex = cx + (int)(R * cos(rad));
    int ey = cy + (int)(R * sin(rad));
    display.drawLine(cx, cy, ex, ey, TRAIL_BASE + level);
  }

  // Punto central brillante
  display.fillCircle(cx, cy, 2, TRAIL_BASE + TRAIL_LEVELS - 1);
}

void updateAndDrawBlips() {
  for (int i = 0; i < N_BLIPS; i++) {
    // Si el haz acaba de pasar sobre este blip, lo enciende al maximo
    if (angleDiff(sweepAngle, blips[i].angle) < sweepStep) {
      blips[i].glow = 1.0;
    } else {
      blips[i].glow *= 0.94; // se va apagando hasta la proxima pasada
    }

    if (blips[i].glow < 0.03) continue;

    float rad = radians(blips[i].angle - 90);
    int bx = cx + (int)(blips[i].radius * cos(rad));
    int by = cy + (int)(blips[i].radius * sin(rad));

    int level = (int)round(blips[i].glow * (BLIP_LEVELS - 1));
    display.fillCircle(bx, by, 3, BLIP_BASE + level);
  }
}

void loop() {
  drawBackground();
  drawSweep();
  updateAndDrawBlips();

  display.swap(); // frame completo, no hace falta copiar el anterior

  sweepAngle += sweepStep;
  if (sweepAngle >= 360.0) sweepAngle -= 360.0;

  delay(25); // ritmo de giro
}
