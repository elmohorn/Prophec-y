# v0.19 — Comprobación final de filtros y envolventes

Fecha: 1 de noviembre de2026, Santiago. DSP de partida: `cf616015fb6569e5c8be3998fc74a469a42a5cfb`. No se modifica el DSP en esta fase.

## Decisión

**Congelar Default y conservar sus tiempos de envolvente.** Las referencias simples disponibles no identifican una ley física de cutoff Hz/raw. Una prueba final de100 renders, con cutoff fijo y amp/filtro separados, no justifica promover otra ley de tiempos. El extremo2,96kHz pertenece al perfil experimental Reference A v0.9; no se adopta como base del producto.

Esta es una decisión de calibración conservadora apoyada por pruebas, no una afirmación de haber recuperado la tabla física del Prophet-6. Los límites de evidencia se mantienen explícitos para la continuación posterior del sound matching.

## 1. Referencias simples antes de tocar las envolventes

Se verificaron hashes y decodificación completa de los tres archivos ya adquiridos de Bonedo: LPF38,591s, HPF25,443s y Selfosc14,105s. Además se adquirieron y verificaron ejemplos del mismo editor: Reverbs74,175s, Chorus23,943s, Distortion34,156s, Unison PWM Bass14,121s y Polymod Quasselpad28,781s.

La fuente original es la reseña de Lasse Eilers,24-08-2015. El autor identifica las clases de ejemplo. **No publica una trayectoria raw ni un SysEx/MIDI emparejado para estos ejemplos.** Su ejemplo de autooscilación se identifica como100% de keyboard tracking: no se deben atribuir sus notas al movimiento del cutoff. Ocho archivos de un editor no son ocho instrumentos independientes.

Mediciones de salida registradas en `calibration/v019_closeout/hardware_descriptors/summary.json`:

| Ejemplo | Percentiles5/50/95 del centroide40Hz–12kHz |
|---|---|
| LPF |128,66 /389,59 /2064,86Hz |
| HPF |372,19 /1168,34 /9716,36Hz |
| Selfosc |67,65 /226,26 /1039,38Hz |

Son descriptores del audio distribuido. **Centroide, rolloff y línea dominante no son cutoff.** La entrada oscilatoria, resonancia, tracking y ejecución desconocidos impiden convertirlos a Hz/raw. El predominio de una sola línea tampoco permite afirmar que sea F0 o frecuencia de autooscilación en todos los cuadros.

El análisis usa potencias L/R independientes, Fs48kHz, FFT4096, hop480, banda40–12000Hz y actividad sobre−40dB del máximo relativo. Las pruebas sintéticas comprueban que cambiar a estéreo en contrafase no cancela el espectro, que un tono de1kHz se recupera dentro de1Hz y que el silencio no genera una medición ficticia. Los metadatos MP3 emitieron avisos de comentarios/BOM en algunos archivos; el audio pasó la decodificación completa con comprobación de errores.

**Conclusión de identificabilidad:** no hay pares suficientes `(raw, frecuencia)` con modulación neutral conocida para ajustar intercepto, pendiente o curvatura. Para cualquier ley monótona alternativa puede proponerse otra trayectoria de knob desconocida; ajustar la frecuencia observada sin conocer esa trayectoria no discrimina las leyes. No se inventa un sweep lineal en el tiempo.

Por ello se congela la ley estática Default que ya venía usándose: base8,1757989156Hz, pendiente1semitono/raw, HP heredado y resonancia sin reescala. La profundidad de filtro sigue10,5833oct; no se cambia para absorber el error de cutoff. Las frecuencias finales conservan su límite de seguridad posterior a la modulación. Esto satisface la secuencia de trabajo acordada: evaluar primero la evidencia estática y fijar una base antes de probar las envolventes.

## 2. Envolventes con el cutoff fijo

Se recuperaron los cinco extractos previos y sus hashes desde los paquetes de escucha existentes. Misma ejecución inferida, Fs48kHz, oversampling2x, cuatro pre-rolls compartidos0/0,5/1,5/3s. Las fuentes y MIDI siguen sujetos a las limitaciones originales. No se recupera el MP3 completo ni se reetiquetan estos casos como holdout ciego.

Se registraron los perfiles antes de los nuevos scores:

- `current`: Default, amp/filtro Tmax10s, exponente3,5.
- `filter_exp3_max10`: sólo exponente de tiempos del filtro3, Tmax10s.
- `filter_exp3_max2p5`: sólo tiempos del filtro, exponente3 y Tmax2,5s.
- `amp_exp3_max10`: sólo exponente de tiempos amp3, Tmax10s.
- `amp_exp3p5_max30`: sólo Tmax amp30s, exponente3,5.

No se cambia la trayectoria temporal interna exponencial de los ADSR, la ley estática de filtros, resonancia, profundidad, PM, shape ni reset. Total:5perfiles×5extractos×4pre-rolls=**100renders**.

