# v0.20 — panel de aluminio basado en el mock aprobado

1 de noviembre de 2026. Revisión visual del editor nativo; sin cambios de DSP, IDs del host, serialización ni política de latencia.

## Referencia y traducción a controles reales

Se revisaron las dos imágenes de concepto de aluminio del 1 de noviembre: «Sintetizador de aluminio de seis voces» y «PROPHEC-Y: módulo sintetizador de escritorio». No se usa el bitmap como fondo con controles dibujados encima. El panel se construye con geometría JUCE, material de grano determinista generado al abrir el editor y componentes conectados al APVTS.

- Superficie continua de aluminio claro, borde oscuro y biseles finos; serigrafía oscura y luz ámbar contenida.
- Osciladores y mixer a la izquierda; pantalla y filtro en el centro; Poly Mod, LFO y las dos ADSR a la derecha.
- Perillas negras con marcas discretas e indicador claro; cutoff LP de mayor tamaño. Mixer con cuatro faders.
- Franja de controles de interpretación, FX normales y navegación Module/Studio. Studio conserva el editor completo de diagnóstico; el browser personalizado sigue pendiente.
- La bandeja Prophec-y comparte el material del instrumento. Al abrir Tube aparece la cavidad de vidrio, placa y filamento con luz dependiente de la excitación. El diseño mantiene una zona común para futuros módulos completos; no añade efectos de catálogo todavía inexistentes.

Se conectan 36 sliders de superficie (incluyendo los niveles de osciladores repetidos en el mixer), 18 switches y el selector de key tracking LP, usando exclusivamente IDs existentes. Los siete controles Tube y el selector/posición/bypass mantienen su contrato anterior. No se copian del mock botones Menu/Save/Init ni medidores de voces que aún no dispongan del flujo/telemetría correspondiente.

## Lectura y edición

Los controles principales muestran su nombre grabado y revelan el valor mediante popup y pantalla contextual durante la edición. La entrada numérica completa sigue en Studio. Los controles Tube conservan entrada numérica con dB/porcentajes. El gráfico del OLED está rotulado **CONTROL POSITION**: es una guía gráfica de posición normalizada, no una FFT ni una respuesta medida del filtro. Las cifras de síntesis conservan la etiqueta RAW para no inventar leyes acústicas calibradas.

El nombre del programa se actualiza en el hilo UI. La animación sólo consume la telemetría ya existente; ocultarla o cerrar el editor no modifica el procesamiento. La textura se genera una vez, fuera de audio. No hay navegador ni renderizador web embebido.

## Tamaño y límites de esta revisión

Tamaño inicial 1460×700 sin bandeja y 1460×930 con Tube desplegado. Ancho mínimo 1240. Las secciones redistribuyen su ancho; no se escala un PNG. None/HIDE recuperan la altura compacta. Se mantienen áreas de controles y texto a tamaño legible; las ventanas pequeñas pueden usar la vista compacta mientras editan el resto en Studio.

Quedan pendientes el acabado final de Studio, browser y flujo de presets, preferencias persistentes de UI, revisión manual de accesibilidad y pruebas musicales en DAW. La latencia fija de 71 muestras y la transición de posición descritas en `V0_20_TUBE_PROTOTYPE.md` permanecen.

## Validación de esta revisión

`P6UiSmoke` comprueba que construir el editor no modifica el SysEx, que Drive presenta unidades, que los componentes visibles caben a anchos 1240/1460/1780, y que la restauración de estado y el audio durante cambios siguen funcionando. Produce capturas reales de Module con Tube, Studio y None. Los resultados de la revisión se registran en `calibration/v020_aluminum_panel/validation.json`.
