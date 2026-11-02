# Evaluación de visualización de oscilador

Decisión provisional: adoptar el patrón de FigBug/Gin para una **vista de modelo**
actualizada con parámetros de voz, usando la geometría propia de Prophec-Y; añadir
una captura de muestras en vivo separada si se quiere representar hard sync y
modulación a tasa de audio. No copiar ciegamente el intercambio entre hilos.

## Referencias fijadas y licencia

- WaveForge `f7aca307549737fa8d5fa65af0a59e4875e7430a`, MIT. `waveforge_exact_test.cpp`
  incorpora su método `WavetableOscillator::getNextSample`; el resto es harness.
- Simple Oscilloscope `2fcdbb12dd477875ad16aff6604c9d563603760c`, MIT.
  `RingBuffer_upstream.h` es el archivo original; `ring_test.cpp` es harness.
- FigBug/Wavetable `1a3df7e2f2cb35c991dcac7639557c22c73d1c97` usa
  FigBug/Gin `77d87a7a3c27847d9d3e396720ff4628a4849ab5`, BSD-3-Clause.
  `figbug_setparams_test.cpp` incorpora el método upstream `setParams`, con
  stubs para JUCE y el resto del componente. Licencias completas en este directorio.
  FigBug indica que las wavetables tienen licencias propias.

## Pruebas ejecutadas

```sh
c++ -std=c++17 -O2 waveforge_exact_test.cpp -o /tmp/waveforge-test && /tmp/waveforge-test
c++ -std=c++17 -O2 figbug_setparams_test.cpp -o /tmp/gin-test && /tmp/gin-test
c++ -std=c++17 -O3 ring_test.cpp -o /tmp/ring-test && /tmp/ring-test
```

WaveForge: dos tablas sintéticas opuestas y posición 0.5. Su DSP interpoló a
cero y la selección entera de tabla usada en el display escogió la primera;
el RMS de diferencia fue 0.707107. Es un contraejemplo a la equivalencia de
esas dos rutas, no una medición acústica de sus presets ni una evaluación de
la calidad general del sintetizador.

FigBug/Gin: se ejecutó el método upstream con stubs de igualdad y repaint.
Cambiar posición solicitó repaint, cambiar fold invalidó la geometría y un
valor sin cambio no hizo ninguna de las dos. La lectura de `OscillatorBox`
muestra un timer de 60 Hz con parámetros de una voz activa; el componente crea
un oscilador aparte para dibujar un ciclo de 64 muestras. Esto reconstruye una
forma a partir de parámetros, no prueba una captura de la salida real. El test
no ejecuta el render de Gin, ni demuestra corrección con hard sync o modulación
durante el ciclo. Hay que revisar su sincronización de acceso a la voz antes de
reutilizar el patrón en Prophec-Y.

Simple Oscilloscope: se compiló el RingBuffer original; la lectura de las 1024
últimas muestras tras envolver 16 bloques de 128 fue correcta. El microtiempo
local no mide carga en un DAW. El ring no proporciona sincronización por sí
solo. El plugin usa dos buffers y try-lock; si falla el intento, copia los
buffers completos en el hilo de audio. Evitar transportar esa solución sin
medir y verificar asignaciones, concurrencia y carga. La captura de muestras
reales no resuelve por sí sola el anclaje de fase ni la selección de voz.

Estas son pruebas unitarias y de código delimitadas, no comparaciones de tres
plugins completos, visuales o sonoras en DAW; no prueban el coste gráfico en el
PC del usuario. La vista del modelo y una futura traza real deben etiquetarse
por separado; inactividad, polifonía, publicación sin bloqueos y cierre del
editor necesitan pruebas específicas antes de integrar en producción.

### Comprobación adicional del build WaveForge

El árbol de WaveForge fijado arriba se configuró y compiló como aplicación
Standalone Linux usando la copia local de JUCE de Prophec-Y. La compilación
terminó correctamente. El intento de abrirla con Xvfb no pudo iniciar su servidor
X por falta de sockets locales en este entorno: no se hizo una ejecución ni
inspección visual del programa completo. FigBug y Simple Oscilloscope tampoco
se ejecutaron completos; las pruebas anteriores ejercitan únicamente sus piezas
indicadas. Esta limitación mantiene la elección como recomendación técnica
provisional y no acredita que un plugin haya ganado una prueba de usabilidad.
