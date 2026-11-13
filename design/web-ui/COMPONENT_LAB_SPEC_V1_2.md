# Prophec-Y — Component Lab v1.2

**Estado:** prototipo web de componentes, sin audio; no integrado al VST. **Rama `main`:** sin modificar.

## Objetivo y alcance

Conservar la distribución modular aprobada (consola de vidrio ahumado + placas de aluminio oscuro) y completar las familias de controles antes de portar el acabado a JUCE. La muestra v1.2 incluye perillas estándar y Master, faders de OSC 1/OSC 2/SUB/NOISE, OLED/preset, logo, LED de salida, botones capacitivos, pasos de secuenciador/arpegiador y, ahora, tres interruptores activos de perfil bajo, uno deshabilitado, perilla secundaria y selectores FX A/B.

El Component Lab **no es el editor completo**. La selección de parámetros, iluminaciones y forma de onda es una simulación; no se debe inferir correspondencia acústica ni funcionamiento del DSP.

## Componentes y contratos visuales

| Componente | Estados mostrados | Comportamiento esperado |
|---|---|---|
| Perilla estándar | Normal, foco, movimiento | Arrastre vertical/rueda/flechas; LED por valor normalizado en instrumento nativo |
| Perilla secundaria | Normal, movimiento, foco | Misma base visual, menor tamaño. Etiqueta legible en DPI bajo |
| Master | Valor real de diseño -60 a 0 dB | Superficie física SOBRE vidrio; etiqueta separada del anillo de LED |
| Faders | Valor de mezcla, hover, drag | Sombra de contacto suave independiente del gráfico del cap; no oscurecer toda la guía |
| Switches | Apagado, activo, deshabilitado, foco | Perfil bajo; halo rojo localizado solo en activo; deshabilitado sin interacción |
| Botones pasos 1–8 | Apagado/activo, pulsado | Halo rojo breve visualmente contenido en ON; sin sombra flotante |
| Capacitivo | Seleccionado/no seleccionado | Bajo vidrio; únicamente señal luminosa, sin relieve de tecla mecánica |
| Selector de FX | Cerrado, enfocado, opción escogida | Popup con fondo opaco, valores reales de las listas estándar y sin dependencia de ancho de etiqueta |
| LEDs / VU | Segmentos encendidos y apagados | Intensidad máxima aprobada para anillo rotatorio; el último segmento activo sirve de puntero. Meter simulado aquí |

### Identificadores de parámetro de referencia

En la implementación JUCE vigente, los siguientes IDs son usados para estos conceptos. No convertir los valores del HTML en datos canónicos de sonido:

| Control | JUCE / parámetro |
|---|---|
| OSC SYNC | `nrpn_0001` |
| UNISON | `nrpn_0156` |
| GLIDE ON | `nrpn_0029` |
| FX A type | `nrpn_0119` |
| FX B type | `nrpn_0127` |
| MASTER | `p6plugin::kMasterOutputID` (-60 a 0 dB; valor inicial -18 dB) |
| Secuenciador/arpegiador 8 pasos | `p6plugin::kArpStepIDs[0..7]` |

FX ON está **deshabilitado a propósito como prueba visual**, no porque el VST lo tenga deshabilitado. El dial secundario `MOD AMT` no tiene binding definitivo en el prototipo. No existe aún sincronización HTML ↔ APVTS.

## Gramática de materiales

1. **Aluminio:** texturizado de metal fino, no imagen prerenderizada de una placa con tornillos/sombras. Adoptar albedo/base color tileable, normal/roughness cuando sea útil, tintado separado de iluminación y fuente única de luz arriba/izquierda. La versión HTML aún usa una textura derivada del render aprobado; **no** se presenta como textura fotográfica final.
2. **Vidrio ahumado:** capa oscura, brillo/reflejo superficial de bajo contraste, una única fascia. OLED, logo, botones y segmentos deben parecer instalados **debajo** de la superficie; Master y su sombra, **encima**.
3. **Sombras:** una capa de contacto próxima al componente, una capa ambiental amplia y tenue; no duplicar sombras que estén ya horneadas en sprites (defecto identificado en v0.9).
4. **Neón:** núcleo brillante y halo óptico cercano; evitar bloom sobre texto u otros controles; no duplicar el wordmark.
5. **LED:** OFF debe conservar lente/reflejo físico, ON añade emisión; halo mínimo alrededor de pulsadores ON; brillo por nivel real, sin animaciones independientes del audio en el VST.
6. **Botones y selectores:** iconografía y tipografía nítidas a distintas escalas, navegación por teclado, tooltips y valores accesibles.

**Fuentes candidatas CC0 para investigar, no incorporadas aún:** [ambientCG Metal012](https://ambientcg.com/view?id=Metal012) (material PBR cepillado, procedimental), [Poly Haven](https://polyhaven.com/) (biblioteca CC0, incluidos materiales escaneados). Antes de la integración definitiva, elegir texturas apropiadas para paneles *limpios*, guardar archivo de licencia y origen, y evaluar textura orientada horizontalmente, mosaicos, rendimiento y legibilidad.

## Implementación JUCE propuesta

- Mantener los IDs de APVTS, dominio de cada parámetro, codec de estado y SysEx. La capa visual no debe introducir parámetros de audio nuevos ni duplicar sus estados.
- Construir `LookAndFeel`/componentes reutilizables para `HardwareKnob`, `LEDArc`, `MetalFader`, `LowProfileSwitch`, `CapacitiveButton`, `FXSelector`, `OLEDGlass`, `OutputLED`, `StepKey`.
- Usar recursos PNG/WebP optimizados para material base y piezas cuyo realismo necesite bitmap; iconos/textos/ring LEDs en vector. Caches de recursos con 1x/2x apropiados para DPI, sin distribuir fuentes sueltas al usuario.
- Escalar **geometría, texto y áreas de interacción**, no reducir una imagen monolítica; controlar límites en 1280×720 con DAW y a 125–150 % de DPI, además del editor expandido.
- Mantener actualizaciones de vistas y telemetría en thread UI, no generar texturas, filtros de blur ni asignaciones de recursos en hilo de audio. Separar el meter simulado de la futura telemetría DSP real.
- Para Character FX, construir marco común y cargar visualizaciones propias después de que el set básico esté aprobado, sin mostrar previas ópticas como DSP activo.

## Validación ejecutada en navegador (sin audio)

- Viewports: `1280×720`, `1480×780`, `1024×700`; sin errores JS y sin scroll de página en la renderización básica.
- Toggle UNISON: `false → true`, y RESET devuelve `false`.
- Selector FX B: `PLATE → HALL`, y RESET devuelve `PLATE`.
- Dial secundario con flecha: `38 → 40`, y RESET devuelve `38`.
- Geometría a 1280×720: la franja de controles termina antes del arpegiador sin solapes; las etiquetas de faders quedan por encima de la nueva franja.

## Pendientes antes de integración al VST

1. Elegir y preparar **textura fotográfica o PBR de producción** y validar un material bajo una iluminación común.
2. Aprobar los tres elementos nuevos (switch, knob secundario y selector), en tamaño normal y escala reducida.
3. Definir las especificaciones de teclado/foco, contrastes y tamaño mínimo de texto en el editor real.
4. Crear una rama aislada en GitHub y realizar reemplazo incremental de JUCE; no tocar `main` ni DSP hasta revisión visual y pruebas smoke.
5. Ejecutar pruebas de VST3: automatización y readbacks de parámetros, guardado/restauración de estados, performance UI, captura 1x/125 %/150 %, pruebas DAW Windows y Linux y `pluginval`.