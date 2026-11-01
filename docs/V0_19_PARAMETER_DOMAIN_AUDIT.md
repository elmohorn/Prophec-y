# v0.19 — Auditoría de dominios, unidades y límites

Fecha: 2026-11-01. Base remota: `d78d0d05afa666149f8240e7102248efb34d0ab1`, rama `v0.19-sound-matching`.

## Resultado

No se encontró una normalización de cutoff 0–1023 ni una división adicional por 255/1023 entre el mapper y el filtro. El dominio empleado es 0–164; la pendiente 0,37 semitonos/raw es una constante explícita de Reference A, no un error de conversión descubierto por esta auditoría.

Se reprodujo y corrigió una inconsistencia en la entrada del mapper: los bytes SysEx fuera de dominio se conservaban correctamente, pero ciertos controles se consumían sin el mismo límite que aplica una edición NRPN. Ejemplo: Osc1Freq=61 producía +37 semitonos al importar y +36 al editar. El mapper ahora limita su lectura según el registro de parámetros, sin modificar `rawPayload` ni `rawParameters`. Los controles legales conservan su mapping.

El mayor riesgo para el fine tuning sigue siendo la ley estática del filtro y su interacción con las profundidades, no un denominador MIDI incorrecto. Reference A v0.9 sólo alcanza **2961,32 Hz de cutoff nominal en raw 164 sin modulación**. Esto no mide la respuesta acústica del filtro, pero demuestra el alcance limitado de esa ley y la posibilidad de que grandes profundidades de envolvente compensen esa limitación.

## Hallazgos y decisiones

| Hallazgo | Clasificación | Decisión |
|---|---|---|
| SysEx fuera de dominio y NRPN podían producir valores DSP distintos | Error de robustez confirmado | Corregido en la frontera del mapper; estado original intacto |
| No hay ruta cutoff 0–1023 | Hipótesis descartada en las rutas examinadas | No modificar denominadores |
| Reference A: 0,37 semitonos/raw; extremo superior 2,96 kHz | Limitación de calibración confirmada por cálculo y DSP | No promover el perfil como ley física; aislar cutoff estático antes de seguir ajustando ADSR |
| Envolvente directa y vía Poly Mod colapsan en una profundidad efectiva cuando comparten destino | Degeneración algebraica del modelo | Documentada y comprobada; no afirmar que reproduce toda interacción del hardware |
| CC102/106 no editan cutoff en `P6MidiRouter` | Compatibilidad MIDI incompleta | Registrada; no inventar aquí una expansión 7-bit→raw sin verificar la convención del hardware |
| Shape satura en 0–1; PW satura en 0,04–0,96 después de sumar modulación | Comportamiento DSP confirmado; límites físicos pendientes | Mantener; las grabaciones actuales no justifican extender las excursiones |
| El techo de frecuencia depende del sample rate interno con oversampling | Seguridad DSP confirmada | Tenerlo en cuenta al comparar herramientas y renders; no confundirlo con límite analógico |
| Tiempos, profundidades, centro de tracking y pendientes no medidos aisladamente | Incertidumbre de calibración | Conservar como candidatos explícitos |

## Recorrido inspeccionado

1. `SysexCodec` desempaqueta los bits altos del transporte de 7 bits a un payload de 1024 bytes. El byte de parámetro no se escala a Hz ni a segundos.
2. `ProgramLayout` asocia 87 campos escalares a offsets y direcciones NRPN. La vista raw conserva el byte exacto; su metadato `normalized` se limita a 0–1.
3. `NrpnDecoder` recompone dirección y valor con dos partes de 7 bits: `(MSB << 7) | LSB`. La capacidad de transportar 14 bits no cambia el máximo legal de cada parámetro.
4. `applyRawNrpn` limita el valor al dominio del parámetro y actualiza la vista raw y su byte serializado.
5. `mapPatchToDSP` lee **raw**, no `RawParameter.normalized`. La corrección agrega el límite documentado al leer controles conocidos.
6. El wrapper JUCE usa `AudioParameterInt` con los mismos mínimos/máximos; `convertTo0to1` se utiliza para notificar al host, mientras que el binding recupera el entero raw. No se identificó una segunda normalización en ese recorrido de código. No se ejecutó el wrapper JUCE en esta fase.
7. `P6Voice` suma modulaciones en octavas/semitonos, convierte a Hz y aplica el límite de seguridad. Los filtros reciben Hz.

