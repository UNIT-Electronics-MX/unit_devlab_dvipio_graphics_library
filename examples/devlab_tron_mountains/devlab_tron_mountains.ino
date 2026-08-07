// TRON Mountains - Paisaje 3D wireframe en movimiento, para DevLab
//
// Portado desde un ejemplo equivalente escrito para la libreria UDVI_HSTX
// (Arduino Nano RP2350 + interfaz HSTX). La logica del terreno y la
// proyeccion 3D son identicas al original; lo que cambia es la capa de
// hardware/API y el modo de color:
//   - UDVI_HSTX.h            -> upicodvi.h
//   - DVHSTXPinout/DVHSTX16  -> dvi_serialiser_cfg / DVIGFX8 (PIO, no HSTX)
//   - DVHSTX_RESOLUTION_320x240 -> DVI_RES_320x240p60
//
// Nota sobre el parpadeo: la primera version usaba DVIGFX16 (RGB565), que
// en este chip NO soporta doble buffer (no alcanza la RAM para 2 buffers
// de ~150KB). Redibujar cielo+terreno cada frame directo sobre el unico
// framebuffer se ve mientras el HDMI lo esta escaneando -> parpadeo.
// La solucion es usar DVIGFX8 (8-bit indexado a paleta) CON doble buffer:
// los tonos continuos de degradado/atenuacion se precalculan como
// entradas de paleta en setup(), y cada frame se dibuja completo sobre
// el buffer trasero y se promueve de una vez con swap().

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

// Configuración del terreno
const int GRID_W = 32;  // Ancho de la malla
const int GRID_D = 32;  // Profundidad de la malla
float terrain[GRID_D][GRID_W];

// Parámetros de cámara y proyección
float camX = 0;
float camY = 80;   // Altura de la cámara
float camZ = -50;  // Posición Z
float horizon = 100;

// Offset de animación
float scrollOffset = 0;

// --- Paleta ---
// Colores base (RGB565) usados solo para precalcular la paleta en setup().
uint16_t colorGrid565 = 0x07FF;     // Cyan brillante
uint16_t colorPeak565 = 0xF81F;     // Magenta para picos
uint16_t colorValley565 = 0x0410;   // Cyan oscuro para valles

// Indices 0-31: gradiente de cielo (mismo valor numerico = tono de azul,
// igual que el canal azul crudo que usaba el original). El indice 16
// coincide ademas con 0x0010, asi que sirve para la linea de horizonte,
// y el indice 0 sirve como negro solido.
#define SKY_LEVELS 32

// A partir de aqui: 3 colores base de terreno (pico/grid/valle) x niveles
// de atenuacion por distancia. TERRAIN_LEVELS niveles cubren desde brillo
// total (nivel 0) hasta casi apagado por niebla (nivel TERRAIN_LEVELS-1).
#define TERRAIN_BASE_IDX SKY_LEVELS
#define TERRAIN_LEVELS 11
#define IDX_PEAK   0
#define IDX_GRID   1
#define IDX_VALLEY 2

#define COL_HORIZON_GRID (TERRAIN_BASE_IDX + 3 * TERRAIN_LEVELS)     // linea vertical horizonte
#define COL_TEXT_CYAN     (TERRAIN_BASE_IDX + 3 * TERRAIN_LEVELS + 1)
#define COL_TEXT_GREEN    (TERRAIN_BASE_IDX + 3 * TERRAIN_LEVELS + 2)

uint16_t dimColor(uint16_t color, float factor) {
  // Reducir brillo multiplicando cada componente
  uint8_t r = ((color >> 11) & 0x1F) * (1.0 - factor);
  uint8_t g = ((color >> 5) & 0x3F) * (1.0 - factor);
  uint8_t b = (color & 0x1F) * (1.0 - factor);

  return (r << 11) | (g << 5) | b;
}

uint8_t terrainColorIndex(uint8_t baseId, float factor) {
  int level = (int)round(constrain(factor, 0.0f, 1.0f) * (TERRAIN_LEVELS - 1));
  return TERRAIN_BASE_IDX + baseId * TERRAIN_LEVELS + level;
}

