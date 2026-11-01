# Comparación ejecutable de núcleos para la válvula Prophec-y

Actualización posterior: el primer prototipo nativo está descrito en `V0_20_TUBE_PROTOTYPE.md`; incluye la política de latencia fija y las limitaciones actuales. El texto siguiente conserva el alcance de esta propuesta/evaluación.

1de noviembre de2026. Evaluación técnica de cinco núcleos seleccionados de los ocho repositorios inventariados. **Recomendación: DSPark TubePreamp para el primer prototipo de válvula; DaisySP como caja de bloques para el resto de la colección.** Airwindows Tube2 queda como alternativa de costo reducido. La elección musical final requiere escuchar los A/B; aquí no se emite una preferencia auditiva ficticia.

## Qué se probó realmente

Se compilaron los cuerpos/headers originales fijados, con adaptadores de API, en un ejecutable separado del plugin:

- Airwindows Tube2: cuerpo original float, clase mínima sin SDK VST2; pad de entrada1 y semillas de dither fijas.
- DSPark TubePreamp:1etapa, sag0,3, mezcla100%, salida0dB, oversampling interno2x; drive de−12 a+36dB.
- Patina TubePreamp: salida1, bias0,5, ruido y grid conduction nativos.
- DaisySP Overdrive: una instancia por canal. Es un overdrive genérico, no un modelo dedicado de válvula.
-0xFX Valve Color: función original de waveshaping y DC block, estado separado por canal, bias0,5 y mezcla100%.

Matriz:5núcleos×3sample rates(44,1/48/96kHz)×3diales nativos(0,2/0,5/0,8)×7señales=**315renders**. Señales: silencio, seno1kHz a−18/−6dBFS de amplitud, seno cercano a15kHz, impulso, canal izquierdo aislado y acorde sintético. Se usan32768frames, bloques128 y0,25s de precalentamiento. Las frecuencias se ajustan a bins impares coherentes.

Se añadieron cinco parejas de block-size16/1024, cinco automatizaciones y cinco versiones musicales de un render original Prophec-y. El acorde musical usa un clon privado del programa000 sin FX/distorsión, notas48/52/55, velocity0,8 y2s de gate. No es una grabación de Prophet físico.

**Igual posición de Drive no significa igual ganancia ni distorsión.** En dial0,5,1kHz/−18dBFS de amplitud, el THD medido es0,56% Tube2,3,32% DSPark,9,66% Patina,25,86% Daisy y6,88%0xFX. Son puntos de caracterización, no un ranking de calidad. La interfaz propia tendrá que diseñar su recorrido de drive.

## Resultados de ingeniería

Todos los315renders fueron finitos. El contador no detectó llamadas ordinarias a C++ `new` durante proceso; no instrumenta todo malloc, asignaciones alineadas, locks ni todos los contratos realtime.

Dos pasadas secuenciales posteriores, sin workers de renders/sanitizers, procesaron estéreo48kHz,128frames, el mismo acorde y dial0,5. CPU de un core del ejecutor compartido:

| Núcleo | CPU pasada1 /2 | Latencia declarada en el adaptador | Ruido de silencio a48kHz/dial0,5 |
|---|---:|---:|---:|
| DSPark |8,21% /8,40% |71samples (1,48ms a48kHz) |≈−294dBFS numéricos |
| Tube2 |0,80% /0,39% |0samples |−143,4dBFS |
| Patina |1,20% /1,34% |0samples |−96,6dBFS |
| Daisy Overdrive |0,039% /0,026% |0samples |silencio exacto en float |
|0xFX Valve Color |0,188% /0,142% |0samples |silencio exacto en float |

No se afirma ausencia de fase/group delay al declarar0samples; es ausencia de una latencia de bloque/PDC reportada por este adaptador. La prueba no predice el costo del PC del usuario ni del plugin completo. Los kernels son de complejidad y procesamiento distintos; los cuatro últimos no recibieron oversampling externo. Los máximos de bloque de un entorno compartido no certifican tiempo real.

En la prueba cercana a15kHz,48kHz/dial0,5, el residuo no armónico dentro de40Hz–20kHz, relativo a la fundamental, fue≈−92,0dB DSPark,−27,8 Tube2,−12,4 Patina,−8,6 Daisy y−12,8dB0xFX. Este indicador también incluye ruido y estados transitorios: **no es una medición pura de aliasing ni una comparación a igual distorsión**. DSPark trae antialiasing interno; los otros necesitarían evaluar esa integración si se exige menor residuo.

