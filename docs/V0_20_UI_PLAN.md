# v0.20 — Interfaz y flujo de presets

## Actualización de prioridades — 5 de noviembre de 2026 (planificación, no implementación)

Esta decisión de planificación sustituye el orden de prioridades previo del 1 de noviembre; no afirma que las tareas siguientes estén ejecutadas. No modifica los compromisos de preservación SysEx/NRPN, las huellas DSP congeladas ni la compatibilidad de sesiones, y no requiere acceso a un Prophet-6 físico.

1. **Primero: cerrar interfaz y biblioteca/presets.** Completar el Studio/Inspector, alinear/validar controles, asegurar que las visualizaciones representen fielmente el modelo, cerrar Compare A/B y undo/redo; terminar el flujo real de biblioteca (Save/Save As, favoritos/búsqueda, import/export SysEx, estado de Character) y comprobar round-trip de patches/sesiones en DAW, con pruebas explícitas de migración v1–v5. El browser local inicial existe, pero no equivale al cierre del librarian.
2. **Después: establecer una referencia sonora reproducible.** Construir un protocolo de renders MIDI y A/B entre Prophec-y y **Arturia Prophet-5 V**, que el propietario tiene instalado, sin asumir que emula exactamente un Prophet-6. Desactivar efectos al aislar osciladores/filtros; igualar niveles y registrar parámetros comparables. Evaluar osciladores, filtros, envolventes, articulación, FX y presets musicales completos. Separar medidas objetivas de escuchas ciegas y no convertir la similitud con Arturia en el único objetivo.
3. **Auditoría específica de Tube (DSPark) — nueva tarea prioritaria de calidad sonora.** La implementación actual usa `DSPark TubePreamp` (triodo 12AX7 / Koren, dinámica de cátodo y sag, tone stack tipo Bassman), fijado a **una etapa y 2×** de sobremuestreo; `plugin/CharacterFx.cpp` entrega audio al núcleo mediante `processBlock` con un frame estéreo por llamada. Hay evidencia técnica previa favorable, pero **no** validación auditiva profesional ni paridad con válvula física.
   - **Arquitectura/CPU:** crear prototipo experimental que agrupe el procesamiento en bloques, manteniendo semántica de estado, latencia, smoothing y wet/dry; comparar contra la implementación actual mediante nulos/regresiones (sin asumir equivalencia exacta de antemano). Medir CPU media/p99/máximo en 44,1/48/96 kHz y bloques 32/64/128, integrado con el motor/FX y en DAW real, no solo un kernel aislado.
   - **Espacio de diseño:** comparar **una vs. dos etapas** del mismo modelo y la respuesta de Drive, Sag y Bass/Middle/Treble; no declarar que dos etapas son mejores sin pruebas. Mantener una etapa como baseline. Considerar simplificaciones físicas (grid conduction/blocking distortion, carga entre etapas, corrección de tono) solo si se demuestra relevancia audible.
   - **Ganancia y controles:** revisar el nivel por defecto (`Output=-24+36*0.5=-6 dB`) y el caso documentado de Tube default **−6,16 dB RMS frente a dry** para una frase específica. Establecer ganancia de unidad/compensación auditable, no normalizar silenciosamente presets o compatibilidad. A/B a nivel perceptual pareado además de conservar renders originales.
   - **Calidad medible:** pruebas de THD y distribución armónica vs Drive/nivel, intermodulación, aliasing frente a referencia de mayor frecuencia de muestreo, respuesta a transitorios/dinámica/Sag, rango de controles, DC y ausencia de clics al automatizar Drive/Mix/bypass/Start-End. Documentar costo y trade-offs de 2× frente a opciones superiores de oversampling sin asumir necesidad de aumentarlo.
   - **Calidad musical:** escuchar a ciegas con presets de bajos, leads suaves y agresivos, pads, acordes polifónicos y notas con distinta intensidad; distinguir cambios útiles de mejoras aparentes por volumen. Comparar con saturadores reconocidos en condiciones razonablemente emparejadas, sin exigir clonación tonal exacta.
   - **Decisión de salida:** **mantener, optimizar o sustituir** DSPark únicamente según resultados musicales y de ingeniería. Conservar ID de parámetros, estado, automatización, latencia reportada y bypass; cualquier cambio audible debe ser explícito y acompañado de fixtures/compatibilidad.
