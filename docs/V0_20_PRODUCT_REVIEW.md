# Revisión de producto y referentes para v0.20

1 de noviembre de 2026. Código revisado: `fa5fc6dca16d2f0e7d9c31df50d8e38344022911`. Este documento revisa la propuesta del propietario; los criterios y el orden siguientes son recomendaciones, no hitos ya implementados.

## Conclusión

Conservar el núcleo, modularizar la UI de manera incremental y priorizar edición, MIDI, presets y visualización fiable. La compatibilidad raw/SysEx y los gates automatizados son una buena base, pero no bastan para declarar madurez equivalente a proyectos con años de uso, pruebas en DAW y validación externa. Se mantiene abierta la fidelidad acústica.

El checkpoint revisado ya contiene la válvula reactiva, selectores FX A/B, formato semántico en parte de Module, RAW14 de pitch bend, corrección de CC121 y headroom medido. No conviene planificar esos elementos como si no existieran. Las releases preview.245 y preview.260 corresponden a commits diferentes; sólo la segunda incorpora los avances posteriores de MIDI/formato/FX/headroom.

## Hallazgos concretos

### 1. El formato debe compartir el modelo del DSP

`plugin/PluginEditor.cpp::hardwareDisplayValue` fija los tiempos ADSR con `rawEnvelopeTime(raw,10.0,3.5)`. El perfil Reference A 0.9 hereda máximos de 2,5 s y exponente 3 para el filtro desde `referenceACandidateV07()` en `src/ParameterMapper.cpp`. Por tanto una envolvente de filtro raw 64 se muestra aproximadamente como 0,91 s, mientras ese perfil calcula aproximadamente 0,32 s. Es un desajuste real, no sólo una mejora estética pendiente.

`ProphecyParameterControl` debe delegar en un descriptor/formateador que reciba la calibración y modo efectivos. Compartir fórmulas y parseo inverso con el modelo; no copiar constantes a la UI. Conservar raw/NRPN visibles en el Inspector. Los valores calculados del modelo no deben presentarse como mediciones verificadas del hardware. LFO sincronizado necesita división efectiva, no sólo su frecuencia libre. Una curva de envolvente representa la semántica real de Attack/Decay/Release, no una promesa de duración total de la nota.

### 2. El headroom está medido; la solución sigue abierta

El ledger actual registra hasta +16,91 dBFS de pico en el caso dual-oscillator máximo y seis voces. Eso identifica exceso respecto de 0 dBFS en la salida; por sí solo no prueba truncamiento dentro de un bus flotante. Localizar saturación/recorte por etapas antes de atribuir todo a Unison. Un Master final reduce la amplitud enviada al host, pero no deshace distorsión ya generada antes.

Preservar la suma interna y Program Volume; añadir Master Output independiente al final, con smoothing y medidores L/R antes/después. Elegir su default con una matriz de presets/FX/Unison, sin garantizar que una atenuación fija resuelva cualquier entrada. Los estados antiguos deben migrar preservando su ganancia anterior; distinguir default para nueva instancia de fallback de un proyecto histórico. El peak meter indica sample peak; true peak requeriría otra medición.

### 3. RAW14 sin canal no identifica el origen del problema del FA-08

El router conserva el valor de pitch bend pero descarta el canal al hacer `status & 0xF0`. La telemetría actual no expone canal. Dos canales con valores diferentes pueden alternarse en la pantalla. Eso no demostraría una rueda defectuosa ni su retorno físico al centro.

Añadir selector Omni/1–16 y monitor con canal, tipo, dato y marca de aceptado/filtrado. Usar cola de tamaño fijo y contador de eventos descartados; no strings/logging sin límite en audio. Los mensajes de reloj son de sistema y necesitan política separada. `NrpnDecoder nrpn_` es una única instancia: en Omni hay que impedir que se mezclen selección y datos de canales distintos. Definir también sustain/note-off cuando el mismo número de nota llega por varios canales y al cambiar de canal de entrada, evitando notas atascadas.

### 4. La respuesta de filtros requiere indicar qué se representa

