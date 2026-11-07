# v0.20 — Auditoría técnica y plan de validación: Tube + Optical Tremolo

**Fecha:** 5 de noviembre de 2026  
**Rama objetivo:** `v0.20-ui`  
**Estado:** investigación y propuesta; **sin implementación ni pruebas nuevas de audio en este hito**.  
**Decisión de producto:** mantener Tube como primer Character, desarrollar Optical Tremolo como próximo candidato y perfeccionar Optical Spring existente. Plasma, Ferrofluid y Neon no se eliminan, pero no son prioridad de refinamiento.

## Principios compartidos

1. El efecto debe ser **musicalmente convincente y controlable**, no únicamente complejo en términos matemáticos ni vistoso.
2. Su ventana debe representar **un estado real del DSP o de su excitación**, diferenciando animación ilustrativa de medición física; la UI jamás debe cambiar el audio.
3. Validación técnica (THD, espectro, ganancia, CPU, estado y latencia) **más** escucha musical a niveles pareados. Ninguna sustituye a la otra.
4. Preservar el contrato **Prophet SysEx/NRPN**, los IDs del host existentes, la migración de sesiones y la separación entre efectos Character y FX A/B del Prophet.
5. La bahía actual admite **un solo Character activo** y usa **71 muestras de latencia global fija**: no prometer simultaneidad entre Tube, Tremolo y Spring sin diseñar otra cadena de slots.

## A. Tube — auditoría del DSPark TubePreamp actualmente integrado

### Evidencia en el código

- Fuente: [`plugin/CharacterFx.cpp`](../plugin/CharacterFx.cpp). El adaptador usa `dspark::TubePreamp<float>`, fija `setStages(1)` y `setOversampling(2)` y prepara con `prepare({sr, 1, 2})`. La función `process()` llama a `tube.processBlock(..., 2, 1)` **por muestra estéreo**.
- El motor modela triodo/12AX7 con una formulación Koren, componentes dinámicos de cátodo y sag y ecualización tipo Bassman; es más que un `tanh` sencillo, pero no una reconstrucción íntegra de un preamplificador físico.
- Ganancia: el mapeo real es `setOutput(-24 + 36*v[6])`. Con `v[6]=0.5`, el control está en **−6 dB nominales**, no 0 dB. Una prueba anterior de un único fragmento documentó **−6,16 dB RMS respecto de dry**; no extrapolar a cualquier señal.
- El wrapper declara 71 samples de PDC aun con Character en None/bypass. La operación húmeda, la referencia seca y los cambios de ruta tienen política de alineación/rampas.
- La comparación previa de **315 renders** de cinco candidatos seleccionó DSPark como primer prototipo técnico. Su ventaja espectral no fue medida con distorsión equivalente ni constituye una escucha ciega; el costo de CPU citado procedía de un ejecutor compartido. Ver [comparación previa](V0_20_FX_COMPARISON.md) y [prototipo](V0_20_TUBE_PROTOTYPE.md).

### Riesgos y decisiones a comprobar

**T1 — Integración por muestras frente a bloques (alta prioridad).** La entrada del módulo es sample-by-sample, y la preparación de Tube fija bloque máximo 1. Un adaptador por bloques exige cambiar ambos aspectos y posiblemente introducir un punto de procesamiento por bloque aguas arriba, manteniendo el orden temporal de los FX y la automatización. **No asumir** que la ganancia de rendimiento será enorme ni que una sustitución directa conserva el sonido: el propio kernel fue costoso incluso en benchmarks por bloques. Crear un prototipo separado, comparar muestras/estado/CPU y conservar como baseline la implementación actual.

**T2 — Una o dos etapas (exploración, no decisión).** Preparar una variante de dos etapas de DSPark con los mismos presets de entrada. Medir las diferencias de armónicos, compresión/transitorios, calidad musical, tiempo de preparación, CPU y latencia. Si cambia el PDC, resolver la compatibilidad de 71 muestras **antes** de integrarla; no introducir un cambio audible silencioso en sesiones existentes.

**T3 — Ganancia y percepción.** Probar referencias dry/Tube con volumen **perceptual** pareado además de RMS, documentando la atenuación actual. Valorar un control explícito de compensación o un nuevo preset Init, pero **sin corregir retroactivamente** los niveles de proyectos anteriores. La ganancia distinta no determina calidad.

**T4 — Dinámica física y rango de controles.** Caracterizar Sag, Drive, Bass/Middle/Treble a distintas intensidades y duraciones. Revisar si el rango de los controles es musical y si el modelo de una etapa proporciona el carácter cálido/agresivo buscado. Grid conduction, blocking distortion y carga entre etapas son aproximaciones: estudiar mayor fidelidad sólo si la comparación auditiva lo justifica.

