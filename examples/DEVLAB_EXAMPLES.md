# Ejemplos DevLab para upicodvi

Colección de ejemplos optimizados para la configuración de hardware DevLab.

## ⚠️ IMPORTANTE: Configuración Verificada

**Todos los ejemplos usan esta configuración que ha sido probada y funciona correctamente:**

```cpp
static const struct dvi_serialiser_cfg devlab_dvi_cfg = {
  .pio = pio0,
  .sm_tmds = {0, 1, 2},
  .pins_tmds = {14, 12, 8},     // ¡IMPORTANTE! Este es el orden correcto
  .pins_clk = 10,
  .invert_diffpairs = false     // false para DevLab
};
```

### Resolución Recomendada
**DVI_RES_320x240p60** - Es la resolución probada que funciona establemente con DevLab.

### Mapeo de Pines DVI

| GPIO | Función      | Descripción                |
|------|--------------|----------------------------|
| 14   | TMDS0+       | Par de datos 0 (positivo)  |
| 15   | TMDS0-       | Par de datos 0 (negativo)  |
| 12   | TMDS1+       | Par de datos 1 (positivo)  |
| 13   | TMDS1-       | Par de datos 1 (negativo)  |
| 8    | TMDS2+       | Par de datos 2 (positivo)  |
| 9    | TMDS2-       | Par de datos 2 (negativo)  |
| 10   | CLK+         | Par de reloj (positivo)    |
| 11   | CLK-         | Par de reloj (negativo)    |

**Nota**: Este orden específico `{14, 12, 8}` es crítico para que los colores se vean correctamente.

## Ejemplos Disponibles

### 1. devlab_16bit_graphics
**Color completo RGB565 - 65,536 colores**

Características:
- Resolución: 320x240 @ 60Hz
- Profundidad de color: 16-bit (RGB565)
- Buffer: Simple (sin doble buffer)
- RAM requerida: ~150 KB

Demuestra:
- Gradientes de color suave
- Círculos concéntricos animados
- Conversión HSV a RGB
- Texto con sombras

### 2. devlab_text_mode
**Terminal de texto de alta velocidad**

Características:
- Resolución: 640x480 @ 60Hz
- Modo: 1-bit (monocromo)
- Caracteres: ~80x30
- RAM requerida: ~40 KB

Demuestra:
- Terminal estilo retro
- Scroll automático
- Actualización en tiempo real
- Formato personalizado de colores

### 3. devlab_monochrome
**Gráficos monocromáticos de alta resolución**

Características:
- Resolución: 640x480 @ 60Hz
- Modo: 1-bit con doble buffer
- RAM requerida: ~80 KB

Demuestra:
- Campo estelar 3D (starfield)
- Animación fluida sin parpadeo
- Cálculo de FPS
- Efectos de profundidad

### 4. devlab_plasma
**Efecto plasma con animación**

Características:
- Resolución: 320x240 @ 60Hz
- Modo: 8-bit (256 colores)
- Buffer: Doble buffer
- RAM requerida: ~80 KB

Demuestra:
- Paleta de colores personalizada
- Algoritmo de plasma
- Efectos matemáticos
- Animación de múltiples capas

### 5. devlab_menu
**Sistema de menú interactivo**

Características:
- Resolución: 320x240 @ 60Hz
- Modo: 8-bit (colores personalizados)
- Buffer: Doble buffer
- RAM requerida: ~80 KB

Demuestra:
- Interfaz de usuario gráfica
- Navegación de menú
- Efectos visuales (sombras, destacados)
- Barra de estado con reloj

## Comparación de Modos

| Ejemplo         | Resolución | Colores | Buffer | RAM    | FPS | Dificultad |
|-----------------|------------|---------|--------|--------|-----|------------|
| 16bit_graphics  | 320x240    | 65K     | Simple | ~150KB | 30+ | Media      |
| text_mode       | 640x480    | 2       | Simple | ~40KB  | 60  | Fácil      |
| monochrome      | 640x480    | 2       | Doble  | ~80KB  | 60  | Media      |
| plasma          | 320x240    | 256     | Doble  | ~80KB  | 60  | Media      |
| menu            | 320x240    | 256     | Doble  | ~80KB  | 60  | Avanzada   |

## Uso

1. Selecciona el ejemplo que deseas probar
2. Abre el archivo `.ino` en Arduino IDE
3. Verifica que la configuración `devlab_dvi_cfg` coincida con tu hardware
4. Compila y carga en tu RP2040
5. Conecta un monitor HDMI/DVI

## Resoluciones Disponibles