## Contrato de unidades

| Control | Dominio raw | Transformación actual | Unidad DSP / límite posterior |
|---|---|---|---|
| Cutoff LP/HP, NRPN45/49 | 0–164 | Ley del perfil; el mapper conserva el código entero | Hz en la voz, después de modulación |
| Resonancia LP/HP, 46/50 | 0–255 | raw/255 × escala del perfil | 0–1 |
| Amount de env LP/HP, 77/82 | 0–254, centro127 | `(raw−127)/127 × profundidad` | Octavas |
| A/D/R amplificador y filtro | 0–127 | `max(0,0005; Tmax × (raw/127)^exponente)` | Segundos; raw0=0,5ms |
| Sustain | 0–127 | raw/127 | 0–1 |
| PM env / Osc2, 143/144 | 0–254, centro127 | `(raw−127)/127` | Ganancia de cada fuente, −1 a +1 |
| Destino PM pitch | Booleano | Activa profundidad configurada | Octavas |
| Destino PM filtro | Booleano | Activa profundidad configurada | Octavas |
| Shape, 3/8 | 0–254 | raw/254 | 0–1; se vuelve a limitar tras PM |
| PW, 4/9 | 0–255 | Ley centrada en127,5, lineal por defecto | 0–1 en mapper; 0,04–0,96 en oscilador |
| Fine Osc2, 6 | 0–254, centro127 | Bipolar ×50 | Cents |
| Keyboard LP/HP | 0/1/2 | 0/0,5/1 | Octavas por octava; referencia MIDI60 |
| Velocidad | MIDI0–127 | raw/127 | Ganancia lineal sobre env directa si está habilitada |

Los valores de fine tuning están en `MapperCalibration`; esta tabla describe la implementación, no valida todas las leyes como mediciones del Prophet-6.

## Cutoff: ecuación y alcance

Para la ley exponencial simple:

`fbase = interceptoHz × 2^(raw × semitonosPorRaw / 12)`

`f = clamp(fbase × 2^(trackingOct + envOct + pmOct + (lfoSemi + pressureSemi)/12), 5, 0,45 × FsInterna)`

`FsInterna = FsHost × oversampling`. El límite se aplica **después** de modular. Por ello no debe calcularse primero un cutoff estático ya recortado para luego aplicar envolventes negativas: eso cambiaría la trayectoria. `rawFilterCutoffHz` devuelve una frecuencia estática ya limitada al sample rate suministrado; es una herramienta de consulta, no la etapa previa del summer dinámico.

| Cutoff raw | Default, Hz antes del límite | Reference A v0.9, Hz antes del límite |
|---:|---:|---:|
| 0 | 8,18 | 88,98 |
| 41 | 87,31 | 213,71 |
| 77 | 698,46 | 461,29 |
| 82 | 932,33 | 513,31 |
| 123 | 9956,06 | 1232,92 |
| 164 | 106318,00 | 2961,32 |

Default y Reference A no difieren sólo en una normalización: cambian tanto intercepto como pendiente. A 48kHz y 1x, Default llega al techo de 21,6kHz desde raw137; a 2x el techo es43,2kHz y se alcanza desde raw149. En Reference A, HP tiene una ley independiente: `40 × (20000/40)^((raw/164)^4)`; no hereda la pendiente LP de0,37.

La rejilla automatizada verifica la coincidencia entre helper y voz para 165 códigos, tres sample rates (44,1/48/96kHz), cuatro factores de oversampling y cuatro leyes LP: Default, Reference A v0.9, curva y tramo con rodilla. En los mismos casos se comprueba HP contra su ecuación independiente, incluyendo la herencia de intercepto y pendiente. Esto verifica unidades e implementación; no calibra el circuito.

## Envolventes: dos curvas diferentes