**T5 — Telemetría visual.** El brillo actual representa excitación/cambio de señal, **no temperatura real**. Conservar esa claridad: integrar suavizados sin locks del hilo audio, UI limitada (p.ej., 30 fps), movimiento reducido y posibilidad de apagar animación sin afectar el DSP.

### Matriz mínima Tube

- Muestras: 44,1/48/96 kHz; bloques 32/64/128 y control de extremos 16/1024; mono/estéreo, 1–6 voces, Unison.
- Señales: silencio, impulso, seno a distintos niveles, dos tonos/intermodulación, seno alto cercano a Nyquist, acordes, bajos, leads agresivos y pads.
- Métricas: THD vs Drive/nivel; espectro armónico, alias en referencia de mayor tasa, DC, transitorios y comportamiento Sag; pico/RMS/LUFS cuando corresponda; latencia/PDC; CPU media, p99 y máximos en build Release + DAW.
- Interacciones: automatización de todos los controles, Mix=0, bypass, None, Start/End, reset, reapertura/restore de sesión y preservación SysEx.
- **Criterio de elección:** mantener DSPark, optimizar integración, probar dos etapas o sustituir **únicamente** tras resultados registrados. No es una carrera de complejidad física.

## B. Optical Tremolo — selección y adaptación técnica

### Candidatos abiertos revisados