```cpp
DVI_RES_320x240p60   // Recomendado para principiantes
DVI_RES_400x240p60   // Más ancho, requiere más overclock
DVI_RES_640x480p60   // VGA estándar, alta resolución
DVI_RES_800x480p60   // Pantalla ancha, requiere máximo overclock
```

## Consideraciones de RAM

El RP2040 tiene aproximadamente 264 KB de RAM total. La cantidad disponible para framebuffers depende de:

- **16-bit (DVIGFX16)**: width × height × 2 bytes
  - 320x240 = 153,600 bytes (~150 KB)
  - Sin espacio para doble buffer

- **8-bit (DVIGFX8)**: width × height × 1 byte + paleta
  - 320x240 = 76,800 bytes (~75 KB)
  - Doble buffer = ~150 KB

- **1-bit (DVIGFX1)**: (width × height) / 8 bytes
  - 640x480 = 38,400 bytes (~38 KB)
  - Doble buffer = ~76 KB

## Optimización

Para mejorar el rendimiento:

1. **Tools → Optimize** → Selecciona `-O3` o `-Ofast`
2. **Tools → CPU Speed** → Usa 125 MHz o superior
3. **Reduce resolución** si experimentas problemas
4. **Desactiva doble buffer** si necesitas más RAM

## Troubleshooting

**LED parpadeando**: RAM insuficiente
- Reduce la resolución
- Desactiva doble buffer
- Cambia de 16-bit a 8-bit o 1-bit

**Solo se ve la mitad de la pantalla**: 
- Verifica que uses exactamente: `pins_tmds = {14, 12, 8}`
- Asegúrate que `invert_diffpairs = false`
- Algunos ejemplos complejos pueden necesitar más optimización

**Sin imagen**: 
- Verifica las conexiones de los pines
- Asegúrate que los pares TMDS estén correctos
- Revisa que el monitor soporte 320x240@60Hz

**Colores incorrectos**:
- Usa el ejemplo `devlab_color_test` para verificar
- La configuración correcta es: `pins_tmds = {14, 12, 8}`
- NO cambies el orden sin probar con `devlab_color_test`

## Ejemplos portados desde software/apps/

Los siguientes ejemplos reimplementan demos de `software/apps/` (las apps
originales en C puro sobre el SDK de Pico) usando la libreria Arduino
(`DVIGFX8`/`DVIGFX16`/`DVIGFX1`), con la configuracion de pines DevLab.
Ninguno usa `libsprite`/`libtile` (no estan incluidas en esta libreria
Arduino) ni el streaming crudo por DMA que usan `vista`/`vista-palette`
originales; cada `.ino` documenta en su cabecera que se cambio y por que.

| Ejemplo                     | Original en software/apps/ | Notas |
|------------------------------|-----------------------------|-------|
| devlab_mandelbrot             | mandelbrot, mandel-full     | Fractal recalculado cada frame (sin generador incremental con deteccion de ciclos) |
| devlab_moon                   | moon                        | Misma imagen 1bpp 640x480, dibujada con `drawBitmap()` |
| devlab_sprite_bounce          | sprite_bounce                | Sprites con transparencia via `drawPixel()` + mascara 1-bit (sin rotacion afin) |
| devlab_tiles                  | tiles                        | Mapa Zelda con auto-scroll diagonal, blit de tiles a mano |
| devlab_tiles_and_sprites      | tiles_and_sprites            | Mapa + 12 personajes animados (el original usa 100). Usa `DVIGFX8` con doble buffer real (`swap()`) en vez de `DVIGFX16`, para evitar el parpadeo que causa dibujar sobre un framebuffer sin doble buffer |
| devlab_tiles_parallax         | tiles_parallax                | Dos capas de tilemap con scroll a distinta velocidad |
| devlab_vista_slideshow        | vista, vista-palette          | Carrusel de fotos reales a 320x240 embebidas en el sketch. **No** es un port 1:1: el original transmite imagenes por DMA crudo desde un blob de flash externo (`.uf2` aparte, offset fijo, boot_stage2 a medida) — nada de eso es posible desde el flujo de subida de Arduino |

**No portado: `bad_apple`.** Requiere un video fuente (`src.mkv`), un
pipeline de `ffmpeg`/PIL para generarlo, y un decodificador RLE en
ensamblador ARM que escribe simbolos TMDS directamente (bypaseando el
framebuffer). No hay forma honesta de reproducirlo sin esos insumos.

## Referencias

- Biblioteca: upicodvi (fork de PicoDVI)
- Gráficos: Adafruit_GFX compatible
- Hardware: RP2040
- Autor original: Luke Wren (PicoDVI)
