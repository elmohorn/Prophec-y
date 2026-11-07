# v0.20 — revisión de panel y válvula reactiva

1 de noviembre de 2026. Continuación de la rama `v0.20-ui` desde `32f56fa4`.

## Panel

Se retoma el render original aportado por el propietario, sin dar por aprobado el prototipo anterior. Se ajustan el ancho del OLED, la distribución del filtro, las filas Poly Mod/LFO, el tamaño de los controles y la franja inferior. Mix A/B vuelven a ser perillas. El control junto al OLED edita el mismo cutoff LP que el control grande; no crea un parámetro ni una ley acústica nueva. Se mantienen valores RAW y el gráfico se identifica como guía de posición.

La válvula utiliza un recurso de vidrio renderizado con transparencia, embebido en el plugin. El filamento y la iluminación se componen en JUCE; no hay vídeo, navegador ni descarga de recursos durante el uso. Procedencia y prompt: `plugin/assets/README.md`.

## Respuesta al audio

- RMS de entrada y salida del insert, con integración exponencial de 20 ms calculada desde la frecuencia de muestreo.
- RMS del cambio entre la salida mezclada y la señal seca compensada por las 71 muestras de latencia.
- La intensidad del filamento depende del nivel; la iluminación interna depende del cambio medido. No se añade parpadeo aleatorio.
- CHANGE no es THD ni una medición exclusiva de saturación: EQ, ganancia y mezcla también contribuyen.
- Bypass apaga la iluminación. Si el host deja de procesar, el editor detecta lecturas sin actualizar y libera los medidores. MOTION pausa la respuesta luminosa sin afectar al audio.

La medición se realiza en el hilo de audio sin asignaciones ni locks. El editor consume valores atómicos independientes a 30 Hz; son indicadores visuales, no una captura sincronizada para análisis. La posición Inicio/Final determina qué señal llega al insert. Las operaciones que producen el audio, IDs, SysEx, formato de estado y latencia fija permanecen iguales.

## Verificación y entrega

Las diez suites CTest locales pasan, incluidas comprobaciones de RMS a 44,1/48/96 kHz, cambio nulo en None, actividad de Tube, bypass, caída a silencio, reset, nulidad exacta del bypass retrasado y ausencia de asignaciones ordinarias en el procesamiento.

El smoke nativo verifica preservación de SysEx, restauración de estado, controles dentro de la ventana a tres anchos y audio finito durante cambios. Genera capturas reales de Module, Tube en bypass, Studio y None, además de doce fotogramas cuyo nivel de entrada cambia por MIDI CC7. La prueba exige que el nivel alto supere al bajo; no se anima la imagen con datos inventados.

La CI conserva Windows/Linux, pluginval 10, ASan+UBSan y guards de análisis. En pushes a esta rama, sólo publica una preview cuando todos los gates pasan. Descarga los ZIP de esa misma ejecución, comprueba commit, procedencia e integridad y publica versiones inmutables con SHA256SUMS. Las capturas nativas también se conservan como evidencia de CI.

El acabado sigue sujeto a revisión visual. Browser/presets, Studio personalizado, preferencias de UI, accesibilidad y pruebas manuales en DAW siguen pendientes. No se declara v0.20 cerrada ni paridad acústica completa.