Criterio de screening previamente fijado: no empeorar espectro MR, perfil de bandas ni envolvente en ninguna pareja de los tres anchors principales, además de necesitar evidencia independiente antes de cambiar defaults. Es un filtro conservador de promoción, no una prueba estadística de equivalencia perceptual; diferencias diminutas no se interpretan como fallos audibles. Sync Stack/PolyModster son diagnósticos, no controles aislados de envolvente.

Mediana MR, dB, menor es mejor:

| Perfil | Brassed Off | Solid Bass | Carpenter Square |
|---|---:|---:|---:|
| Default |6,2618|13,9200|11,1563|
| Filtro exp3,10s |6,2537|13,9421|10,8541|
| Filtro exp3,2,5s |6,8950|13,9243|12,2675|
| Amp exp3,10s |6,2864|13,9199|11,1888|
| Amp exp3,5,30s |6,5493|13,9198|11,3528|

La primera variante mejora el MR de Carpenter Square, pero su envelope MAE sube de0,1575 a0,1586. En Solid Bass sube de0,1524 a0,1644, además de empeorar MR. La variante corta degrada las bandas en las12parejas principales. Alargar amp a30s perjudica claramente el contorno de Brassed Off y Carpenter Square; las mejoras ínfimas en Solid Bass no justifican una promoción global.

Número de empeoramientos numéricos sobre12parejas principales:

| Perfil | MR | Bandas | Envelope MAE |
|---|---:|---:|---:|
| Filtro exp3,10s |4|0|8|
| Filtro exp3,2,5s |11|12|6|
| Amp exp3,10s |8|10|7|
| Amp exp3,5,30s |8|10|7|

**Resultado:** ninguna alternativa pasa. Se mantienen por separado los tiempos actuales de amp y filtro; no se resuelve una inconsistencia de507 modificando todo el instrumento.

## 3. Regresión y cobertura

Se compararon los20resultados Default contra el perfil `depth10.5833_res1` de la rejilla posterior a la corrección Unison. En MR, bandas, envelope MAE y RMS candidato, la diferencia máxima es exactamente0 a la precisión de los CSV. Esto comprueba reproducción de esas mediciones; no se presenta como una comparación byte a byte de los WAV antiguos.

Las nuevas fuentes de PWM/Poly Mod/distorsión/FX amplían la cobertura observable y cuentan con descriptores y hashes. Al desconocerse controles, entrada y gates, no se hacen renders arbitrarios para declarar coincidencia acústica de efectos. Esas leyes siguen provisionales. Los ocho presets reservados siguen sin test; no se sustituyen a posteriori por estos ejemplos.

Los archivos `frozen_defaults.json`, `frozen_envelope_profiles.json`, métricas, protocolo y decisión quedan versionados. Los audios de terceros y los WAV sintetizados permanecen fuera del repositorio.

## 4. Rendimiento

Se ejecutó el benchmark Release existente en nueve escenarios. Se conserva un piloto concurrente con los renders y dos pasadas posteriores secuenciales sin esos workers. No se elige la mejor pasada ni se promete rendimiento del PC del usuario.

A48kHz/2x/seis voces/FX, las dos pasadas reportan21,312% y21,095% de un core de este ejecutor compartido; a48kHz/4x/seis voces/FX,45,549% y49,102%. Hay picos que exceden el presupuesto de bloque, especialmente en configuraciones más costosas: este ambiente no prueba ausencia de dropouts ni una capacidad de tiempo real garantizada. No hay una línea base equivalente de hardware dedicado que permita declarar una mejora de CPU.

Se mantiene2x como opción inicial; no se añade oversampling adaptativo ni una optimización aproximada por obligación de cierre. Las huellas DSP quedan registradas. La matriz de carga/DAW/soak en equipos reales continúa en0.21.

## Reproducir

```sh
python3 tools/prepare_basic_waveform_suite.py
python3 tools/analyze_closeout_evidence.py calibration/v019_closeout/hardware_manifest.json --output /tmp/closeout-hardware
python3 tools/compare_online_suite.py calibration/recovered_reference_suite_v019.csv --renderer build-check/p6_render_events_cal --profiles-file calibration/v019_closeout/frozen_envelope_profiles.json --profiles current,filter_exp3_max10,filter_exp3_max2p5,amp_exp3_max10,amp_exp3p5_max30 --output /tmp/closeout-envelopes --source-sha cf616015fb6569e5c8be3998fc74a469a42a5cfb
```

La primera orden recupera los cinco archivos antiguos; los cinco nuevos se recuperan desde sus URLs del manifest y se aceptan sólo con el SHA256 publicado y decodificación completa. La segunda orden requiere esos ocho archivos privados. La tercera requiere los cinco extractos anteriores y los eventos/presets preparados por `prepare_recovered_online_suite.py`. Las opciones por defecto del comparador coinciden con los cuatro pre-rolls y2x registrados.

Fuente editorial: https://www.bonedo.de/artikel/dave-smith-instruments-sequential-prophet-6-test/