El exponente del mapper define **posición del control→duración**, no la forma temporal de la envolvente. `ADSREnvelope` implementa después su propia trayectoria exponencial: ataque con objetivo por encima de1; decay/release con objetivo ligeramente por debajo del destino. No hay evidencia nueva que justifique sustituir esas trayectorias por una supuesta tabla de Sequential ni agregar un smoother de150–300Hz.

| Raw A/D/R | Default: Tmax10s, exp3,5 | RefA v0.9 filtro: Tmax2,5s, exp3 |
|---:|---:|---:|
| 0 | 0,50ms | 0,50ms |
| 19 | 12,95ms | 8,37ms |
| 32 | 80,30ms | 39,99ms |
| 59 | 683,39ms | 250,66ms |
| 64 | 908,48ms | 319,94ms |
| 88 | 2769,34ms | 831,72ms |
| 95 | 3620,09ms | 1046,40ms |
| 127 | 10000ms | 2500ms |

Cambiar Tmax del filtro Reference A de2,5 a10s con exponente3 multiplica por4 los tiempos no limitados por el mínimo: D88 pasa de0,83172 a3,32688s. No es equivalente a modificar sólo una curvatura ADSR. La prueba de reloj confirma que el oversampling no multiplica las duraciones por el factor1/2/4/8.

No se modifican los modos Unison/retrigger existentes: `noteOn` puede continuar desde el valor actual; el arranque desde cero depende del modo y del estado de la voz. El smoothing físico y los tiempos absolutos siguen pendientes de capturas controladas.

## Poly Mod: degeneración y límites

El DSP usa `source = E × amountEnv + Osc2 × amountOsc2`, sin limitar la suma de fuentes a±1. Cada destino tiene su profundidad y su límite propios. La cantidad real de la fuente Osc2 depende además de la forma y normalización de esa onda.

Para un destino de filtro:

`envOct + pmOct = E × (vel × depthEnv + amountEnv × depthPM) + Osc2 × amountOsc2 × depthPM`

Con Osc2 neutro y velocidad fija, infinitas parejas de cantidades directas/PM producen la misma trayectoria de cutoff. Nuestro test lo demuestra con una profundidad directa de2oct más PM0,5×3oct, equivalente a una profundidad directa de3,5oct. Esto es una propiedad matemática del código, no una medición del hardware.

El manual describe una interacción de respuesta entre las dos rutas de envolvente. Nuestro summer puede cambiar la excursión y el momento en que se alcanzan límites, pero por sí solo no crea dos curvas de envolvente independientes. Falta determinar si la respuesta observada exige una transformación distinta en alguna ruta. El manual no basta para deducir esa función.

En pitch, la profundidad configurada está en octavas y se aplica una sola vez mediante `2^(source × depth)`. El oscilador limita finalmente su frecuencia a0,01–0,45×FsInterna. Shape se limita después de sumar PM a0–1; PW después de LFO+PM a0,04–0,96. En shape1 con fuente positiva, anular el destino puede dejar el audio idéntico porque ambas rutas acaban en1. Una fuente negativa puede sacar el control de ese límite. Se añadieron trazas de cutoff LP/HP, shape y PW aplicados para comprobarlo sin inferir controles desde FFT.

## Pruebas y cobertura

Se agregó `p6_parameter_domain_audit`, disponible como test CTest y como ejecutable con entrada de banco opcional. Los checks usan terminación explícita y siguen activos con `NDEBUG`.

- 87 campos: offsets y NRPN únicos; mínimos/máximos consistentes entre registro y layout.
- 7745 valores legales: transporte NRPN de14bits→payload→SysEx→decode; equivalencia de controles mapeados.
- 22272 casos byte/campo (87×256), cada uno en Default y RefA v0.9: límites importación/edición y conservación del payload.
- 7920 casos pareados de cutoff LP/HP en voz: tres sample rates, cuatro oversamplings, cuatro leyes, 165 valores.
- 512 casos de ley tiempo; 18 extremos/centros bipolares; 12 combinaciones de reloj de envolvente.
- Pruebas numéricas del summer, velocidad, límite posterior a modulación y degeneración EG/PM.
- **9/9 CTest aprobados**, incluyendo las suites previas de núcleo, estado, control, realtime, FX y Reference Isolation.