void setupPalette() {
  // Gradiente de cielo: 32 tonos de azul (0 = negro/horizonte lejano)
  for (int i = 0; i < SKY_LEVELS; i++) {
    display.setColor(i, (uint16_t)i);
  }

  // Tabla de atenuacion por distancia para cada color base de terreno
  uint16_t bases[3] = {colorPeak565, colorGrid565, colorValley565};
  for (int c = 0; c < 3; c++) {
    for (int L = 0; L < TERRAIN_LEVELS; L++) {
      float factor = (float)L / (TERRAIN_LEVELS - 1);
      display.setColor(TERRAIN_BASE_IDX + c * TERRAIN_LEVELS + L,
                        dimColor(bases[c], factor));
    }
  }

  display.setColor(COL_HORIZON_GRID, 0x0208);
  display.setColor(COL_TEXT_CYAN, colorGrid565);
  display.setColor(COL_TEXT_GREEN, 0x07E0);
}

void setup() {
  if (!display.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }

  setupPalette();
  // Duplicar la paleta en ambos buffers una sola vez para que no
  // parpadeen los colores al alternar entre ellos.
  display.swap(false, true);

  // Generar terreno inicial
  generateTerrain();
}

void loop() {
  // Limpiar y dibujar cielo con gradiente sobre el buffer trasero
  drawSky();

  // Actualizar terreno (scroll infinito)
  scrollOffset += 0.3;
  if (scrollOffset >= 1.0) {
    scrollOffset -= 1.0;
    shiftTerrain();
  }

  // Dibujar malla 3D
  drawTerrain();

  // Info en pantalla
  display.setCursor(5, 5);
  display.setTextColor(COL_TEXT_CYAN, 0);
  display.setTextSize(1);
  display.print("TRON MOUNTAINS");

  display.setCursor(5, 225);
  display.setTextColor(COL_TEXT_GREEN, 0);
  display.print("SPEED: ");
  display.print((int)(scrollOffset * 100));
  display.print("%");

  // El frame se dibujo completo desde cero en el buffer trasero, asi que
  // no hace falta copiar el framebuffer anterior: solo promoverlo.
  display.swap();

  delay(30); // ~33 FPS
}

void drawSky() {
  // Gradiente de cielo estilo Tron
  for (int y = 0; y < display.height() / 2; y++) {
    uint8_t blue = map(y, 0, display.height() / 2, 0, 31);
    display.drawFastHLine(0, y, display.width(), blue);
  }

  // Horizonte con líneas de grid
  for (int y = display.height() / 2; y < display.height(); y++) {
    // Grid horizontal cada 10 píxeles
    if ((y - display.height() / 2) % 10 == 0) {
      display.drawFastHLine(0, y, display.width(), 16); // == 0x0010
    } else {
      display.drawFastHLine(0, y, display.width(), 0);  // negro
    }
  }
}

void generateTerrain() {
  for (int z = 0; z < GRID_D; z++) {
    for (int x = 0; x < GRID_W; x++) {
      // Generar altura con múltiples ondas
      float height = 0;

      // Onda principal
      height += sin((x * 0.3 + z * 0.2) * 0.5) * 20;

      // Ondas secundarias
      height += sin((x * 0.5 - z * 0.3) * 0.8) * 10;
      height += sin((x * 0.8 + z * 0.5) * 1.2) * 5;

      // Ruido adicional
      height += (random(100) - 50) * 0.1;

      terrain[z][x] = height;
    }
  }
}

void shiftTerrain() {
  // Mover todas las filas hacia adelante
  for (int z = 0; z < GRID_D - 1; z++) {
    for (int x = 0; x < GRID_W; x++) {
      terrain[z][x] = terrain[z + 1][x];
    }
  }

  // Generar nueva fila al final
  for (int x = 0; x < GRID_W; x++) {
    float offset = (millis() / 1000.0) * 2.0;

    float height = 0;
    height += sin((x * 0.3 + offset) * 0.5) * 20;
    height += sin((x * 0.5 - offset * 0.5) * 0.8) * 10;
    height += sin((x * 0.8 + offset * 0.3) * 1.2) * 5;
    height += (random(100) - 50) * 0.1;

    terrain[GRID_D - 1][x] = height;
  }
}

