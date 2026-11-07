# Auditoría global de Prophec-y — 2 noviembre 2026

Base publicada: preview 278, commit `77e549f31d31cd93c702f862cfb7d0426f60db73`.
Auditoría de arquitectura, fronteras de estado/MIDI, DSP Character, UI, presets,
pruebas y entrega. Revisión estática dirigida, ejecución de las 15 suites CTest
y una reproducción C++ independiente del defecto de Neon. Se verificaron
fuentes remotas relevantes y los tres assets publicados de la release.
No se realizó escucha comparativa, prueba manual en DAW, TSan, medición completa
de aliasing ni revisión exhaustiva línea por línea de todo el motor.

## Dictamen

Base válida para continuar; no hay motivo demostrado para reescribir el core.
La release es un prototipo funcional con buena infraestructura de pruebas.
No equivale a validación acústica del Prophet-6, madurez de los cuatro nuevos
FX ni certificación de continuidad en proyectos reales de DAW.
Recomiendo estabilización antes de agregar funciones o dar por cerrado v0.20.

## Hallazgos priorizados

### A01 — Alta: discontinuidad de Neon con Bias + Symmetry

Confirmada ejecutando `CustomCharacter::neonTransfer` enlazado con las librerías
Release. Con Drive/Folds/Symmetry/Bias = 0.42, x=-1e-7 produce -0.783051 y
x=+1e-7 produce -0.625312: salto de 0.157739 para una entrada casi idéntica.
`plugin/CustomCharacter.cpp` selecciona una ganancia distinta por signo de x
y luego el kernel aplica el offset antes de multiplicar. Si bias no es cero,
ambos lados tienen límites distintos. La rama de audio emplea la misma ley.
Esto genera contenido de alta frecuencia adicional; no se ha cuantificado su
nivel audible tras filtros y oversampling. Si se desea esta discontinuidad,
debe ser una decisión sonora explícita, no un efecto incidental de controles.
Cierre: definir una ley continua o un modo explícito, probar cruces por cero,
bias/simetría combinados y medir aliasing contra una referencia de mayor tasa.

### A02 — Media: tiempos de respuesta dependientes del sample rate

En `CustomCharacter.cpp`, Plasma usa coeficiente 0.04 y el ataque de Ferrofluid
0.01 por muestra sobremuestreada. Las constantes de tiempo equivalentes son:

| Sample rate | Ataque Ferrofluid | Borde Plasma |
|---|---:|---:|
| 44.1 kHz | 1.128 ms | 0.278 ms |
| 48 kHz | 1.036 ms | 0.255 ms |
| 96 kHz | 0.518 ms | 0.128 ms |
| 192 kHz | 0.259 ms | 0.064 ms |

Calculadas como -1/(2*sr*ln(1-alpha)); no son mediciones perceptuales.
La liberación de Ferrofluid y el hold de Plasma sí usan sample rate.
Cierre: expresar los tiempos deseados en segundos y verificar equivalencia
entre tasas con el mismo estímulo en Hz y duración física.

### A03 — Alta: MIDI combina canales y comparte el estado NRPN

`src/P6MidiRouter.cpp` conserva el nibble de tipo y descarta el canal para
notas, CC, pitch bend y presión. Hay un único `NrpnDecoder nrpn_`.
Mensajes de selección NRPN y Data Entry de canales distintos pueden combinarse;
CC121 de otro canal también puede recentrar el pitch bend global.
Esto confirma una vulnerabilidad de diseño relevante para un controlador
multizona como el FA-08; no demuestra qué mensajes está enviando ese teclado.
Cierre: contrato Omni/1–16 explícito, decodificadores NRPN por canal, política
de propiedad de notas y pruebas con tráfico intercalado. Añadir monitor CH.

### A04 — Alta: huellas diagnósticas no son un gate automático de regresión

