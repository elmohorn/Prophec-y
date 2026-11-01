# v0.20 — primera entrega de compatibilidad y controles comunes

1 de noviembre de 2026. Base: `16a419e3a072f3baf9beb50a1f96eb76c06d967b`.

## Alcance entregado

Se inicia el bloque de protección + 0.20b.1 del plan integrado. No se declara completada la migración de todos los controles ni Studio/Inspector.

- Fixtures permanentes de formatos v1/v2 emitidos por los codificadores históricos de v0.17–v0.20. Son datos sintéticos con procedencia verificable, no proyectos antiguos capturados en un DAW. Ver `tests/state_versions/README.md` y su manifest.
- Capa de presentación independiente de JUCE en `plugin/ui/ParameterPresentation.*`. Reutiliza el descriptor/dominio canónico `p6::ParameterInfo`, recibe el perfil efectivo y conserva IDs y normalización del host.
- Tiempos de envolvente compartidos por mapper/presentación a través de `mappedEnvelopeTime`; glide compartido y PW mostrado conforme a la curva/clamp existentes. Las seis envolventes se verifican contra `mapPatchToDSP` en Default y Reference A 0.9.
- Corrección del desajuste raw 64: el filtro muestra 908 ms en Default y 320 ms en Reference A 0.9, siguiendo el modelo. Es un tiempo nominal del software, no una medición de hardware ni la duración completa de una nota.
- El formateador se instala después del SliderAttachment, que durante su construcción reemplaza los callbacks del slider. La lectura enfocada se refresca si cambia el perfil sin mover el knob.
- LFO sincronizado deja de mostrar su frecuencia libre como si fuera efectiva: indica SYNC RAW hasta disponer de una semántica de división verificada. Cutoff usa porcentaje de control, sin llamar OPEN al extremo alto de HPF ni inventar Hz medidos.

## Control común: Low Pass y extensión al panel

`ProphecyParameterControl` mantiene Slider/SliderAttachment como vía de edición del host. Low Pass fue el primer caso validado: ambos knobs LP están vinculados al mismo NRPN 45. El patrón se extiende después a las 37 instancias de controles de síntesis del panel (34 IDs distintos, con controles duplicados que comparten parámetro). Incorporan:

- clic derecho o Shift+F10: menú común;
- edición exacta de porcentaje o edición raw independiente, con rechazo de valores inválidos y ajuste al paso hardware más próximo;
- Reset to Init y doble clic, usando el default real del parámetro;
- flechas: un paso raw; Home: reset; Enter: edición exacta;
- Shift al comenzar un drag: menor sensibilidad;
- foco visible, nombre/descripción accesibles y ayuda con raw/NRPN.

Las operaciones de menú/reset/teclado tienen gestos begin/end explícitos; una entrada inválida no cambia el valor ni emite un gesto. No se añaden parámetros ni se modifica el formato de estado. Los controles propios de Tube conservan su edición especializada; switches y selectores conservan sus componentes JUCE. Esta entrega no completa todavía toda la cobertura de Inspector/ayuda específica por parámetro.

## Validación

- 12 suites CTest locales aprobadas, incluidas presentación y compatibilidad histórica.
- 1.536 combinaciones de segmento/raw/perfil comparadas con el mapper efectivo; parseo, límites, unidades y decimal con coma comprobados.
- Carga de fixtures, conservación de los 1024 bytes —incluidos opacos—, extras y controles de secuencia; migración de Character desde v1, rechazo de todas las truncaciones y versión futura sin alterar el estado del llamador.
- Huellas DSP y tablas deterministas de headroom idénticas antes/después. SHA-256 de ambas salidas: `d91d8649fe2ea3302fa317b38f310551ffaaceb6272847f31e6f8091e8295a2f`.
- Smoke nativo ampliado para edición LP, rechazo de texto, reset, teclado, gestos host y formateo de envolvente al cambiar perfil. Metadatos de accesibilidad comprobables sin peer; el handler nativo sólo se prueba si hay una ventana nativa disponible. La validación con lectores de pantalla y DAW reales sigue pendiente.
- Las dos suites nuevas se añaden a los gates Windows/Linux y sanitizers. La preview se publica únicamente tras aprobar la CI completa.

## Siguiente trabajo

Revisar el uso musical de estos controles comunes y ampliar la ayuda específica/Inspector sobre la misma base. Mantener la protección de estados en cada entrega. Master Output/MIDI por canal, browser, Inspector, visualizaciones de modelo, FX/secuencia y RC1 conservan su lugar en el plan; este cambio no los marca completados.
