# v0.19 — nuevas grabaciones para estudiar hard sync

Fecha de adquisición: 31 de octubre de 2026 (Santiago).

**Se recuperaron ocho MP3 válidos y se prepararon cuatro WAV con el canal del Prophet-6.** La mejor nueva referencia es un barrido dedicado de sync de OSC1. Todavía no se encontró una captura con todos los controles, afinaciones y SysEx/MIDI documentados. Esta fase incorpora fuentes; no presenta nuevas comparaciones del motor ni modifica el sonido de producción.

## Prioridad: barrido de sync, con instrumentos separados por canal

[t.goldschmitz, AMAZONA, 8 de marzo de 2017](https://www.amazona.de/test-sequential-prophet-6-module-vs-oberheim-ob-6-module/) documenta que sus comparaciones colocan el OB-6 a la izquierda y el Prophet-6 a la derecha. La [tercera página](https://www.amazona.de/test-sequential-prophet-6-module-vs-oberheim-ob-6-module/3/) incluye el archivo **Sync Sweep OSC1**, junto con ejemplos de sierra, pulso y variación de forma de OSC2.

Se descargaron los cuatro originales estéreo. Para cada uno se copió exclusivamente el canal derecho decodificado a WAV mono float32, sin normalización, remuestreo ni recortes. La identidad muestra a muestra se verificó. El MP3 podría introducir fuga entre canales; seleccionar un canal no demuestra aislamiento físico perfecto. Los enlaces del editor contienen literalmente `/2030/01/`; esa ruta no cambia la fecha de la publicación.

El barrido aporta una referencia dinámica más específica que un preset con múltiples voces. Sin embargo, no publica una trayectoria exacta de afinación, valores raw de forma/PW, mezcla de osciladores, filtro, efectos o número de voces. Puede guiar una comparación exploratoria con incertidumbres explícitas; por sí solo no permite identificar una corrección única del reset del VCO.

## Preset 507 y quiz de sync

La [reseña de AMAZONA de 2015](https://www.amazona.de/test-sequential-prophet-6-analog-synthesizer/) proporciona **507 SyncLead Uni** y **535 SynClavi**. Se recuperaron ambos. El segundo queda como auxiliar: su nombre no prueba que use oscillator sync.

[Audiofanzine, Eric Synthwalker](https://fr.audiofanzine.com/synthe-analogique/sequential/prophet-6/editorial/tests/le-retour-du-roi.html) ofrece otra interpretación del **507** y un **Quiz Sync VF**. El [autor confirma en el mensaje 171](https://fr.audiofanzine.com/synthe-analogique/sequential/prophet-6/forums/t.603359,commentaires-sur-le-test-le-retour-du-roi,post.8522542.html) que el orden es Prophet-6 primero y Prophet-5 después. La reseña describe el quiz sin efectos y el Prophet-6 en mono. El mensaje 163 añade ataque y release a cero para el sonido sync.

**El quiz conserva ambos instrumentos y no se puntuará entero como Prophet-6.** Falta fijar su frontera temporal y conocer el patch exacto. Tampoco se asumirá que dos ejemplos llamados 507 conservan idénticos bytes, efectos o controles durante la interpretación. La lista de descargas FLAC se localizó, pero no se obtuvo un FLAC verificado; los archivos adquiridos siguen siendo MP3.

## Otras pistas comprobadas

- [Polydata — tutorial FM del Prophet-6](https://www.youtube.com/watch?v=iuOlPy_AlIA&t=882s): sección de oscillator sync entre 14:42 y 19:00, según los capítulos del autor. Se obtuvo metadata, pero la descarga normal devolvió 195 bytes que ffprobe rechazó. No hay audio adquirido ni ajustes extraídos de sus fotogramas. La descripción y capítulos también están [indexados aquí](https://www.matrixsynth.com/2022/06/tutorial-fm-synthesis-with-sequential.html).
- [Rüdiger Gaenslen — comparación Prophet-6/Prophet-5](https://www.youtube.com/watch?v=sLfQ_DaXIHM): la descripción del autor documenta ejemplos sync/unison, P6 en mono y FX bypass, pero también slop de 35%. Es una referencia de conjunto, con intervalos/estado incompletos.
- [Reverb — sonidos de Bernie Worrell](https://reverb.com/news/video-the-synth-sounds-of-parliament-funkadelics-bernie-worrell): el propio autor advierte que la receta escrita es de Prophet-5 aunque use Prophet-6 en el video; además interviene wah. No sirve como receta exacta del P6.
- [PresetPatch](https://www.presetpatch.com/synth/sequential-prophet-6): se localizó Furry_Sync_Lead.syx, pero no una pareja audio/SysEx accesible y verificada. No se sortearon controles de descarga.

## Archivos y validación

`calibration/v019_sync_sources/acquisition.json` conserva URLs, selección de canal, SHA-256 de originales y PCM, duración decodificada, formato y limitaciones por archivo. Todos los originales se decodificaron completos y contienen muestras finitas. Los cuatro WAV derivados coinciden exactamente con el canal derecho de sus originales decodificados.

Reproducción desde la raíz del repositorio:

```sh
python3 tools/prepare_sync_reference_suite.py
```

La herramienta verifica hashes antes de aceptar archivos y detiene la ejecución ante HTML, contenido sustituido o una decodificación distinta. Se ejecutó con éxito sobre los ocho archivos. No se requieren nuevas pruebas del plugin para este registro de fuentes: no cambió DSP, mapper, presets ni valores predeterminados.

La próxima comparación debe empezar por el canal derecho del barrido, usar la sierra/pulso de la misma sesión como controles y mantener los parámetros no documentados como hipótesis. Los ejemplos del preset 507 permiten después comprobar si una explicación se sostiene en otra grabación. Ninguna mejora aparente autoriza por sí sola una calibración global.