En `.github/workflows/plugin-ci.yml`, DSP fingerprint snapshot ejecuta el
programa y escribe su salida. `tools/dsp_fingerprint.cpp` no compara una
referencia ni devuelve fallo por un cambio de huella. El benchmark también
imprime resultados. Ambos instancian P6Engine sin instalar CharacterFx.
La CI verde no certifica conservación automática de esas huellas ni presupuesto
CPU de los cuatro módulos. Las comparaciones manuales anteriores siguen siendo
válidas para los commits comparados, pero no protegen futuros cambios solas.
Cierre: referencias versionadas con tolerancias/plataforma definidas y revisión
explícita de cambios; benchmark del wrapper + cada Character, bloques pequeños,
seis voces, parámetros extremos y percentiles de tiempo por bloque.

### A05 — Media: cobertura acústica insuficiente en Character

`tests/character_fx_test.cpp` verifica determinismo, finitud, energía, dry null,
allocations ordinarias, rutas y una cola spring. No detectó A01 ni valida THD,
aliasing, impacto de cada control, continuidad de automatización o fidelidad.
El estímulo `.2*sin(n*.11)` cambia de frecuencia física al variar sr. Las
rápidas selecciones cada 257 muestras prueban transiciones cortas, no permanencia
prolongada de cada módulo en extremos. No prueban por sí solas ausencia de clicks.
Cierre: ensayos complementarios de audio y automatización con métricas y
referencias independientes; mantener los tests estructurales actuales.

### A06 — Media: restore de sesión no se publica como una transacción completa

`PluginProcessor::setStateInformation` reemplaza patch bajo patchLock_, libera
el lock, notifica parámetros individualmente y al final publica fullPatchLoadPending_.
processBlock lee Character/Master mediante atomics independientes. Si restore
se solapa con render, existe una ventana para una combinación parcial de estados.
Es un riesgo por inspección; no se ha reproducido un fallo de audio ni una data
race. Atomics individuales no garantizan coherencia de un preset completo.
Cierre: prueba concurrente controlada y publicación de snapshot/versiones en
frontera de bloque. No añadir un lock bloqueante al callback de audio.

### A07 — Media: contrato de controles y Studio incompletos

Solo los controles con descriptor NRPN reciben `configure()` y el contrato
común completo. Character usa attachments/textboxes/rebinding y no hereda el
mismo menú, Enter/Home y foco dibujado de los controles configurados.
Studio sigue siendo GenericAudioProcessorEditor. Esto es funcional, pero aún
no cumple el Inspector contextual propuesto ni una interacción uniforme.
Cierre: descriptor para parámetros software sin inventar NRPN; contrato común
verificable por teclado y lector de pantalla; luego construir Inspector.

### A08 — Media: visualización medida y metáfora física se mezclan

Los taps de Spring y la envolvente de Ferrofluid son datos reales, pero la
geometría de resortes/fluido es ilustrativa. Plasma y varias telemetrías usan
solo L; señal exclusiva de R no queda representada por esas trazas.
La historia de 32 puntos decimada por 8 no es un osciloscopio de banda completa.
Motion apagado congela los dispositivos nuevos, mientras Tube fija un brillo
base de 0.12. Revisar uniformidad, estados sin señal y codificación de caracteres
en capturas nativas antes de aprobar acabado visual. No confundir estas vistas
con mediciones físicas del dispositivo representado.

### A09 — Media: compatibilidad del codec no equivale a migración de DAW

`tests/state_versions/README.md` declara correctamente fixtures sintéticos
emitidos por encoders históricos. Eso protege bytes/formato y no demuestra
recuperación de automatización, selección de presets, buses o proyectos DAW.
Los fixtures permanentes cubren formatos v1/v2; la comprobación v3 actual usa
bytes derivados del encoder actual. Falta fixture independiente permanente v3
(y fijar v4 al estabilizarlo). La asociación ID/alias de biblioteca es transitoria
tras restaurar sesión, limitación documentada del browser.
Cierre: proyectos reales de al menos dos hosts, restore con transporte parado
y activo, automatización antigua, Save/Save As y recuperación tras reinicio.

### A10 — Media: cierre acústico y documentación requieren una única matriz

El cierre v0.19 remoto declara explícitamente calibración provisional, holdouts
pendientes y ausencia de prueba manual DAW. Core/filtros/sync/modulación no deben
presentarse como paridad demostrada con Prophet-6. Optical Spring es un FDN con
dispersión y mezcla de taps; Plasma es una ley original de conducción. Sus
nombres no acreditan emulación de pedales físicos.
`V0_20_CURRENT_STATUS.md` aún enumera output meters como pendientes en el apartado
del display, pese a estar implementados. El roadmap y la terminología de cierre
necesitan un estado único: implementado / probado / validado musicalmente /
aprobado visualmente. El plan inicial no debe confundirse con estado vivo.