4. **Luego: aplicar el mismo estándar a los demás Character FX** (Plasma, Optical Spring, Ferrofluid, Neon); no aumentar la colección solo por atractivo visual antes de validar su calidad sonora y uso real.

**Criterio de aceptación del hito Tube:** informe trazable con commit, parámetros, renders originales y nivelados, métricas reproducibles, comparaciones auditivas y decisión argumentada; pruebas de estabilidad y costo en host real. Los tests anteriores de finitud, sanitizer, estado y CI se conservan, pero **no sustituyen** una validación acústica ni musical.

Referencias internas: [prototipo Tube](V0_20_TUBE_PROTOTYPE.md), [comparación de cinco núcleos](V0_20_FX_COMPARISON.md), [auditoría global](AUDITORIA_GLOBAL_2026_11_02.md), [estado de desarrollo](V0_20_CURRENT_STATUS.md) y [Character implementados](V0_20_CUSTOM_CHARACTER.md).

---

Prioridad acordada: audicionar los cuatro nuevos módulos Character — Plasma,
Optical Spring, Ferrofluid y Neon Wavefolder — antes de retomar MIDI y Studio.
Los cuatro prototipos nativos ya tienen DSP, controles/estado y ventana común;
ver [alcance actual](V0_20_CUSTOM_CHARACTER.md). El cierre de v0.20 sigue pendiente.


Estado: **PROTOTIPO ACTIVO EN IMPLEMENTACIÓN**, 1 de noviembre de 2026, Santiago.

Dirección acordada: vistas **Module + Studio** sobre el módulo contemporáneo de aluminio. Module ya incorpora la organización principal del sinte, Performance, selección visible de FX A/B y la bahía Character FX. La colección visual corresponde exclusivamente a efectos propios Prophec-y, empezando por Tube; los efectos normales conservan edición convencional. Ver `V0_20_CHARACTER_CAROUSEL.md` para el carrusel Inicio/Final y `V0_20_FX_COMPARISON.md` para los núcleos ensayados. La válvula DSPark ya está integrada como primer prototipo Character y dispone de telemetría reactiva; ver `V0_20_TUBE_PROTOTYPE.md` y `V0_20_REACTIVE_TUBE.md`. Studio conserva por ahora el editor genérico completo. El estado vivo del branch, incluyendo pitch bend, valores semánticos y diagnóstico de headroom, se mantiene en `V0_20_CURRENT_STATUS.md`.

## Dirección recomendada

Un instrumento contemporáneo con organización clásica. Conservar la lectura inmediata de una superficie de síntesis: controles esenciales visibles, agrupación estable y señal de izquierda a derecha. Darle identidad propia mediante proporciones, tipografía y color, con edición y navegación adecuadas a un plugin.

| Dirección | Aporte | Limitación |
|---|---|---|
| Réplica clásica | Familiaridad inmediata; carácter de instrumento | Madera, tornillos y escala física pueden ocupar espacio sin ayudar a editar; reproduce las limitaciones del panel |
| Moderna y abstracta | Legibilidad, presets y trabajo con mouse/teclado | Si reparte todo en pestañas pierde visión de conjunto y sensación de instrumento |
| **Híbrida, propuesta** | Panel estable, controles táctiles visualmente sobrios, edición precisa y ayudas contextuales | Exige jerarquizar para evitar densidad y decoración excesiva |
| Giro experimental completo | Identidad muy distintiva | Demasiadas decisiones de interacción nuevas junto con el primer editor |

El boceto de apariencia es exploratorio: sus valores no constituyen un preset real ni son mediciones. La implementación parte del contrato completo de parámetros y no de convertir cada píxel del boceto en un control.

La novedad propuesta es el **foco de sección**: seleccionar un módulo amplía su edición en un área contextual y muestra sólo las rutas que lo afectan. No sustituye la arquitectura de Poly Mod por una matriz arbitraria ni crea destinos nuevos.

## Lenguaje visual inicial