| Candidato | Qué ofrece | Evaluación |
|---|---|---|
| [Patina — TremoloVCA](https://github.com/ShmKnd/Patina/blob/main/dsp/circuits/compander/TremoloVCA.h) | C++ MIT; estado LDR con ataque/liberación asimétricos; variantes Bias/Optical/VCA. | Buena **referencia inicial** para trémolo óptico; necesita corregir ganancia, optimizar y probar. No es automáticamente un efecto terminado. |
| [DSPark — Tremolo](https://github.com/CristianMoresi/DSPark/blob/main/Effects/Tremolo.h) | C++ MIT; LFO seno/triángulo/cuadrada suavizada, estéreo y crossfades de cambio de forma. | Baseline robusta y eficiente; **no** modela internamente una LDR con ataque/liberación asimétricos. |
| [Open Riff Box — Tremolo](https://github.com/dlujic/open-riff-box/blob/main/src/dsp/Tremolo.cpp) | JUCE: Photo (comparador tipo neón + envolvente LDR), Bias y Harmonic; modo óptico con tiempos ~10/35 ms. | Referencia muy útil de comportamiento y organización; **verificar y respetar su licencia** antes de copiar código. |

La licencia permisiva facilita integrar Patina/DSPark con atribuciones correctas, pero no reemplaza la verificación de licencias transitivas. No se ha ejecutado todavía una comparación acústica de estos tres candidatos dentro de Prophec-y.

### Hallazgo de código especialmente relevante: Patina no es plug-and-play

En el modo Optical de Patina:

```cpp
target = 0.5 + lfo * depth * 0.5;
alpha  = 1 - exp(-1 / (sampleRate * tau));
state += alpha * (target - state);
gain   = 0.2 + 0.8 * state * state;
```

Al fijar `depth=0`, `target=0.5`; con el estado inicial 0,5, **la ganancia de la ruta óptica es 0,4 (~−7,96 dB), no 1**. Se trata de una atenuación intrínseca: `Depth=0` detiene la **modulación**, pero no devuelve ganancia unitaria. Se debe decidir explícitamente la política de compensación —ganancia base unitaria y/o trim separado— y validar el recorrido de Mix. No corregir el problema con normalización indiscriminada: cualquier opción debe probar ruido, clipping, rango de profundidad y respuesta de salida.

El código calcula `exp` **en el procesamiento de cada muestra**; si los tiempos se mantienen constantes pueden precalcularse al preparar/cambiar parámetros (no en audio callback), conservando la fórmula. La ley de LDR (`state²`) y los tiempos ~1 ms ataque / 10 ms liberación son aproximaciones creativas, **no una validación de un optoacoplador físico**. Confirmar si suena bien sobre un sintetizador, no sólo sobre guitarra.

### Diseño experimental recomendado

**O1 — Baseline simple.** Construir un prototipo aislado sobre DSPark Tremolo (LFO + control de profundidad), sin afirmar comportamiento físico óptico. Debe pasar ganancia unitaria en Depth=0, fase continua, bypass sin clic y controles suaves. Considerar la opción de sincronía de tempo y tasa libre.

**O2 — Candidato óptico.** Adaptar la **dinámica** LDR de Patina, no su ruta de salida directamente: precalcular coeficientes de ataque/liberación, modelar respuesta asimétrica y mapear LFO a iluminación/conductancia y después a ganancia musicalmente calibrada. Usar la formulación de Patina con atribución si se incorpora código; considerar código propio si resulta más limpio. Elegir cuidadosamente piso de ganancia (0–1) y forma de onda, sin ocultar una atenuación por defecto.

**O3 — Tercer comportamiento.** Usar Open Riff Box como **referencia** para un drive tipo lámpara/neón con transición de encendido y apagado seguida de LDR. Contrastar respuesta cuadrada suavizada frente a sinusoidal: un desarrollo estéticamente más rico sólo se adopta si suena distinto y útil. No copiar código de licencia incompatible.

**O4 — Estéreo y control musical.** Asegurar un LFO con fase y velocidad definidas por **muestra**, no actualizado de forma accidental una vez por canal; modo mono, inversión de fase estéreo y profundidad reproducible. Probar Rate 0,1–12 Hz y cambios de tempo, y medir transitorios/clics al cambiar forma, Depth, Mix, bypass y ruta. Mantener superficie de controles legible (Rate, Depth, Response, Stereo, Mix, Output y opcional Shape/Sync, según contrato acordado).

**O5 — Visualización reactiva y honesta.** La **lámpara** sigue el estado de excitación/iluminación del LFO; la **fotorresistencia** y, si corresponde, una aguja de atenuación siguen el estado LDR/ganancia con rezago temporal. Una envolvente de audio puede aportar brillo secundario, sin falsear el ciclo óptico. Publicar resúmenes limitados desde el hilo de audio y dibujar en JUCE fuera de él. No crear simulación física 3D ni FFT sólo decorativa.

**O6 — Contrato de estado y latencia.** El catálogo hoy es None/Tube/Plasma/Optical Spring/Ferrofluid/Neon. Para Optical Tremolo: **añadir un nuevo ID**, no reordenar ni reutilizar IDs de Character ni de sus controles; persistir su propio banco y migrar estados históricos v1–v5. Conservar SysEx original y el PDC global fijo 71 samples, alineando dry/wet aunque el LFO en sí sea de latencia cero. No prometer dos Character simultáneos sin un rediseño explícito de slots.

### Matriz mínima Optical Tremolo

- **Pruebas funcionales:** Depth=0 sin atenuación no solicitada, Mix=0 igual dry compensado, Rate=0/0,1/1/6/12 Hz, Sweep de profundidad, cambio de forma, bypass, Start/End, suspend/resume, retrigger/reopen/restore.
- **Tiempo y sonoridad:** curva real de ataque/liberación LDR, pulsos de lámpara rápidos, bajo sostenido, pad, pluck y secuencia; fase estéreo, correlación y compatibilidad mono; cambios de ganancia sin jumps.
- **Ingeniería:** estabilidad a 44,1/48/96/192 kHz, bloques 16–1024, finitud/NaN, determinismo respecto del tamaño de bloque, CPU p99 y realtime sin asignaciones/locks, pruebas de automatización y sesiones DAW.
- **Comparación musical:** tres variantes con nivel de salida pareado (no comparar directamente la ley actual de Patina con una baseline unitaria) y escucha ciega. Elegir la más útil, no la de más knobs ni mayor atenuación.

### Decisión provisional

**No copiar Patina sin adaptar.** Prototipar **DSPark Tremolo** como baseline y **Patina LDR corregido/calibrado** como candidato principal; considerar arquitectura de comparador/neón de Open Riff Box como tercer comportamiento de referencia. El objetivo es un trémolo vintage agradable, dinámico y fácil de tocar; **no** demostrar una emulación exacta de un circuito determinado.

## C. Optical Spring Reverb — encaje, sin sustituir esta auditoría

El prototipo actual **ya está implementado**, basado en dispersión/allpass, FDN y tres captaciones internas; la visualización utiliza energía de taps reales pero no desplazamiento mecánico medido. Su trabajo pendiente será un **hito propio** de fidelidad de cola, dispersión, transitorios, estéreo, nivel, CPU y representación de los resortes. No confundirlo con la reverb Spring estándar de FX B. [Estado Character](V0_20_CUSTOM_CHARACTER.md).

## Entregables y criterios de aceptación

1. **Reporte comparativo Tube** con commits/fixtures/render original y pareado, matrices acústicas, medición en host y decisión sobre una/dos etapas y arquitectura por bloques.
2. **Harness trémolo** que compile candidatos aislados, exporte curvas LFO/LDR, gain y audio, documente licencias y permita A/B sobre señales fijas.
3. **Selección musical Optical Tremolo**, aprobada por escucha y QA, antes de exponerlo como efecto terminado. La visualización debe seguir al DSP elegido.
4. **Actualización de la integración** únicamente después de fijar IDs, persistencia, PDC, tests de migración y compatibilidad con sesiones anteriores.

**No realizado todavía:** benchmark nuevo, medición del trémolo dentro de Prophec-y, comparación ciega, modificación de DSP/UI, compilación, ni validación de hardware/DAW por esta actualización documental.
