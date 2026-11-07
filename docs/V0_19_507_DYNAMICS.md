# v0.19 — evolución del preset 507, filtro y Poly Mod

Se evaluaron 114 configuraciones válidas: 102 notas finales con release bajo la hipótesis inicial, ocho reconstrucciones de frases y cuatro comprobaciones de gate tardío. No se modificó producción. Se conserva como candidata de diagnóstico una envolvente de filtro más lenta; falla parte de la comprobación entre métricas y articulaciones, por lo que no se promueve.

## Reconstrucción y límites

El detector guiado por el programa oficial 007 produjo 53 tramos para AMAZONA y 39 para Audiofanzine, con 10,1% y 8,3% de cuadros ambiguos. Son inferencias, no MIDI recuperado. Los tramos menores de100ms pueden perderse; la confianza interna no es una probabilidad de acierto. La cola final genera cambios aparentes de octava, por lo que no se convirtió en nuevos ataques.

Se fijaron ataques finales provisionales a12,91s/MIDI79 y13,49s/MIDI57; gate final14,10s en ambos. Estos gates son hipótesis visuales/auditables, no mediciones de note-off. Se probaron desplazamientos pareados de ataque/gate de(−40,−100), (0,0) y(+40,+100)ms. La nota aislada omite el historial anterior. Las frases mantienen ese historial aproximado mediante eventos inferidos, con una variante separada(20ms entre notas) y otra ligada(20ms de solapamiento).

No se afirma que el estado del programa de fábrica sea idéntico al usado por cada autor. Velocidad1, pre-roll0,5s y sobremuestreo2x se mantuvieron fijos. No hubo alineación automática ni estiramiento temporal. El conjunto de ambas fuentes ya se había observado en fases previas: la comprobación entre fuentes no es ciega.

## Experimento

Se compararon v09, filtro omitido preservando controles de salida, Poly Mod omitido, solo pitch de Poly Mod anulado y solo forma de Poly Mod anulada. Se exploraron máximos de tiempo de envolvente de filtro2,5/5/10s × exponentes2/3/4, y rangos de pitch de Poly Mod1/2/6 octavas. Los máximos afectan ataque, decay y release; esto no identifica cuál de los segmentos físicos explica una mejora. El exponente afecta el mapeo del control raw a duración y no sustituye la curva interna del ADSR.

La candidata se eligió únicamente por la mediana del error espectral multirresolución en la nota final de Audiofanzine, excluyendo bypasses como soluciones de producción. El perfil elegido **time10_curve3** mantiene exponente3 y cambia el máximo de2,5 a10s. Con decay raw88, prolonga nominalmente ese segmento aproximadamente de0,83 a3,33s en el mapeo actual. Se aplicó sin reajustarla a AMAZONA y a ambas reconstrucciones de frases.

## Separación de causas

Mediana del error espectral multirresolución de nota+release sobre las tres hipótesis temporales, dB:

| Hipótesis | Audiofanzine | AMAZONA |
|---|---:|---:|
| v09 | 11,414 | 12,818 |
| Filtro omitido | 5,944 | 9,604 |
| Poly Mod omitido | 11,630 | 12,774 |
| time10_curve3 | 6,865 | 10,372 |

Omitir el filtro reduce mucho esta medida; no demuestra que el hardware tuviera el filtro omitido ni que el error sea exclusivamente del cutoff. La envolvente compartida también mueve el pitch de Poly Mod. Una simple ablación no separa esas contribuciones físicas.

Anular solo la modulación de forma produce audio **idéntico byte a byte** a v09 en las seis notas aisladas. Es coherente con que la forma inicial está en el extremo de pulso y la modulación positiva satura el rango permitido. En esta prueba, esa ruta no explica el residuo. Anular el pitch de Poly Mod equivale al bypass completo en la métrica mostrada y no mejora consistentemente ambas fuentes.

## Comprobación sobre frases completas

Error espectral multirresolución / error del perfil de bandas, dB; menor es mejor:

| Fuente y articulación supuesta | v09 | time10_curve3 |
|---|---:|---:|
| AMAZONA, separada | 9,533 /3,539 | 8,842 /7,332 |
| AMAZONA, ligada | 12,684 /5,064 | 9,641 /3,567 |
| Audiofanzine, separada | 8,572 /2,435 | 7,016 /5,534 |
| Audiofanzine, ligada | 15,519 /9,089 | 14,827 /6,646 |

La medida espectral mejora en las cuatro combinaciones; el perfil de bandas empeora en las dos variantes separadas. La envolvente de amplitud normalizada mejora ligeramente en las cuatro, pero las hipótesis temporales siguen condicionando el resultado. No se elige la articulación de cada grabación a posteriori para ocultar los casos adversos.

## Sensibilidad adicional del gate final

La revisión gráfica mostró que AMAZONA conserva energía considerable después del gate inicial supuesto de14,10s. Por ello se añadieron cuatro comprobaciones adaptativas, sin reajustar la candidata, con gates15,10/16,10s y referencia hasta18,60s. A15,10s, el error espectral baja de12,192 a9,071dB pero el de bandas sube de9,651 a10,582dB; a16,10s, baja de11,874 a8,699dB y el de bandas sube de9,631 a10,617dB. La contradicción entre métricas persiste. El gate14,10s de la primera tabla y las frases no debe confundirse con un note-off establecido.

## Decisión

**No cambiar globalmente los tiempos de envolvente.** La candidata prolongada es una dirección de investigación, no una calibración física. Ya falla un criterio básico de robustez dentro del propio507; no se avanzó a promoverla ni a afirmar una regresión validada frente a los otros presets. La comprobación con capturas antiguas queda pendiente para una candidata que supere este filtro inicial.

La siguiente comparación útil separará los controles de tiempo del filtro de la ley de cutoff/contorno, usando MIDI/gates conocidos o una fuente con ejecución mejor documentada. Los resultados actuales no justifican tocar el reset.

## Reproducir y verificar

`calibration/v019_507_dynamics/` contiene eventos, cuadros con incertidumbre, protocolos, métricas y hashes. Generar las inferencias con `analysis/infer_performance.py` (frame80ms, hop20ms, rango MIDI48–90; fuentes completas) y ejecutar `python3 tools/compare_507_dynamics.py`. Ejecutar además `python3 tools/check_507_late_gate.py` para la sensibilidad adaptativa. Requiere los MP3 y el007.syx ya documentados en la fase anterior.

Se verificaron 102 filas de comparación aislada y ocho de frases, valores finitos y las seis igualdades de audio al anular modulación de forma. Las cuatro comprobaciones adicionales también producen métricas finitas. Los scripts pasan compilación Python. No se atribuye a esta fase una nueva ejecución de pluginval ni una modificación del motor.