- Dirección actual: panel continuo de aluminio mate claro, controles oscuros, serigrafía legible y luz cálida contenida. Las agrupaciones se construyen con alineación/espacio y jerarquía de controles; el mock grafito anterior queda como exploración histórica. Estados reconocibles por forma/texto además de color.
- Perillas claras, marcas legibles y algo de relieve para transmitir manipulación. Textura y sombras discretas; sin necesidad de madera fotográfica.
- Una tipografía de interfaz legible y números de ancho estable. Preset y valores deben poder leerse a tamaño normal.
- Color por función con moderación: una indicación consistente de fuente/destino, no un tablero multicolor.
- Envelope miniatures y previews de forma como ayudas de edición. Deben representar el modelo/control correspondiente y no presentarse como osciloscopios medidos.
- Animaciones discretas, con modo de movimiento reducido y desactivación. La bahía Prophec-y debe prever varios dispositivos desde el diseño. Tube es el primer prototipo funcional de esta familia; los FX normales no necesitan una representación visual especial.

## Superficie y navegación

La revisión nativa actual usa 1460×700 sin bandeja y 1460×930 con Tube desplegado, ancho mínimo 1240. Mantener texto y áreas de interacción legibles en lugar de reducir una imagen completa; ver `V0_20_ALUMINUM_PANEL.md`.

1. **Cabecera permanente:** nombre/número de preset, anterior/siguiente, browser, guardar/guardar como, Init, Compare, A/B, undo/redo y acceso a ajustes. Estado modificado visible. Parte de este flujo sigue pendiente.
2. **Vista Module:** Osc1/Osc2 y mixer a la izquierda; HP/LP y ADSR en el centro; Poly Mod/LFO a la derecha. Performance (Unison/key mode/glide/Slop/pan) está integrada en la superficie. Las dos envolventes deben poder compararse visualmente.
3. **Vista Studio / foco contextual:** valores ampliados, mapeo de fuentes/destinos y ayuda breve del módulo elegido, sin tapar el control activo. Preview de curvas estáticas y movimiento opcional sólo donde exista telemetría adecuada. La implementación actual de Studio sigue siendo el editor genérico y debe reemplazarse.
4. **FX normales y Prophec-y:** acceso directo a On/Off, tipos A/B y mixes desde Module; edición convencional de los tipos normales. Bahía independiente y ampliable para dispositivos Prophec-y, mediante carrusel de un efecto activo inicialmente, módulo visual completo por tipo y posición Inicio/Final de toda la cadena normal. Ninguno oculta el cuerpo y elimina el efecto; contraer mantiene el audio. Studio amplía el mismo dispositivo. No incrustar sólo la válvula como si fuera el único efecto futuro.
5. **Browser:** búsqueda por nombre/número, bancos, favoritos y etiquetas propias. Metadatos del plugin separados del payload Prophet original. Import/export de programa y banco SysEx mediante las APIs existentes.
6. **Sequence:** editor de las notas/velocidades verificadas. Bits opacos y modos no comprobados se conservan; no se inventan botones de tie ni semánticas de firmware. Arp con sus opciones funcionales ya establecidas.
7. **Ajustes:** oversampling y su aplicación al preparar/reiniciar audio, clock source, perfil de calibración y modo Slop. No esconder controles musicales esenciales aquí.

Module mantiene la lectura de instrumento; Studio debe abrir edición completa del mismo estado, sin reiniciar el sonido al cambiar de vista. FX y Sequence usan espacio adicional cuando se editan en profundidad.

## Contrato de controles

- Reutilizar IDs del host y dominios raw; no cambiar sonido, normalización o formato de sesión por diseñar knobs nuevos.
- Drag vertical/lineal, ajuste fino con modificador, doble clic al valor Init documentado y entrada numérica. Gestos de automatización con begin/end, sin escribir al host por cada refresco visual.
- Los controles bipolares muestran centro y signo. Los booleanos y enums deben ser reconocibles, con etiquetas de sus estados.
- Mostrar tiempos/frecuencias nominales calculados por el perfil cuando ayuden; conservar acceso al raw. Esta capa semántica ya comenzó en Module. No presentar esas cifras como mediciones del hardware ni dibujar el cutoff modulado como valor único si varias voces difieren.
- En destinos modulados, indicador secundario discreto y acotado; nunca mover el valor base del knob en respuesta a la modulación.
- Compare/A-B y undo deben respetar el contrato de estado y orden host/MIDI existente. Definir antes si almacenan sólo patch o también configuración; no cambiar clock/transporte/oversampling accidentalmente.
- Controles no implementados u opacos no deben parecer operativos. Mantener editor genérico como herramienta de diagnóstico hasta que Studio custom lo reemplace sin perder cobertura.
- Nombres accesibles, foco visible, navegación de teclado, contraste suficiente y zoom/DPI; no depender sólo de color o hover.

