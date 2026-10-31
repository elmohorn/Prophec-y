# v0.19 — comparación de las nuevas grabaciones de hard sync

Se completaron **802 renders de diagnóstico**: 711 del barrido, 72 componentes de acordes, 18 del preset 007/507 y un render preliminar de afinación. Los resultados contienen 316 filas de comparación. No cambió el DSP de producción, el mapper ni los presets.

## Qué permite concluir esta tanda

La sierra del motor vuelve a coincidir bien con las referencias. La diferencia del pulso casi desaparece usando un ancho fijo ligeramente distinto de 50%, disponible en el motor actual. En cambio, el barrido de sync mantiene diferencias que no se resuelven con un cambio global de fase de reinicio. Los ejemplos del 507 dependen fuertemente de la edad de nota supuesta; todavía no permiten elegir una corrección de reset o envolvente.

## Controles de la misma sesión: son acordes

Los archivos Waveform Saw y Waveform Pulse contienen tríadas aproximadamente C3–Eb3–Gb3 y D3–F3–Ab3, no notas aisladas. Se reconstruyeron ventanas de un segundo en 1,5, 6,5, 10,5 y 15,5 s, con tres osciladores libres, igual ganancia, fases determinadas por semillas fijas y afinación medida por nota. Se probaron 2x y 8x.

Para comparar el perfil de cada nota se excluyeron armónicos que se solapaban con los de otra nota. La métrica usa magnitudes relativas a cada fundamental, parciales de referencia por encima de −40 dBc y un piso de −60 dBc. Es una medida de perfil espectral, no una prueba de fase ni un porcentaje de fidelidad.

| Control | RMSE de perfil armónico a 2x, cuatro ventanas |
|---|---:|
| Sierra actual, forma 0,5 | 0,328–0,365 dB |
| Pulso actual, ancho equivalente fijo 48,4% | 0,344–0,419 dB |

El pulso simétrico de 50% tiene ceros matemáticos en los armónicos pares, ausentes en la grabación. Su error agregado de 22,8–23,2 dB depende mucho del piso numérico y **no se interpreta como un defecto de esa magnitud en el instrumento**. Se buscó un único ancho equivalente entre 5% y 50%, paso 0,1 puntos porcentuales, usando H1–H8 de las primeras dos ventanas. El valor 48,4% se mantuvo en las cuatro ventanas y se verificó con 24 renders C++. La solución reflejada alrededor de 50% tiene el mismo espectro ideal de magnitud.

Este ajuste es una contraprueba con controles existentes: no recupera la posición real de la perilla, no identifica la causa analógica y no cambia el centro del pulso global. Los resultados nominales de las ventanas posteriores ya se habían observado; no se presentan como validación ciega. Las métricas adicionales por bins/bandas se conservan, pero son sensibles al ruido MP3, suelo de señal y fases entre voces; no sustituyen la comparación de parciales activos.

## Barrido de sync: comparación de hipótesis