## Fortalezas respaldadas

- 15/15 CTest aprobados de nuevo durante esta auditoría.
- Payload Prophet y extras software separados; codec versionado con rechazo
  de truncados; selector antiguo None/Tube conserva su dominio.
- Core separado de JUCE; UI nueva fuera del callback de audio.
- Biblioteca con escrituras por archivo mediante temporal y reemplazo,
  lock entre procesos y protección de slots/ediciones obsoletas.
- Publicación depende de los jobs requeridos; preview 278 contiene ambos ZIP
  y SHA256SUMS, y apunta al commit auditado. main se identifica como base v0.18
  con portada de descargas; el desarrollo activo reside en v0.20-ui.
- Reutilización identificable de DSPark y DaisySP con licencias y procedencia.

## Orden recomendado y aceptación

1. Corregir A01; normalizar A02; ampliar pruebas acústicas A05.
2. Convertir A04 en gates efectivos, incluyendo CPU del wrapper/Character.
3. Resolver política MIDI A03 y verificar transacciones A06 antes del cierre.
4. Completar A07–A09 con pruebas reales de uso; revisar visuales con el usuario.
5. Unificar A10 y decidir RC1 a partir de evidencia, sin exigir reescribir core.

Esta auditoría no cambia DSP, parámetros, presets ni la release publicada.

## Primer bloque publicado en preview 280

- A01: Neon usa una asimetría continua antes del bias; cobertura de cruces
  por cero con combinaciones de controles. Quedan medición de aliasing y escucha.
- A02: coeficientes de ataque de Plasma/Ferrofluid derivados de la frecuencia
  de muestreo, conservando la respuesta de 48 kHz. Falta comparación acústica
  independiente entre tasas.
- A03: decodificadores NRPN independientes por canal, con mensajes intercalados
  y reset cubiertos. El selector Omni/1–16, la propiedad de notas y el monitor
  aún no están implementados; los performance controllers siguen globales.
- A04: gate Linux para cuatro métricas de cinco escenarios del core frente a
  referencia versionada, tolerancia relativa 0.01 % y absoluta 1e-5. El hash
  se mantiene diagnóstico; Windows todavía no compara la referencia. El gate
  detectó una regresión numérica inyectada de prueba. Falta benchmark Character
  y validar la tolerancia en los runners de CI.

La release 278 no incluye estas correcciones; consultar el commit y la CI de la siguiente preview para conocer su estado de publicación.

### Primer benchmark Character

`p6_character_benchmark` mide P6Engine con seis notas y el insert Character,
2x de oversampling del core y parámetros 0/1 al 80 % en los módulos custom,
250 bloques por escenario, 32/64/128 samples y 48/96 kHz. Registra promedio,
p99, máximo y presupuesto de bloque para None/Tube/cuatro dispositivos. Linux
CI lo ejecuta como diagnóstico, sin umbrales de rendimiento todavía.
La captura local `measurements/character_benchmark_2026_11_02.csv` es una sola
pasada en un entorno compartido, sin prioridad de audio; no representa el PC ni
el DAW del usuario. No incluye wrapper JUCE, editor ni FX estándar, y 250 bloques
solo ofrecen una estimación preliminar del p99. Algunos valores superan el
presupuesto; no se puede atribuir sin más a DSP o descartar interferencia del
sistema. Falta medición repetida en DAW y condiciones controladas.

El VST3 Linux local y el smoke de editor/audio compilaron y pasaron tras los
cambios NRPN/Neon; la integración del benchmark fue verificada en build-core.
La comprobación de huella se ejecutó sobre un render nuevo y pasó. La validación
Windows/pluginval y la publicación deben verificarse en la CI de la preview correspondiente.

### Fixture independiente de codec v3

