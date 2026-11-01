# v0.20 — Interfaz y flujo de presets

Estado: **PROPUESTA PARA REVISIÓN**, 1 de noviembre de2026, Santiago.
Actualización acordada: vistas **Module + Studio**, con Performance para más adelante. El usuario favorece el módulo contemporáneo de aluminio del segundo mock. La colección visual corresponde exclusivamente a efectos propios Prophec-y, empezando por válvula; los efectos normales siguen convencionales. Ver `V0_20_CHARACTER_FX_RESEARCH.md` para la bahía ampliable y fuentes DSP. El diseño final de controles y la implementación todavía están pendientes.

## Dirección recomendada

Un instrumento contemporáneo con organización clásica. Conservar la lectura inmediata de una superficie de síntesis: controles esenciales visibles, agrupación estable y señal de izquierda a derecha. Darle identidad propia mediante proporciones, tipografía y color, con edición y navegación adecuadas a un plugin.

| Dirección | Aporte | Limitación |
|---|---|---|
| Réplica clásica | Familiaridad inmediata; carácter de instrumento | Madera, tornillos y escala física pueden ocupar espacio sin ayudar a editar; reproduce las limitaciones del panel |
| Moderna y abstracta | Legibilidad, presets y trabajo con mouse/teclado | Si reparte todo en pestañas pierde visión de conjunto y sensación de instrumento |
| **Híbrida, propuesta** | Panel estable, controles táctiles visualmente sobrios, edición precisa y ayudas contextuales | Exige jerarquizar para evitar densidad y decoración excesiva |
| Giro experimental completo | Identidad muy distintiva | Demasiadas decisiones de interacción nuevas junto con el primer editor |

El boceto de apariencia es exploratorio: todavía hay que distribuir Sync, fine tune, velocity, aftertouch y otros controles del inventario. Sus valores no constituyen un preset real ni son mediciones. La implementación partirá del contrato completo de parámetros, no de convertir cada píxel del boceto en un control.

La novedad propuesta es el **foco de sección**: seleccionar un módulo amplía su edición en un área contextual y muestra sólo las rutas que lo afectan. No sustituye la arquitectura de Poly Mod por una matriz arbitraria ni crea destinos nuevos.

## Lenguaje visual inicial

- Dirección actual: panel continuo de aluminio mate claro, controles oscuros, serigrafía legible y luz cálida contenida. Las agrupaciones se construyen con alineación/espacio y jerarquía de controles; el mock grafito anterior queda como exploración histórica. Estados reconocibles por forma/texto además de color.
- Perillas claras, marcas legibles y algo de relieve para transmitir manipulación. Textura y sombras discretas; sin necesidad de madera fotográfica.
- Una tipografía de interfaz legible y números de ancho estable. Preset y valores deben poder leerse a tamaño normal.
- Color por función con moderación: una indicación consistente de fuente/destino, no un tablero multicolor.
- Envelope miniatures y previews de forma como ayudas de edición. Deben representar el modelo/control correspondiente y no presentarse como osciloscopios medidos.
- Animaciones discretas, con modo de movimiento reducido y desactivación. La bahía Prophec-y debe prever varios dispositivos desde el diseño. Válvula será el primer prototipo de esta familia; los FX normales no necesitan una representación visual especial.

## Superficie y navegación

Ventana inicial orientativa1280×820, redimensionable. Layout compacto alrededor de1040×720 mediante reorganización; mantener texto y áreas de interacción legibles en lugar de reducir una imagen completa.

1. **Cabecera permanente:** nombre/número de preset, anterior/siguiente, browser, guardar/guardar como, Init, Compare, A/B, undo/redo y acceso a ajustes. Estado modificado visible.
2. **Vista Module:** Osc1/Osc2 y mixer a la izquierda; HP/LP y ADSR en el centro; Poly Mod/LFO a la derecha. Performance (Unison/key mode/glide/Slop/pan) en una franja clara. Las dos envolventes deben poder compararse visualmente.
3. **Vista Studio / foco contextual:** valores ampliados, mapeo de fuentes/destinos y ayuda breve del módulo elegido, sin tapar el control activo. Preview de curvas estáticas y movimiento opcional sólo donde exista telemetría adecuada.
4. **FX normales y Prophec-y:** acceso directo a On/Off, tipos A/B y mixes desde Module; edición convencional de los tipos normales. Bahía independiente y ampliable para dispositivos Prophec-y, con miniaturas/estados en Module y dispositivo seleccionado ampliado en Studio. No incrustar sólo la válvula como si fuera el único efecto futuro.
5. **Browser:** búsqueda por nombre/número, bancos, favoritos y etiquetas propias. Metadatos del plugin separados del payload Prophet original. Import/export de programa y banco SysEx mediante las APIs existentes.
6. **Sequence:** editor de las notas/velocidades verificadas. Bits opacos y modos no comprobados se conservan; no se inventan botones de tie ni semánticas de firmware. Arp con sus opciones funcionales ya establecidas.
7. **Ajustes:** oversampling y su aplicación al preparar/reiniciar audio, clock source, perfil de calibración y modo Slop. No esconder controles musicales esenciales aquí.