El LPF actual tiene realimentación y límites no lineales. Una curva obtenida de coeficientes no es una respuesta universal del audio en tiempo real. Una primera vista puede mostrar respuesta nominal de pequeña señal, identificada como tal y basada en el modelo/estado efectivo. Una medición real necesita señal/protocolo definidos. Reutilizar primitivas para waveform/ADSR/LFO y separar preview determinista de señal observada.

### 5. Compatibilidad se protege antes de refactorizar

Recuperar fixtures genuinos desde versiones históricas; no generar hoy un estado y renombrarlo como v0.17. Cubrir payload desconocido, valores musicales, controles propios del wrapper, defaults añadidos y estados malformados. Un mismo estado musical no implica audio idéntico entre versiones que corrigen un defecto documentado.

## Referentes verificados y alcance

| Referente | Uso recomendado | Límite |
|---|---|---|
| Surge XT | Edición coherente, menú contextual, navegación y accesibilidad | Copiar el contrato de interacción sin importar su complejidad de síntesis |
| Dexed | Librarian/SysEx, compatibilidad, MIDI por canal y separación de módulos | La corrección de CC por canal es una referencia de diseño; no diagnostica por sí sola el FA-08 |
| Vital | Feedback visual que ayuda a comprender el sonido | Empezar por pocas vistas veraces; no asumir GPU o 60 fps como requisito |
| Gearmulator | Separación dispositivo/plugin/editor | Ejecutar firmware de DSP digital no es un patrón transferible de fidelidad al Prophet analógico |
| Odin 2 | Proyecto de escala acotada y archivo histórico de patches | La carpeta patch_versions existe; eso no demuestra por sí solo una suite CI de migraciones equivalente a la propuesta |

Surge documenta menús de parámetros y accesibilidad. El README actual de Dexed confirma la reorganización en core/engine/midi/parameter/ui y la corrección del canal en mapeos CC; esta última aparece en su changelog 0.9.9 y no debe describirse como una novedad recién publicada. Vital advierte que su repositorio se actualiza con retraso respecto a los binarios. Estudiar patrones no implica reutilizar código; cualquier incorporación de código o recursos debe revisar la licencia del archivo concreto.

Fuentes primarias consultadas:

- https://surge-synthesizer.github.io/manual-xt/
- https://surge-synthesizer.github.io/accessibility/
- https://github.com/asb2m10/dexed
- https://asb2m10.github.io/dexed/
- https://github.com/mtytel/vital
- https://github.com/dsp56300/gearmulator
- https://github.com/TheWaveWarden/odin2

## Orden propuesto para llegar a RC1

| Paso | Trabajo acotado | Criterio de salida |
|---|---|---|
| 0 | Fixtures históricos y snapshot del estado actual | Migraciones comprobables antes de añadir controles o refactorizar |
| 1 | Descriptores, formato/parseo y control común; extracción gradual de componentes | UI y DSP coinciden en perfiles/modos; edición exacta, reset, fine edit, raw, help y teclado comparten contrato |
| 2 | Master/medidores y política MIDI con diagnóstico | Headroom documentado por etapas; canal y controladores reproducibles sin notas atascadas |
| 3 | Browser local mínimo | Buscar, cargar, guardar, favoritos, import/export y dirty state; cancelación de import/export no destruye edición |
| 4 | Studio como Inspector y display contextual | Cada vista distingue control, preview del modelo y señal medida; valores efectivos compartidos |
| 5 | FX A/B y Character como flujo integrado | Edición específica de efectos verificados, cadena clara, nada de slots de funciones inexistentes |
| RC1 | Congelar funciones, probar en DAW, empaquetar | Carga de proyectos/automatización, reapertura, MIDI, escucha, accesibilidad básica y binarios trazables |

No crear todavía un framework multi-sinte. Extraer componentes pequeños alrededor de necesidades presentes; reconsiderar abstracciones reutilizables cuando exista un segundo consumidor real. UI, presets y contrato de interacción necesitan modularización; el núcleo no requiere una reescritura general para ejecutar este plan.
