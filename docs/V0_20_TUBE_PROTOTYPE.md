# v0.20 — válvula y primer editor nativo

Prototipo funcional, 1 de noviembre de 2026. La revisión visual posterior está en `V0_20_ALUMINUM_PANEL.md`; la lista inicial de diez controles de este documento describe el primer prototipo. No es el cierre de v0.20 ni el diseño final del mock.

## Implementación

- Editor JUCE nativo de aluminio claro: Module con diez controles esenciales de síntesis y módulo Tube; Studio conserva el editor genérico completo para que ningún parámetro quede inaccesible durante esta etapa.
- Carrusel inicial con None y Tube. El catálogo crecerá agregando IDs; no se ofrecen dispositivos futuros como si funcionaran. Navegación anterior/siguiente y selector de acceso directo.
- Drive −12/+36 dB, Sag, Bass/Middle/Treble, Mix y Output −24/+12 dB. El tone stack es parte del carácter DSPark, no un EQ del Prophet original. Una etapa y oversampling interno 2x fijos.
- Inicio inserta después de program/MIDI volume y antes de distorsión, FX A y FX B. Final inserta después de FX B. El motor usa un callback opcional no propietario; sin callback mantiene su camino previo.
- None suspende el procesamiento de válvula, oculta el cuerpo y reduce la altura de Module. Bypass conserva selección/controles. Collapse oculta los controles y mantiene el efecto; Animation sólo cambia la luz.
- El brillo usa una envolvente de amplitud de entrada publicada por bloque; es una visualización de excitación, no una medición de temperatura ni una simulación óptica del filamento.

## Latencia y transiciones

El wrapper declara **71 muestras fijas** desde `prepareToPlay`, incluso con None/bypass. Son 1,48 ms a 48 kHz. Esto evita cambiar PDC durante automatización o navegación. None usa un retraso puro al final de la cadena: comparación exacta contra el motor original desplazada 71 muestras. El motor sin wrapper no adquiere ese retraso.

La válvula se construye/prepara una vez fuera de la callback; seleccionar un tipo no reserva memoria. El dry/wet usa la misma latencia. Selección y bypass usan una rampa de mezcla de 10 ms cuando conservan la ruta. Un cambio efectivo de posición baja la salida completa durante 10 ms, intercambia/reset a cero, espera la latencia y recupera volumen en 10 ms. **Puede oírse una breve caída de volumen**: es una política explícita de prototipo, no un hot swap perceptualmente transparente. Cambiar de una válvula inicial a None/bypass requiere pasar a la ruta final de retraso puro.

Las colas de los FX normales siguen procesándose; no se implementó un sistema de colas para futuros dispositivos. Sólo hay una instancia Tube y un retraso fijo acotado. No se acumulan procesadores al navegar.

## Estado y compatibilidad

IDs de host propios por dispositivo, añadidos después de los parámetros existentes. El estado binario pasa a versión 2: extensión de selección, posición, bypass y siete controles normalizados con precisión 0,001. Sigue leyendo versión 1 con None y valores iniciales. Se valida la extensión antes de reemplazar estado. El payload Prophet de 1024 bytes conserva su identidad y el SysEx exportado no incluye los efectos propios.

Los controles Tube se conservan al seleccionar None. El cierre del editor no afecta al audio. Collapse/Animation son preferencias de la ventana en esta primera implementación; todavía no persisten entre aperturas. El diseño Studio personalizado, browser, compare/undo, teclado de notas y el resto del inventario Module siguen pendientes.

## Dependencia y compilación

`third_party/DSPark` contiene sólo las 13 cabeceras transitivas de TubePreamp y su licencia MIT, sin modificaciones. Commit y SHA-256 de archivos en `UPSTREAM.json`. No se enlaza DaisySP ni los demás candidatos. `p6character` separa el adaptador del núcleo del instrumento.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DP6_BUILD_PLUGIN=ON -DP6_BUILD_UI_SMOKE=ON
cmake --build build --target P6CorePlugin_VST3 P6UiSmoke p6_character_tests p6_plugin_state_tests --parallel 2
# Linux, con X disponible:
xvfb-run -a ./build/plugin/P6UiSmoke_artefacts/Release/P6UiSmoke /tmp/prophec-ui
```

La prueba de editor puede producir PNG del propio componente sin abrir una ventana de escritorio. La CI configura el smoke con Xvfb en Linux y ejecución nativa en Windows. Esto no sustituye tocar el plugin en un DAW ni revisar accesibilidad completa.

## Validación

Los resultados finales de esta implementación se registran en `calibration/v020_tube_prototype/validation.json`. La prueba del adaptador cubre tres sample rates, None con retraso exacto, separación estéreo, cambios repetidos de posición/drive, retorno a None y contador de `new` ordinario sin asignaciones durante proceso. Este contador no certifica todos los malloc, locks ni tiempos máximos de audio.

Las cinco huellas del motor sin inserto se comparan contra el cierre v0.19. Las pruebas de estado cubren extensión v2, recuperación v1, payload intacto y rechazo de tipos inválidos/truncado. El smoke de JUCE comprueba restauración de controles, SysEx idéntico, audio finito durante cambios y reapertura de editor. Las pruebas de sanitizer se ejecutan con LeakSanitizer desactivado por la limitación ptrace ya documentada; no se afirma ausencia de leaks.
