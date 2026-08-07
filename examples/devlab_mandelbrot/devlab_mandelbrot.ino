// Fractal de Mandelbrot animado (zoom infinito) para DevLab
// Adaptado de software/apps/mandel-full y software/apps/mandelbrot:
// aqui se recalcula el fractal completo cada frame en lugar de usar el
// generador incremental con deteccion de ciclos de la version original en C
// puro (esa parte vive en libsprite y no esta expuesta por la libreria
// Arduino), pero el resultado visual -zoom continuo con paleta ciclica- es
// el mismo.

#include <upicodvi.h>

// Configuracion DevLab (misma que el resto de ejemplos devlab_*)
static const struct dvi_serialiser_cfg devlab_dvi_cfg = {
  .pio = pio0,
  .sm_tmds = {0, 1, 2},
  .pins_tmds = {14, 12, 8},
  .pins_clk = 10,
  .invert_diffpairs = false
};

// 320x240, 8-bit indexado, doble buffer para que el redibujado no parpadee.
DVIGFX8 display(DVI_RES_320x240p60, true, devlab_dvi_cfg);

#define MAX_ITER 255

// Ventana actual del plano complejo
static float minx = -2.25f, maxx = 0.75f;
static float miny = -1.5f, maxy = 1.5f;

// Punto sobre el que hacemos zoom (ubicado en una zona "interesante"
// del set de Mandelbrot, cerca del valle entre el cardioide y el bulbo).
static const float zoomTargetX = -0.745428f;
static const float zoomTargetY = 0.113009f;

static uint8_t paletteOffset = 0;
static int zoomStep = 0;
#define ZOOM_STEPS 90     // pasos antes de reiniciar el zoom
#define ZOOM_FACTOR 0.93f // que tanto se encoge la ventana cada paso

void buildPalette() {
  for (int i = 0; i < 256; i++) {
    uint8_t c = (uint8_t)(i + paletteOffset);
    uint8_t r, g, b;
    if (c == 0) {
      r = g = b = 0; // dentro del set: negro
    } else {
      // Bandas de color ciclicas, estilo arcoiris continuo.
      float t = (c % 64) / 64.0f;
      float phase = (c / 64) * (3.14159f / 2.0f);
      r = (uint8_t)(128 + 127 * sin(t * 6.2832f + phase));
      g = (uint8_t)(128 + 127 * sin(t * 6.2832f + phase + 2.094f));
      b = (uint8_t)(128 + 127 * sin(t * 6.2832f + phase + 4.188f));
    }
    display.setColor(i, r, g, b);
  }
  paletteOffset += 3;
}

void resetView() {
  minx = -2.25f;
  maxx = 0.75f;
  miny = -1.5f;
  maxy = 1.5f;
  zoomStep = 0;
}

void setup() {
  if (!display.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }
  resetView();
}

void renderMandelbrot() {
  int w = display.width();
  int h = display.height();
  float sx = (maxx - minx) / w;
  float sy = (maxy - miny) / h;

  for (int y = 0; y < h; y++) {
    float ci = miny + y * sy;
    for (int x = 0; x < w; x++) {
      float cr = minx + x * sx;
      float zr = 0, zi = 0;
      int iter = 0;
      while (zr * zr + zi * zi <= 4.0f && iter < MAX_ITER) {
        float zrtemp = zr * zr - zi * zi + cr;
        zi = 2.0f * zr * zi + ci;
        zr = zrtemp;
        iter++;
      }
      display.drawPixel(x, y, iter == MAX_ITER ? 0 : (uint8_t)iter);
    }
  }
}

void loop() {
  buildPalette();
  renderMandelbrot();

  display.setTextSize(1);
  display.setCursor(4, display.height() - 10);
  display.setTextColor(255, 0);
  display.print("DevLab Mandelbrot  zoom ");
  display.print(zoomStep);

  display.swap();

  // Acercar la ventana hacia el punto objetivo.
  float cx = minx + (maxx - minx) * 0.5f;
  float cy = miny + (maxy - miny) * 0.5f;
  cx = cx + (zoomTargetX - cx) * (1.0f - ZOOM_FACTOR);
  cy = cy + (zoomTargetY - cy) * (1.0f - ZOOM_FACTOR);
  float halfw = (maxx - minx) * 0.5f * ZOOM_FACTOR;
  float halfh = (maxy - miny) * 0.5f * ZOOM_FACTOR;
  minx = cx - halfw;
  maxx = cx + halfw;
  miny = cy - halfh;
  maxy = cy + halfh;

  if (++zoomStep >= ZOOM_STEPS) resetView();
}
