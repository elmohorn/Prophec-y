# v0.20 — Interfaz y flujo de presets

Estado: **PROPUESTA PARA REVISIÓN**, 1 de noviembre de2026, Santiago.
La elección visual final se hará con el usuario. Este documento inicia la planificación; no declara implementado el editor.

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

- Panel grafito con capas de contraste sutil, texto marfil y acentos ámbar/cobre. Estados activos reconocibles por texto/forma además del color.
- Perillas claras, marcas legibles y algo de relieve para transmitir manipulación. Textura y sombras discretas; sin necesidad de madera fotográfica.
- Una tipografía de interfaz legible y números de ancho estable. Preset y valores deben poder leerse a tamaño normal.
- Color por función con moderación: una indicación consistente de fuente/destino, no un tablero multicolor.
- Envelope miniatures y previews de forma como ayudas de edición. Deben representar el modelo/control correspondiente y no presentarse como osciloscopios medidos.
- Animaciones discretas, con modo de movimiento reducido y desactivación. Visuales físicos de FX quedan para exploración posterior, no condicionan la primera UI funcional.

## Superficie y navegación

Ventana inicial orientativa1280×820, redimensionable. Layout compacto alrededor de1040×720 mediante reorganización; mantener texto y áreas de interacción legibles en lugar de reducir una imagen completa.

1. **Cabecera permanente:** nombre/número de preset, anterior/siguiente, browser, guardar/guardar como, Init, Compare, A/B, undo/redo y acceso a ajustes. Estado modificado visible.
2. **Vista Synth:** Osc1/Osc2 y mixer a la izquierda; HP/LP y ADSR en el centro; Poly Mod/LFO a la derecha. Performance (Unison/key mode/glide/Slop/pan) en una franja clara. Las dos envolventes deben poder compararse visualmente.
3. **Foco contextual:** valores ampliados, mapeo de fuentes/destinos y ayuda breve del módulo elegido, sin tapar el control activo. Preview de curvas estáticas y movimiento opcional sólo donde exista telemetría adecuada.
4. **FX:** acceso directo a On/Off, tipos A/B y mixes desde Synth; panel ampliable con parámetros reales de cada tipo y orden A→B visible.
5. **Browser:** búsqueda por nombre/número, bancos, favoritos y etiquetas propias. Metadatos del plugin separados del payload Prophet original. Import/export de programa y banco SysEx mediante las APIs existentes.
6. **Sequence:** editor de las notas/velocidades verificadas. Bits opacos y modos no comprobados se conservan; no se inventan botones de tie ni semánticas de firmware. Arp con sus opciones funcionales ya establecidas.
7. **Ajustes:** oversampling y su aplicación al preparar/reiniciar audio, clock source, perfil de calibración y modo Slop. No esconder controles musicales esenciales aquí.

No es una colección de páginas aisladas: Synth mantiene una vista suficiente para programar sonido sin navegar continuamente. FX y Sequence usan espacio adicional porque lo necesitan.

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
|0.20a — concepto | Dos propuestas del mismo panel y misma lista de controles, con opción clásica e híbrida | Elección visual con el usuario; claridad y densidad revisadas a tamaño real |
|0.20b — editor Synth | Todos los controles de síntesis conectados; entrada precisa, gestos y estado coherente | Tocar/automatizar/restaurar sin discrepancia con el editor genérico |
|0.20c — presets | Browser, SysEx, Init/Compare/A-B y undo/redo con alcance definido | Import/export lossless, guardar/reabrir y protección de cambios no guardados |
|0.20d — FX y secuencia | Edición visible de funciones verificadas; conservación de metadatos opacos | Ningún control decorativo o semántica inventada; orden FX claro |
|0.20e — accesibilidad y host | DPI, resizing, teclado, reduced motion y costo de UI medido | Sin regresión de audio/estado; gates Windows/Linux y pluginval |

## Fuera de alcance inicial

No rediseñar DSP, ampliar polifonía, inventar una matriz de modulación, resolver por apariencia los modos de secuencia desconocidos ni implementar todos los conceptos de FX animados. La identidad puede ser fuerte con una superficie sobria y una buena interacción.