El snapshot comparado contiene76 controles de `MappedPatch` relevantes para síntesis y performance. No evalúa todos los parámetros internos de FX. Los roundtrips de87 campos sí incluyen sus bytes, pero no certifican fidelidad acústica de FX ni cobertura integral de nombres/secuencias. Tampoco se declara una nueva validación pluginval ni un nuevo barrido de audio de todos los presets.

## Banco oficial

Se auditaron los500 mensajes del banco ya disponible, sin redistribuir sus datos. Fuente: ZIP oficial de Sequential `P6-Factory-Programs-ReadMe.zip`. SHA256 del `.syx`: `b46ee1b907b77fdfa17a358e9541722b5a1e021b0a5512d53b0fe9d50c620f19`.

Todos los mensajes pasan roundtrip binario exacto. Hay un campo fuera de metadatos: banco4/programa20, NRPN88, LFOFreq255 frente al máximo254 documentado. Ya tenía idéntico resultado mapeado al de254. No se altera el byte del banco ni se concluye que sea corrupción: podría ser un valor tolerado por firmware.

Se compararon76000 escalares mapeados (500×2perfiles×76) antes/después de la corrección del mapper. Las huellas textuales hexfloat son idénticas, SHA256 `951b12df22cff99264af7714812608b07121dab09781fc7b73a9ad5d623f8023`. Esta evidencia verifica controles mapeados, no es un hash de audio.

## Reproducción

```sh
cmake -S . -B build-check -DP6_BUILD_PLUGIN=OFF
cmake --build build-check -j4
ctest --test-dir build-check --output-on-failure
build-check/p6_parameter_domain_audit
build-check/p6_parameter_domain_audit --bank /ruta/privada/factory.syx /tmp/mapped-controls.txt
```

La primera ejecución anterior a la corrección falló de forma reproducible en NRPN0, byte61. La huella previa del banco se produjo con el test/trazas ya añadidos pero antes del cambio a `rawByNrpn`. Los datos originales del banco y las huellas completas permanecen privados.

## Siguiente paso recomendado

1. Seleccionar referencias existentes con cutoff alto, resonancia baja y poca modulación, verificando sus patches; usarlas para imponer una restricción independiente al extremo superior de la ley LP. Si no existen controles suficientes, registrar el faltante en lugar de inferirlo desde507.
2. Congelar esa ley estática y evaluar una sola familia de tiempos de envolvente con notas/gates conocidos o con incertidumbre explícita. Mantener separados el mapping raw→segundos y la trayectoria temporal.
3. Usar las nuevas trazas para cuantificar cuánto tiempo cada render permanece en los límites de cutoff/shape/PW. Sólo después investigar funciones distintas entre env directa y PM, y no introducir asimetrías OTA o constantes físicas sin evidencia.

Esto cambia el objetivo de la próxima fase: reducir las compensaciones entre parámetros antes de otro barrido multidimensional. No se promueven depth12, time10_curve3 ni una nueva fase de sync como resultado de esta auditoría.

## Fuentes y límites de atribución

- [Manual oficial Prophet-6 v2.1](https://sequential.com/wp-content/uploads/2021/02/Prophet-6-Operation-Manual-2.1.pdf): apéndice MIDI y secciones Filter Envelope/Poly Mod/Unison. Se usó para contrastar dominios y modos, no para inventar curvas físicas.
- Código inspeccionado: `SysexCodec.cpp`, `ProgramLayout.cpp`, `ParameterRegistry.cpp`, `NrpnCodec.cpp`, `P6MidiRouter.cpp`, `ParameterMapper.cpp`, `P6Voice.cpp`, `ADSREnvelope.cpp`, `PolyBLEPOscillator.cpp`, wrapper `PluginProcessor.cpp` y codec de estado.
- Constantes0,37, 2,5s, 13oct y los límites DSP proceden del código del proyecto. Los cálculos y los resultados del test son evidencia de implementación, no datos nuevos de un Prophet-6 físico.