## Display contextual

El display central no debe inventar telemetría. La visualización genérica actual de posición de control es transitoria. La excepción ya implementada es el diagnóstico de pitch bend, que muestra el valor MIDI 14-bit realmente recibido (`RAW14`) y su desplazamiento en semitonos.

Primera entrega implementada: OSC 1/2 nominal y ADSR del modelo, con selección por ratón/teclado y curvas cacheadas. Ver `V0_20_CONTEXT_DISPLAY.md`. Filtro, LFO, FX y medidores siguen pendientes.

Dirección completa:

- oscilador: preview de waveform/morph basado en el control real;
- filtro: respuesta/control coherente con cutoff/resonancia;
- envolvente: curva ADSR;
- FX: visualización de parámetros pertinentes;
- Performance/Mixer: estado o nivel real, no una curva decorativa.

## Headroom y Master Output

El diagnóstico de clipping de v0.20 mostró que la suma de voces puede superar 0 dBFS tanto en polifonía normal como en Unison. No se añadirá una normalización automática por número de voces sólo para resolver el síntoma. El diseño UI debe prever un **Master Output global**, separado del Program Volume canónico, junto con medidor de pico/clip. Ver `V0_20_CURRENT_STATUS.md` para cifras y decisión técnica.

## Implementación

Editor nativo JUCE sobre el wrapper existente. No introducir un navegador embebido o un puente web para la primera versión del VST.

1. Componentes de controles y tokens visuales compartidos; layout adaptable y escalado DPI.
2. Bindings sobre APVTS/contrato de raw actual. El hilo UI consume estado y mensajes; no toma locks de audio ni recalcula síntesis.
3. Telemetría opcional y acotada: resumen por bloque/voz publicado sin asignaciones; UI a 30 fps con pausa al cerrar/ocultar. No FFT nueva para decoración.
4. Persistencia de preferencias visuales separada del SysEx canónico, manteniendo compatibilidad de sesiones anteriores.
5. Medir costo UI, host automation y abrir/cerrar editor durante reproducción; las huellas de audio deben conservarse cuando no hay edición.

## Etapas y secuencia integrada

Integración del plan previo con la revisión de producto del 1 de noviembre de 2026. Se conserva el alcance de 0.20a–e; se explicitan dependencias y se divide 0.20b para que el Inspector y las visualizaciones se apoyen en un contrato de parámetros común. La revisión técnica que justifica este orden está en [V0_20_PRODUCT_REVIEW.md](V0_20_PRODUCT_REVIEW.md). Los elementos siguientes son trabajo pendiente salvo donde se indica implementación existente.

Module de aluminio, Performance, FX A/B y Tube se conservan como base implementada. El diseño final sigue pendiente de revisión visual; esa revisión no impide avanzar en compatibilidad, edición o diagnóstico. No se reinicia el diseño ni se descartan los avances actuales.