Module mantiene la lectura de instrumento; Studio abre edición completa del mismo estado, sin reiniciar el sonido al cambiar de vista. FX y Sequence usan espacio adicional cuando se editan en profundidad. Performance queda explícitamente fuera de la primera etapa.

## Contrato de controles

- Reutilizar IDs del host y dominios raw; no cambiar sonido, normalización o formato de sesión por diseñar knobs nuevos.
- Drag vertical/lineal, ajuste fino con modificador, doble clic al valor Init documentado y entrada numérica. Gestos de automatización con begin/end, sin escribir al host por cada refresco visual.
- Los controles bipolares muestran centro y signo. Los booleanos y enums deben ser reconocibles, con etiquetas de sus estados.
- Mostrar tiempos/frecuencias nominales calculados por el perfil cuando ayuden; conservar acceso al raw. No presentar esas cifras como mediciones del hardware ni dibujar el cutoff modulado como valor único si varias voces difieren.
- En destinos modulados, indicador secundario discreto y acotado; nunca mover el valor base del knob en respuesta a la modulación.
- Compare/A-B y undo deben respetar el contrato de estado y orden host/MIDI existente. Definir antes si almacenan sólo patch o también configuración; no cambiar clock/transporte/oversampling accidentalmente.
- Controles no implementados u opacos no deben parecer operativos. Mantener editor genérico como herramienta de diagnóstico.
- Nombres accesibles, foco visible, navegación de teclado, contraste suficiente y zoom/DPI; no depender sólo de color o hover.

## Implementación

Editor nativo JUCE sobre el wrapper existente. No introducir un navegador embebido o un puente web para la primera versión del VST.

1. Componentes de controles y tokens visuales compartidos; layout adaptable y escalado DPI.
2. Bindings sobre APVTS/contrato de raw actual. El hilo UI consume estado y mensajes; no toma locks de audio ni recalcula síntesis.
3. Telemetría opcional y acotada: resumen por bloque/voz publicado sin asignaciones; UI a30fps con pausa al cerrar/ocultar. No FFT nueva para decoración.
4. Persistencia de preferencias visuales separada del SysEx canónico, manteniendo compatibilidad de sesiones anteriores.
5. Medir costo UI, host automation y abrir/cerrar editor durante reproducción; las huellas de audio deben conservarse cuando no hay edición.

## Etapas

| Etapa | Entregable | Aceptación |
|---|---|---|
|0.20a — concepto | Module de aluminio + Studio y bahía de varios dispositivos, sobre un inventario común de controles | Densidad revisada a tamaño real y colección ampliable; sólo válvula será el primer efecto funcional |
|0.20b — Module + Studio | Todos los controles de síntesis conectados; entrada precisa, gestos y estado coherente | Tocar/automatizar/restaurar sin discrepancia con el editor genérico |
|0.20c — presets | Browser, SysEx, Init/Compare/A-B y undo/redo con alcance definido | Import/export lossless, guardar/reabrir y protección de cambios no guardados |
|0.20d — FX y secuencia | Edición visible de funciones verificadas; conservación de metadatos opacos | Ningún control decorativo o semántica inventada; orden FX claro |
|0.20e — accesibilidad y host | DPI, resizing, teclado, reduced motion y costo de UI medido | Sin regresión de audio/estado; gates Windows/Linux y pluginval |

## Fuera de alcance inicial

No rediseñar el DSP Prophet validado, ampliar polifonía, inventar una matriz de modulación, resolver por apariencia los modos de secuencia desconocidos ni implementar todos los conceptos de FX animados. La colección propia será una extensión opcional separada, con bahía común y primer prototipo de válvula; su adopción DSP requiere la evaluación descrita en el informe de fuentes. La identidad puede ser fuerte con una superficie sobria y una buena interacción.