Se añadió `tests/state_versions/v020_codec3.state`, emitido por el encoder
histórico de preview 275 verificado por Git blob `868b21d...`, con Master
-23.4 dB y Tube activo. El test lo lee desde disco, comprueba payload, wrapper,
Master y truncamientos. Proveniencia y SHA-256 en el manifiesto. Es sintético:
no sustituye la recuperación de proyectos reales en DAW.


### Medición y candidato 4x para Neon — bloque posterior a preview 280

El probe `p6_neon_rate_probe` genera senos de 251, 997 y 4001 Hz con Drive y
Folds al 80 %, Bias/Symmetry al 42 %, Tone al máximo, durante un segundo a
48 y 192 kHz. `tools/analyze_neon_rates.py` baja la referencia de 192 kHz con
filtro polifásico, descarta el arranque y minimiza un desplazamiento fraccional
entre -2 y +2 muestras. Es una diferencia total de forma de onda; incluye
aliasing, filtros, discretización y remuestreo, y no es una medición aislada de
aliasing ni una validación auditiva. Requiere numpy y scipy.

Comparación con la misma referencia de 192 kHz / 4x Medium. El código 2x corresponde al commit `d9c72ca0b1b6f02fbb29ea2be7f9e46bd350441d`; los datos están en [2x](measurements/neon_2x_vs_4x_reference.csv) y [4x](measurements/neon_4x_vs_4x_reference.csv):

| Entrada | 2x High anterior | 4x Medium candidato |
|---|---:|---:|
| 251 Hz | -35.19 dB | -43.18 dB |
| 997 Hz | -14.87 dB | -22.81 dB |
| 4001 Hz | +6.83 dB | -15.52 dB |

El candidato reduce la diferencia medida; aún puede haber aliasing audible.
Usa 48 muestras de retardo del FIR más 23 de alineación, para conservar 71.
Un microbenchmark aislado del efecto sobre 10 segundos de audio tardó 0.244 s
con 2x High y 0.383 s con 4x Medium en este entorno compartido: ≈1.57 veces el
coste de ese efecto, no una medición del plugin completo ni del PC del usuario.
Los resultados son exploratorios; faltan escucha, CPU en DAW y comparación
espectral independiente. El cambio modifica el sonido de presets Neon previos
en esta etapa de preview; el estado y los IDs de parámetros se conservan. Se añadió una prueba de llegada del impulso y una
prueba del tiempo de ataque de Ferrofluid a cuatro sample rates.

### MIDI: canal y diagnóstico — bloque posterior a preview 280

Selector Omni/1–16 y monitor de dos eventos CC/PB; los mensajes filtrados se
marcan. Cambiar canal silencia notas, vacía colas estándar y reinicia controles.
Se conserva el reloj de sistema y los presets no alteran este ajuste global.
Codec v5 migra v1–v4 a Omni, con una nueva fixture independiente v4 emitida
por la preview 278. En Omni, las notas de igual tono se agrupan y se liberan al salir el último
canal; CC120/CC123 y performance controllers siguen siendo globales. No hay
MPE ni sustain independiente por canal. Falta probar con el FA-08 y en DAW.


### Telemetría estéreo y Motion — bloque posterior a preview 280

Las trazas y medición de filtro seleccionan el canal de entrada dominante con
promedio e histéresis; Plasma y Neon registran actividad en ambos canales.
Pruebas de señal exclusiva por R cubren Plasma, Ferrofluid y Neon. Motion off
congela la iluminación de Tube; bypass la apaga. La representación sigue siendo
una metáfora y el historial decimado no sustituye un osciloscopio completo.

### Restore concurrente — bloque posterior a preview 280

Se reprodujo el intercalado: Character nuevo y Master anterior mientras el
escritor estaba detenido. La corrección protege la aplicación de controles por
bloque con un gate de intento no bloqueante y una bandera de publicación. El
escritor espera a que termine una captura ya iniciada y luego notifica al host
sin mantener el gate de audio. Restore/import y lecturas de estado/SysEx se
serializan en el lado no realtime. El probe comprueba el caso provocado, un
save concurrente y 24 rondas de dos escritores con audio en marcha. Un NRPN
recibido durante la pausa utiliza la calibración previamente aplicada. Es evidencia
limitada al alcance del probe, no una certificación de todos los hosts ni de
reentradas arbitrarias; faltan estrés prolongado y pruebas reales en DAW.
