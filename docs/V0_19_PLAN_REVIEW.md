# v0.19 — Revisión del plan antes del cierre

Fecha: 31 de octubre de2026, Santiago. Estado: **ACTIVA; no cerrar todavía**.
Base revisada: `783856bb15d8cfd1d66f6fbc6fd1f7b34d7a686c`.

## Conclusión

La última propuesta de cerrar v0.19 tras la auditoría era prematura. El usuario confirmó que primero se debe investigar el cutoff estático y después las envolventes, y pidió contrastar esa decisión con el plan previamente establecido. Esta revisión no sustituye ese trabajo por documentación ni inicia la interfaz.

El plan original también exige cobertura representativa de modulación, Unison, PWM y FX, y regresión/rendimiento. Por tanto, terminar cutoff y envolventes será un avance necesario, pero no certificará por sí solo todo el hito. Lo restante debe probarse o quedar sujeto a una decisión explícita de alcance antes de etiquetar la versión.

## Documentos de alcance revisados

- `PROPHET6_PARITY_MATRIX.md`: matriz maestra, versiones0.19/0.20/0.21 y gates de publicación.
- `V0_18_CLOSEOUT.md`: frontera funcional heredada y tareas acústicas transferidas.
- `V0_19_SOUND_MATCHING.md`: plan específico posterior, prioridades P0-A..G y restricción de no depender de que el usuario obtenga hardware.
- `V0_19_EXTERNAL_CALIBRATION_MATRIX.md`, `V0_19_PRIOR_CROSSCHECK.md` y `V0_19_RESEARCH_REVIEW.md`: investigación, priors y reglas de promoción.
- Informes de filtros/sync, Unison y recalibración, integridad/holdout, audio independiente, ondas básicas, comparaciones hard sync, dinámica507 y auditoría de dominios.

Hay una contradicción heredada: la matriz y el cierre0.18 exigían capturas controladas como P0, mientras que el plan específico posterior permite avanzar con grabaciones online y las declara opcionales. Se actualiza la matriz para reflejar esa restricción vigente. No se rebaja la calidad de evidencia requerida para promover una ley física.

## Alcance versus evidencia actual

| Área del plan | Evidencia completada | Estado al revisar el cierre |
|---|---|---|
| Investigación de proyectos/literatura | Priors con licencias y límites; documentación oficial; literatura DSP; hipótesis ejecutables | Realizada como revisión de ingeniería; no promete una búsqueda exhaustiva permanente |
| P0-A osciladores/PW/mixer | Referencias recuperadas, pruebas de priors,40 renders de ondas individuales y controles equivalentes | Cobertura parcial; no identifica extremos físicos PW, normalización o todos los niveles del mixer |
| P0-B hard sync | Oráculo independiente, pruebas por etapa/ensemble y varias fuentes reales | Investigación extensa; residuo espectral abierto. Rechazar una intervención global es una decisión válida, no prueba de fidelidad completa |
| P0-C filtros LP/HP | Factoriales, rejilla corregida de360 renders, fuente independiente, self-osc y auditoría de unidades | **Siguiente prioridad:** separar cutoff estático de env/profundidad; Reference A v0.9 termina en2,96kHz nominales sin modulación |
| P0-D envolventes | Incertidumbre de gates, reataque Unison corregido,114 configuraciones de507, reloj y unidades probados | **Después del cutoff:** falta identificar las leyes temporales sin compensaciones; candidata lenta no promovida |
| P0-E LFO/Poly Mod/pressure | Rutas funcionales; fuentes/ganancias auditadas; casos complejos reales y ablaciones | Parcial. Faltan controles acústicos aislados y cobertura representativa; no certificar profundidades por coincidencia entre emuladores |
| P0-F distorsión/Slop/Vintage/glide | Implementaciones provisionales y controles; resultados previos acotados | No se encontró en los informes examinados una validación acústica suficiente para cerrar estas leyes |
| P0-G efectos digitales | v0.18 entrega DSP funcional y pruebas; demos adquiridas e inventario de fuentes | Funcionalidad y fidelidad acústica son distintas. Falta una cobertura representativa documentada de FX contra hardware |
| Optimización CPU | Infraestructura y optimizaciones exactas heredadas de0.18 | No se acredita una nueva mejora de CPU por esta revisión. Registrar una medición final comparable; optimizar sólo si hay evidencia |
| Generalización | Cinco extractos principales, fuentes independientes exploratorias, comprobaciones entre registros/articulaciones | Los ocho holdouts reservados siguen sin ejecutarse; ninguna comparación exploratoria los reemplaza retroactivamente |
| Release gates |9/9 CTest locales en auditoría; historial de CI anteriores | **Bloqueo concreto:** última CI fallida. Ver sección siguiente |
| Interfaz/presets | Editor genérico y APIs existentes | Sigue asignado a0.20. No se inicia aquí |
| DAW matrix/soak prolongado | Asignado por el plan a0.21 | No trasladarlo entero a0.19. Una prueba básica del VST en DAW es útil, pero no sustituye pluginval ni cierra0.21 |