Se utilizó exclusivamente el canal derecho documentado como Prophet-6 por [el autor de la sesión](https://www.amazona.de/test-sequential-prophet-6-module-vs-oberheim-ob-6-module/). Se analizaron 17 ventanas de 250 ms, en los segundos impares de 1 a 33. Su base medida mediana fue **156,346 Hz**. La ventana de análisis se fijó antes de puntuar modelos. La estabilidad de F0 se conserva en el registro; no se supone que el timbre permanezca estático durante un barrido.

La primera búsqueda produjo 540 renders: tres formas (0; 0,5; 1), cuatro fases de reset (0; 0,25; 0,5; 0,75 ciclos) y 45 relaciones esclavo/maestro entre 1 y 12. Se fijaron ancho de pulso 50%, sobremuestreo 2x, sin filtro, mezclador adicional, Poly Mod ni efectos. La relación se eligió independientemente para cada ventana usando **solo H1–H8**. H9–H16 se midieron aparte. Esto es un ajuste optimista de una familia simplificada, no una reconstrucción de la trayectoria del control ni de todo el patch.

Tras revisar la búsqueda inicial se añadieron 171 renders para refinar la relación en los dos perfiles de sierra más relevantes, con paso 1/16 alrededor de cada mínimo grueso. Esta extensión es adaptativa; los resultados de los armónicos altos ya eran visibles.

| Sierra, relación ajustada por ventana | Mediana H1–H8 usados para ajuste | Mediana H9–H16 no usados para ajustar la relación |
|---|---:|---:|
| Reset actual, fase 0 | 4,403 dB | 6,902 dB |
| Reset alternativo, fase 0,5 | 4,259 dB | 10,937 dB |

Las relaciones elegidas presentan saltos y la sierra con reset actual alcanza el límite de búsqueda 12 en una ventana. No se interpretan esos valores como una trayectoria física ni se afirma que el mínimo global esté dentro de la búsqueda.

El reset alternativo mejora ligeramente el conjunto usado para ajustar la relación, pero empeora claramente los armónicos altos. No hay fundamento para promoverlo. La tabla completa de las otras formas/fases también conserva residuos: no se puede atribuir el barrido a sierra pura ni identificar un reset físico a partir del mejor mínimo.

La hipótesis más limitada que sí se sostiene es que **cambiar solo la fase global de reset no basta dentro de esta familia de pruebas**. Siguen sin conocerse forma/PW exactos, mezcla del maestro, envolventes, número de voces, filtrado y trayectoria de afinación. No se han identificado ni descartado individualmente esas causas.

## Dos demos del 507: el tiempo de nota es decisivo

Se recuperó de [Sequential](https://sequential.com/support/download/prophet-6-sounds/) el banco oficial `P6_Programs_v1.01.syx`, SHA-256 `b46ee1b907b77fdfa17a358e9541722b5a1e021b0a5512d53b0fe9d50c620f19`. Contiene 500 programas válidos de 1178 bytes. Se extrajo 007 **DeepSyncLead**, contraparte de fábrica de 507. Esto no garantiza que las grabaciones conservaran esa versión o todos sus controles.

Se compararon ventanas de 250 ms en AMAZONA 13,5 s y Audiofanzine 13,75 s. Se infirieron teclas MIDI 79 y 57 teniendo en cuenta que OSC2 está una octava abajo. Se mantuvieron notas sintéticas de 1,75 s, velocidad 1, tres pre-rolls (0; 0,5; 1,5 s) y tres edades de nota (0,25; 0,75; 1,25 s). No se reconstruyó la ejecución completa, el legato o el historial de notas anterior.

| Grabación | Perfil v09: error armónico a edad 0,25 s, rango de pre-rolls | A edad 0,75–1,25 s |
|---|---:|---:|
| AMAZONA 507 | 5,74–7,35 dB | 30,43–32,09 dB |
| Audiofanzine 507 | 6,86–10,85 dB | 32,24–35,70 dB |

La variación con edad de nota es mucho mayor que la mejora parcial por reset. Las medianas sobre las nueve hipótesis de edad/pre-roll, sin seleccionar la mejor para cada modelo, fueron:

| Grabación | v09 | Candidato depth12 | Reset 0,5 |
|---|---:|---:|---:|
| AMAZONA: RMSE armónico | 30,653 | 30,653 | 28,829 dB |
| Audiofanzine: RMSE armónico | 33,526 | 33,528 | 30,622 dB |
| AMAZONA: RMSE de bandas | 18,041 | 18,041 | 23,069 dB |
| Audiofanzine: RMSE de bandas | 29,820 | 29,821 | 37,246 dB |

El reset 0,5 mejora parcialmente una métrica y empeora otra en ambas demos. Depth12 no mejora estas hipótesis. Estas comparaciones no validan la hipótesis de patch/ejecución, ni demuestran por sí solas un error de la constante de tiempo de envolvente. El preset mezcla ambos osciladores, seis voces de unison, Slop 50 y Poly Mod hacia frecuencia y forma de OSC1: no es una medición aislada del esclavo.

## Decisión, alcance y reproducción

Se mantiene el sonido actual. El siguiente experimento útil debe controlar trayectoria de pitch y estado de filtro/mezclador/Poly Mod; el 507 requiere además inferir ataques, gates y legato. No se promueve un cambio global de reset ni se declara cerrada v0.19.

El quiz mixto P6/P5 y el ejemplo dinámico OSC2 Shape se mantienen como material de consulta; no se puntuaron como capturas aisladas del P6. SynClavi no se usó como prueba de hard sync.

Los CSV y protocolos completos están en `calibration/v019_sync_comparison/`. Las herramientas verificadas se ejecutan desde la raíz del repositorio, con los audios adquiridos y el banco oficial:

```sh
python3 tools/prepare_sync_reference_suite.py
python3 tools/compare_sync_recordings.py
python3 tools/refine_sync_ratios.py
python3 tools/compare_sync_controls.py
python3 tools/compare_pulse_width_uncertainty.py
```

Para la comparación del 507, colocar el banco oficial en `reference_private/factory.syx` y su mensaje índice 7 en `reference_private/007.syx`. La procedencia, los hashes, los parámetros raw y las hipótesis están registrados. Los scripts nuevos pasan compilación Python; pasan las 17 pruebas existentes de análisis y las ocho pruebas CTest. No se afirma una nueva validación de VST3/pluginval: el motor de producción no fue modificado.