Tube2, DSPark, Daisy y0xFX dieron igualdad exacta entre bloques16/1024 con entrada/estado inicial controlados. Patina difirió en un máximo9,31e−5 y RMS1,76e−5; su secuencia de ruido por canales/bloques explica que no sea una ruta determinista respecto del tamaño de bloque en esta configuración. No se etiqueta ese ruido intencional como corrupción de memoria.

DSPark requirió aproximadamente0,12–0,24s de preparación Release en la corrida final; los otros adaptadores fueron mucho más pequeños. Cualquier cambio de modelo en el carrusel debe preparar fuera de audio y realizar un intercambio seguro.

ASan+UBSan completaron la matriz y pruebas adicionales sin diagnósticos, con leak detection desactivado. La primera corrida con LeakSanitizer terminó el procesamiento pero su comprobación final falló porque el entorno ptrace impidió leer tareas de `/proc`. **No se declara una prueba de ausencia de leaks.** Tres pruebas unitarias verifican el medidor: THD coherente conocido, silencio/canal aislado y localización de impulso.

## Decisión por candidato

**DSPark:** primera opción técnica para la válvula. Ofrece comportamiento dinámico de circuito, antialiasing interno, rampas de controles y latencia consultable. El costo es mayor y debe presupuestarse en la cadena completa. No demuestra exactitud frente a hardware real; su tone stack/compensación y las aproximaciones del modelo deben escucharse con el sintetizador. La interfaz no ofrecerá un control de bias si el modelo elegido no lo expone.

**Airwindows Tube2:** alternativa ligera y con carácter. Aporta mucha menos latencia/costo en la configuración nativa. La variable Tube usa un power factor entero y el algoritmo no reemplaza por sí solo una política de antialiasing/automatización de nuestro producto. Necesita adaptación y nivelación; no es un solver de circuito.

**DaisySP:** prioridad como biblioteca de componentes. Su Overdrive es extremadamente barato y puede ser útil en plasma/distorsiones, pero su respuesta al drive es agresiva en la zona ensayada. No elegirlo como simulación de válvula sólo por su costo. PitchShifter, delay lines, tremolo y wavefolder se evaluarán por dispositivo; probar Overdrive no valida esos otros módulos. Separar el núcleo MIT del submódulo LGPL.

**Patina:** conservar para exploración, sin prioridad para la primera válvula. Ruido no opcional en la ruta usada, coeficientes discretos, observación previa de discontinuidad de transferencia y dependencia del orden de ruido exigen más revisión. No se descarta toda la biblioteca ni se certifican sus modelos de resorte/cinta a partir de este ensayo.

**0xFX:** útil para prototipos creativos y controles básicos. Valve Color es un waveshaper asimétrico simple con DC blocker; bajo costo, pero mayor residuo de alta frecuencia en esta prueba. Sus spring/shimmer/granular no fueron ensayados aquí y no se declaran listos para producción.

BYOD continúa como referencia por su licencia pública GPL y requiere revisión específica antes de adopción bajo nuestra política. chowdsp_wdf es infraestructura, no un efecto de válvula terminado; demandaría construir más circuito. DaisySP-LGPL no se compiló. No hay resultados de audio atribuidos a esos tres casos.

## Escucha y siguiente implementación

Se prepararon seis WAV24-bit/48kHz: fuente seca y cinco modelos, RMS−20dBFS con ganancia constante por archivo, y latencia DSPark recortada. No es normalización LUFS ni distorsión emparejada, y el dial nativo sigue0,5. `audition_levels.csv` deja visibles las diferencias de ganancia. La comparación evita premiar volumen, pero no permite concluir que todos trabajan con la misma saturación.

El primer prototipo propuesto usa DSPark detrás de un adaptador propio, con Drive, sag/tono sólo según la API realmente utilizada, Mix, Output, bypass, latencia compensada y telemetría de excitación para la válvula. Antes de integrarlo: escucha, análisis con niveles ajustados a un carácter musical comparable, CPU conjunto, cambios de parámetros y prueba de host. El motor Prophet existente no se modifica con este harness.

La propuesta del carrusel y las posiciones Inicio/Final quedan en `V0_20_CHARACTER_CAROUSEL.md`. El catálogo puede crecer sin acoplar las vistas a una sola biblioteca DSP.
