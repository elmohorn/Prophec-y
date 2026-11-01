# v0.19 — Cierre de ingeniería y sound matching

Fecha: 1 de noviembre de2026, Santiago.
Estado: **CERRADA COMO CHECKPOINT DE INGENIERÍA/INVESTIGACIÓN**; calibración acústica aún provisional.
Versión de build:0.19.0. Frontera funcional: `3e148b6c9d1002178ca294fc4613943fa7533ee6`.

## Qué cierra esta versión

v0.19 entrega una base estable para construir la interfaz, una corrección funcional de Unison, consistencia de dominios SysEx/NRPN→DSP, un corpus ampliado y un sistema reproducible de comparación acústica. Se conserva el sonido Default: la investigación no justificó promover un paquete nuevo de constantes globales.

La última fase acordada está ejecutada: se examinaron primero las referencias simples del cutoff, se congeló conservadoramente Default por falta de controles raw verificables y se probaron después las envolventes por separado. Los100renders adicionales rechazan una promoción global; los20resultados baseline reproducen las métricas y RMS anteriores sin diferencia. Véase `V0_19_FINAL_CALIBRATION.md`.

## Cambios confirmados de producción durante v0.19

- Corregido el reataque de primera tecla Unison tras una cola Release, cubriendo los seis modos. La nueva nota ya no puede perderse porque la voz anterior siga activa en release.
- Lecturas de parámetros conocidas limitadas en el mapper al mismo dominio que las ediciones NRPN/host, conservando bytes SysEx y vista raw originales. La auditoría recorre87campos,7745roundtrips legales,22272casos byte/campo y7920casos LP/HP.
- Trazas de controles aplicados para cutoff LP/HP, shape y PW; infraestructura de diagnóstico, sin nueva ley acústica.
- Reparado el harness CI para compilar el nuevo test y admitir Python3.10 en hashing. Versión de build y nombres de artefactos actualizados a0.19.

## Decisiones de calibración

| Pregunta | Decisión de cierre | Límite |
|---|---|---|
| Cutoff LP/HP | Congelar Default; Reference A v0.9 sigue experimental | No se identificó una ley física Hz/raw con las fuentes actuales |
| ADSR amp/filtro | Mantener10s y exponente raw3,5 por separado | Pruebas no establecen constantes físicas ni note-offs desconocidos |
| Profundidad filtro12oct | No promovida | Prometedora en el corpus visto; holdout reservado no ejecutado |
| Perfil lento507 | No promovido | Mejora espectro pero empeora bandas en articulaciones separadas |
| Shape/PW global | Mantener | Controles cercanos explican parte de diferencias; settings hardware inciertos |
| Hard sync | Mantener reset/corrección estable | Residuo armónico real abierto; nuevas fases/slope no pasan pruebas globales |
| Modulación y FX | Mantener implementaciones funcionales y constantes provisionales | Demos nuevas no publican estados suficientes para calibración exacta |
| Optimización | Conservar optimizaciones exactas heredadas; no añadir aproximaciones | Benchmark cloud registra picos y no garantiza tiempo real del PC del usuario |

## Diferencia frente al alcance acústico original

El plan original aspiraba a cobertura representativa sin regresiones en básicos, sync, PWM, filtros, modulación, Unison y FX. Hay cobertura de investigación y pruebas funcionales, pero **no se declara completada la calibración física de todas esas áreas**. FX, Slop/Vintage, amplitudes de modulación, mixer/PW extremos y residuos de sync siguen provisionales. Los ocho holdouts pendientes no se cuentan como ejecutados.

El cierre solicitado para pasar a la UI se interpreta y registra como **checkpoint de ingeniería/investigación con sonido congelado**, no como certificación de paridad acústica. Esta es una delimitación explícita del alcance de salida: las filas de calibración no se marcan validadas por falta de evidencia. El backlog de sound matching permanece abierto fuera del trabajo de UI, para futuras tareas con referencias capaces de discriminar las hipótesis. No depende de que el usuario obtenga un Prophet-6.

## Gates y evidencias

-9/9CTest locales aprobados sobre la base final.
-19pruebas Python en siete suites, incluyendo dos nuevas comprobaciones de descriptores.
-100nuevos renders con cutoff fijo, ocho archivos verificados y comparaciones de20baseline anteriores.
-Huellas DSP y dos pasadas secuenciales de benchmark Release, más un piloto concurrente retenido.
-CI anterior `cf616015...`: Windows/Linux VST3, pluginval10, ASan+UBSan y guards de análisis aprobados en run41257295713.
-**Gate final aprobado** sobre `3e148b6...`: [run41268532229](https://github.com/elmohorn/Prophec-y/actions/runs/41268532229), Windows y Linux VST3, pluginval10, ASan+UBSan y guards de análisis. Artefactos `P6-Core-v0.19-Windows-VST3` y `P6-Core-v0.19-Linux-VST3` disponibles. Los commits posteriores de cierre contienen sólo documentos/mediciones; la frontera funcional validada es la indicada arriba.

No se realizó una nueva prueba manual en un DAW. La matriz amplia de hosts/proyectos/automatización y soak sigue en0.21. Los artefactos de CI validados son builds de desarrollo; no se afirma que exista un instalador o una release pública nueva.

## Continuación v0.20

Plan propuesto en `V0_20_UI_PLAN.md`: organización clásica del instrumento con edición moderna, identidad grafito/marfil/ámbar y foco contextual por sección. No se aprueba todavía una maqueta final ni se cambia el DSP para acomodar el diseño.

La UI debe conservar IDs, normalización, compatibilidad de estado y límites de evidencia. Los parámetros opacos no se convierten en controles ficticios. El editor genérico permanece como diagnóstico.