void drawTerrain() {
  // Dibujar la malla de atrás hacia adelante
  for (int z = GRID_D - 1; z > 0; z--) {
    for (int x = 0; x < GRID_W - 1; x++) {
      // Calcular posición 3D con interpolación para scroll suave
      float z1 = z + scrollOffset;
      float z2 = z + scrollOffset;
      float z3 = z - 1 + scrollOffset;
      float z4 = z - 1 + scrollOffset;

      // Vértices del quad
      float x1 = (x - GRID_W / 2) * 10;
      float y1 = terrain[z][x];

      float x2 = (x + 1 - GRID_W / 2) * 10;
      float y2 = terrain[z][x + 1];

      float x3 = (x - GRID_W / 2) * 10;
      float y3 = terrain[z - 1][x];

      float x4 = (x + 1 - GRID_W / 2) * 10;
      float y4 = terrain[z - 1][x + 1];

      // Proyectar a 2D
      int sx1, sy1, sx2, sy2, sx3, sy3, sx4, sy4;

      if (project3D(x1, y1, z1 * 10, &sx1, &sy1) &&
          project3D(x2, y2, z2 * 10, &sx2, &sy2) &&
          project3D(x3, y3, z3 * 10, &sx3, &sy3) &&
          project3D(x4, y4, z4 * 10, &sx4, &sy4)) {

        // Color basado en altura y distancia
        float avgHeight = (y1 + y2 + y3 + y4) / 4.0;
        uint8_t baseId;

        if (avgHeight > 15) {
          baseId = IDX_PEAK;    // Magenta para picos altos
        } else if (avgHeight > 5) {
          baseId = IDX_GRID;    // Cyan para altura media
        } else {
          baseId = IDX_VALLEY;  // Cyan oscuro para valles
        }

        // Atenuar por distancia (solo a partir de 70% de profundidad,
        // igual que el original)
        float distFactor = (float)z / GRID_D;
        float dim = (distFactor > 0.7) ? distFactor : 0.0;
        uint8_t color = terrainColorIndex(baseId, dim);

        // Dibujar líneas del wireframe
        display.drawLine(sx1, sy1, sx2, sy2, color);  // Horizontal
        display.drawLine(sx1, sy1, sx3, sy3, color);  // Vertical

        // Dibujar diagonales cada 4 cuadros para detalle
        if (x % 4 == 0 && z % 4 == 0) {
          float diagDim = max(dim, 0.5f);
          display.drawLine(sx1, sy1, sx4, sy4, terrainColorIndex(baseId, diagDim));
        }
      }
    }
  }

  // Dibujar líneas verticales de grid en el horizonte
  for (int x = 0; x < GRID_W; x += 4) {
    int sx1, sy1, sx2, sy2;
    float worldX = (x - GRID_W / 2) * 10;

    if (project3D(worldX, 0, GRID_D * 10, &sx1, &sy1) &&
        project3D(worldX, -50, GRID_D * 10, &sx2, &sy2)) {
      display.drawLine(sx1, sy1, sx2, sy2, COL_HORIZON_GRID);
    }
  }
}

bool project3D(float x, float y, float z, int* sx, int* sy) {
  // Transformar al espacio de cámara
  float px = x - camX;
  float py = y - camY;
  float pz = z - camZ;

  // Verificar que está delante de la cámara
  if (pz <= 0) return false;

  // Proyección en perspectiva
  float scale = horizon / pz;

  *sx = display.width() / 2 + (int)(px * scale);
  *sy = display.height() / 2 - (int)(py * scale);

  // Verificar que está en pantalla
  if (*sx < 0 || *sx >= display.width() ||
      *sy < 0 || *sy >= display.height()) {
    return false;
  }

  return true;
}