Se conserva el criterio original de salida: cobertura documentada de escucha/medición sobre patches representativos básicos, sync, PWM, filtro, modulación, Unison y FX, sin regresiones fuera de las tolerancias acordadas. No hay una cifra única de similitud ni un conjunto universal de tolerancias numéricas ya establecido en los documentos revisados; las métricas y límites deben fijarse según cada fuente antes de puntuar nuevas candidatas.

## CI: fallo detectado y reparación concreta

[Run41255778521](https://github.com/elmohorn/Prophec-y/actions/runs/41255778521), sobre el commit de auditoría, terminó en fallo:

1. Linux, Windows y sanitizers compilaron listas explícitas que omitían `p6_parameter_domain_audit`. CTest reconocía el noveno test, pero no encontraba el binario. Los ocho anteriores pasaron; pluginval quedó sin ejecutar.
2. La prueba de integridad de medios falló porque `hashlib.file_digest` no existe en Python3.10 del runner Ubuntu22.04.

Correcciones de esta revisión:

- Incluir el ejecutable nuevo en ambas listas de compilación (build Windows/Linux y sanitizers).
- Reemplazar la API de hashing por SHA256 incremental con bloques de1MiB, compatible con Python3.10 y memoria acotada.
- Verificar el hash calculado contra el SHA256 de los bytes originales en la prueba de medios existente.

El resultado9/9 anterior era local y sigue siendo cierto; no se presenta como CI verde. La nueva CI completa debe pasar antes del cierre. No hubo modificación de sonido ni calibración como parte de esta reparación.

## Orden de trabajo para terminar v0.19

1. **Restaurar y verificar gates:** compilar el test en CI y corregir compatibilidad Python; exigir nueva ejecución satisfactoria de Windows/Linux VST3, sanitizers, análisis y pluginval10.
2. **Cutoff estático:** clasificar LPF/HPF/self-osc ya adquiridos por información realmente observable. Publicar frecuencias/plateaus medibles, límites y códigos raw conocidos o desconocidos. Un sweep sin valores raw no identifica por sí solo Hz/raw. Separar LP y HP y evitar que profundidad/resonancia absorban sus errores. Mantener Default si no hay evidencia suficiente para promover otra ley.
3. **Congelar el cutoff seleccionado para la prueba de envolventes:** luego evaluar amp y filtro por separado, mapping raw→tiempo y contorno temporal, con gates conocidos o hipótesis explícitas. No reajustar cutoff y PM para mejorar cada frase.
4. **Regresión y cobertura original:** usar los mismos eventos/pre-rolls para comparar referencias anteriores y nuevas, reportar métricas en desacuerdo y residuos de sync. Revisar PWM, modulación, Unison y FX. Identificar qué casos faltan; no presentar audio sin patch conocido como calibración exacta.
5. **Resolver el estado de los holdouts:** ejecutar sólo si se consigue audio apto y estado/eventos verificables. De lo contrario conservarlos como no ejecutados y explicar el límite de generalización, sin sustituirlos por casos ya vistos.
6. **Rendimiento y cierre:** snapshot final de huellas/CPU con condiciones registradas, ledger de decisiones y pendientes, gates de release aprobados. Cualquier traslado de parte del alcance acústico debe quedar explícito; no marcarlo terminado por falta de evidencia.
7. **Sólo entonces v0.20:** editor propio, browser de presets/bancos, SysEx, Init/Compare/A-B y edición de secuencias conforme a semántica verificada. Los visuales reactivos de FX son exploración0.20+, no motivo para reabrir DSP ni inventar controles.

## Qué no exige esta revisión

- Que el usuario compre, pida prestado o grabe un Prophet-6.
- Promover alguna candidata por obligación: conservar el modelo y documentar una hipótesis rechazada es un resultado válido.
- Resolver parámetros opacos de firmware, nuevas codificaciones FX o toda la matriz de hosts antes de la UI; sus destinos posteriores siguen en la matriz.
- Repetir búsquedas o rejillas amplias sin una pregunta que los datos puedan resolver.

## Validación de esta revisión

La modificación funcional se limita al harness de CI y al hash de adquisición, sin tocar DSP. Se ejecutan localmente CTest y las17 pruebas Python de métricas/oráculo/eventos/medios/self-osc/armónicos estéreo. La disponibilidad de hardware, la calibración de cutoff/env y la cobertura pendiente no se marcan como completadas por estas pruebas.