| Orden | Encaje en el plan previo | Trabajo | Criterio de salida |
|---|---|---|---|
| 0 | Protección transversal de 0.20b–e | Congelar un checkpoint trazable y recuperar fixtures genuinos de estados v0.17–v0.20 antes de refactorizar o añadir configuración persistente | Los estados históricos cargan y conservan el estado musical, bytes opacos y valores del wrapper; defaults y migraciones nuevos quedan explícitos |
| 1 | 0.20a y base de 0.20b | Consolidar la superficie existente y extraer gradualmente controles, display, Character y edición contextual de PluginEditor | Mismas funciones y estado al cambiar de componente/vista; sin una reescritura general ni un framework multi-sinte |
| 2 | 0.20b.1 — contrato común | Descriptor/formateador/parseo compartido con el modelo efectivo; ProphecyParameterControl con edición exacta, reset, fine edit, raw/NRPN, ayuda, foco y teclado | Formato y DSP coinciden bajo los perfiles y modos disponibles; los gestos host y límites raw se mantienen |
| 3 | 0.20b.2 — salida y MIDI | Master final independiente de Program Volume, medidores L/R y diagnóstico por etapas; selector Omni/1–16 y monitor por canal | Ganancia histórica preservada al migrar; headroom medido; pitch bend/CC/NRPN, sustain y note-offs comprobados por canal sin notas atascadas |
| 4 | 0.20c — presets | Browser local con búsqueda, banco/programa, etiquetas/categorías propias, favoritos, import/export SysEx, guardar/guardar como e indicador de edición | Cargar/guardar/reabrir y cancelar conservan el estado; import/export lossless; protección de cambios sin guardar; Init, Compare/A-B y undo/redo con alcance definido |
| 5 | 0.20b.3 — Studio y display | Reemplazar el genérico por Inspector del módulo seleccionado; compartir la selección con el OLED y añadir vistas OSC/ENV/LFO/FILTER/OUTPUT/FX por entregas pequeñas | Valores efectivos y relaciones reales; previews identificados como tales; filtro nominal distinguido de medición; modulación indicada sin desplazar el knob base |
| 6 | 0.20d — FX y secuencia | Completar edición específica de A/B, relación clara con Character Inicio/Final y edición de notas/velocidades verificadas de Sequence | Sin funciones ficticias ni reinterpretación de bits opacos; tipos/mixes actuales conservados; Tube es el único Character necesario para cerrar esta versión |
| 7 | 0.20e — cierre y RC1 | Congelar funciones, revisar coherencia visual, DPI/resizing, accesibilidad básica, reduced motion, costo UI y pruebas de uso en DAW; publicar binarios trazables | Gates Windows/Linux, pluginval, sanitizers y regresiones aprobados; carga/reapertura de proyectos, automatización y MIDI comprobados en una matriz mínima de hosts documentada |

**Avance:** la base común de controles y fixtures de los encoders históricos ya está implementada; las sesiones DAW históricas reales siguen pendientes. Por solicitud del propietario se adelanta una entrega del punto 5: OLED contextual con OSC/ADSR del modelo. Master Output y medidores L/R ya están implementados, con -18 dB para instancias nuevas y migración unitaria de sesiones anteriores (codec v3). El propietario pospuso MIDI por canal/monitor. Ya está implementado el browser local de primera entrega (ver `V0_20_PRESET_BROWSER.md`); Compare/A-B/undo y la asociación de biblioteca en sesiones DAW siguen pendientes. El siguiente bloque principal es Studio/Inspector. Ver `V0_20_MASTER_OUTPUT.md`.

### Decisiones que atraviesan todas las etapas

- Accesibilidad, foco, ayuda, teclado y reduced motion forman parte de cada widget desde su incorporación; 0.20e los revisa de conjunto.
- La compatibilidad se comprueba con cada cambio persistente, no únicamente al final. Un nuevo Master o selector MIDI no altera silenciosamente sesiones anteriores.
- Descriptor/formateador, Inspector y display comparten datos efectivos. No se vuelven a implementar leyes de tiempo/frecuencia por separado en cada vista.
- Compare/A-B de patches es independiente de los slots de efectos A/B. Su alcance debe especificar patch, Character y controles globales, conservando clock/transporte/oversampling según el contrato acordado.
- Cada entrega deja commit, estado documentado, gates y preview descargable cuando corresponda. Una compilación de desarrollo no implica cierre del hito ni aprobación visual.
- 0.20 RC1 incluye pruebas manuales mínimas de uso real. v0.21 conserva la matriz amplia de hosts, soak de reloj/voice stealing, carga/CPU en máquina dedicada y endurecimiento del empaquetado.

### Alcance conservado y pospuesto

Se mantienen en v0.20 Module, Studio/Inspector, browser local, SysEx, Init/Compare/A-B/undo, display veraz, edición FX/secuencia verificada y accesibilidad básica. La nueva revisión cambia el orden y los criterios de salida; no elimina esas funciones del plan original.

Quedan para trabajo posterior la conexión librarian bidireccional con hardware, nube de presets, nuevos Character FX, una vista Performance independiente y un framework para otros sintetizadores. Los controles Performance existentes permanecen en Module. El backlog acústico de v0.19 sigue abierto por separado y no se declara resuelto por terminar la interfaz.

## Fuera de alcance inicial

No rediseñar el DSP Prophet validado, ampliar polifonía, inventar una matriz de modulación, resolver por apariencia los modos de secuencia desconocidos ni implementar todos los conceptos de FX animados. La colección propia es una extensión opcional separada, con bahía común y primer prototipo Tube ya integrado. Cualquier cambio de gain staging/headroom debe tratarse como una decisión explícita y medible, no como una corrección visual o una normalización silenciosa.
