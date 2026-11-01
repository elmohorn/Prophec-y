# v0.20 — Familia de efectos Prophec-y y fuentes DSP

1 de noviembre de2026. Investigación de repositorios y lectura focalizada de fuentes. La comparación ejecutable posterior está en `V0_20_FX_COMPARISON.md`; este inventario no certifica rendimiento ni preferencia sonora. Commits y archivos inspeccionados en `calibration/prophecy_fx_sources_v020.json`. No se integra código externo en esta tarea.

## Decisiones de producto confirmadas por el usuario

- Dos vistas del mismo instrumento: **Module** y **Studio**. Performance queda para otra fase.
- Module adopta el lenguaje de módulo de sobremesa contemporáneo: panel continuo de aluminio mate, jerarquía física y pantalla central contextual. El mock no es el inventario final de controles.
- FX normales del Prophet conservan su edición convencional y no reciben obligatoriamente objetos animados.
- La colección **Prophec-y FX** añade efectos de carácter con identidad sonora/visual: válvula, plasma, resorte, cinta, trémolo óptico, ring mod, ferrofluido y shimmer.
- **Válvula es el primer efecto**, por decisión del usuario. La arquitectura y el espacio visual deben contemplar desde el principio toda la colección.

## Integración visual propuesta

La propuesta actual adopta el **carrusel de módulos completos** sugerido por el usuario: un efecto activo inicialmente, catálogo ampliable y cuerpo visual propio por tipo. Posición **Inicio/Final** respecto de toda la cadena normal, incluida distorsión. Ninguno quita el procesador y oculta el cuerpo; contraer sólo oculta la edición. Especificación de estado, latencia, colas y transiciones en `V0_20_CHARACTER_CAROUSEL.md`.

## Contrato técnico propuesto

Separar tres piezas por dispositivo: **procesador DSP**, **descriptor de parámetros/estado** y **vista visual**. El visual consume telemetría; no ejecuta el procesador ni condiciona su sonido.

- Identificador de efecto e instancia, versión de estado, bypass, mezcla, parámetros, latencia y cola declaradas.
- IDs de automatización estables. Antes de implementar fijar capacidad de slots y esquema de parámetros; cambiar de tipo no puede reinterpretar silenciosamente una automatización guardada.
- Preparación/asignaciones fuera del hilo de audio; intercambio seguro de estado y telemetría pequeña y acotada. Dry/wet compensado si el algoritmo introduce latencia.
- Datos visuales específicos: excitación/drive para válvula, fase de portadora para ring, energía de cola para resorte/shimmer. El brillo no se deduce exclusivamente de la posición del knob si queremos mostrar respuesta a la señal.
- Actualización visual con frecuencia limitada y pausa al ocultarse. Movimiento reducido/desactivación conservan el audio.
- Los efectos propios se guardan en el estado del plugin/preset extendido, separados del SysEx Prophet canónico. Exportar SysEx no debe inventar bytes para la colección.
- Nuevas dependencias/modificaciones con atribución, prueba de estabilidad, ruido, DC, aliasing, mono/estéreo, sample rate, bloques, cambios de parámetros, colas, bypass, latencia y CPU antes de elegirlas.

## Repositorios candidatos

### DSPark — candidato de válvula más elaborado

[Repositorio](https://github.com/CristianMoresi/DSPark), MIT, C++20. Se leyeron licencia, README y `Effects/TubePreamp.h` en el commit fijado. Hay código de corriente de placa Koren, solución/interpolación, estado de alimentación/cátodo, tone stack y oversampling configurable. Incluye latencia reportable y mezcla compensada. La cabecera también reconoce aproximaciones, entre ellas conducción de grilla y desacoplo de ciertas cargas.

Tiene más superficie de integración que un waveshaper. Las cifras de aliasing/CPU de sus comentarios son afirmaciones del autor sobre su modelo y máquina, no mediciones nuestras ni validación contra una válvula física. Requiere prueba local de costo, latencia, sonido y comportamiento antes de elegirlo. Sería mi candidato de circuito para el primer A/B, importando sólo el núcleo necesario, sin reemplazar JUCE ni el motor Prophec-y.

### Airwindows — candidato ligero de carácter y catálogo creativo

[Repositorio](https://github.com/airwindows/airwindows), MIT. Se leyó `Tube2Proc.cpp`, su cabecera y licencia. `Tube2` tiene procesamiento estéreo de carácter con asimetría y estado; es un algoritmo estilizado, no un solver de circuito de válvula. Los wrappers inspeccionados usan la API VST antigua y hay que extraer/adaptar el núcleo; no incorporar ese SDK como dependencia.

El inventario incluye Tube/Tube2/TubeDesk, TapeDelay, ToTape, RingModulator, Tremolo y familias de reverb. Sólo Tube2 recibió lectura de implementación en esta tarea; los demás nombres no equivalen a módulos auditados. Candidato alternativo para comparar musicalidad/costo con DSPark, con adaptación propia de parámetros, antialiasing, mezcla y telemetría.

### Patina — piezas para varios dispositivos; revisión necesaria

[Repositorio](https://github.com/ShmKnd/Patina), MIT, biblioteca C++17 ya inventariada en nuestra investigación anterior. Ahora se leyeron específicamente `TubeTriode`, `TubePreamp` y `SpringReverbModel`. El árbol contiene además cinta, trémolo/VCA y ring mod: la correspondencia con la colección es útil, aunque un VCA tremolo no prueba por sí solo un modelo óptico completo.

La válvula usa una transferencia asimétrica y filtros/estados aproximados. La lectura identifica ruido inyectado, coeficientes discretos y un salto entre ramas de la transferencia cerca de x=−1,5: límites aproximados−0,78242089 y−0,8. Es una observación de código, no un defecto audible medido. No recomendaría importarlo sin evaluar esos puntos. La red de resorte comparte ciertos buffers entre canales: revisar su semántica estéreo antes de adaptar. Útil como cantera, no adopción automática de toda la biblioteca.

### 0xFX — banco experimental amplio en C

[Repositorio](https://github.com/averagenative/0xFX), MIT en la raíz. Se leyeron `pedals.c`, `studio.c` y licencia, no se auditaron todas las dependencias/recursos del producto. Hay implementaciones de spring-like reverb, tape echo, ring, shimmer y granular, además de Valve Color. Éste último usa waveshaping `tanh` asimétrico y bloqueo DC; su nombre no implica simulación física detallada.

Los algoritmos están vinculados a structs/dispatch del motor C. Las inicializaciones reservan estado; la adaptación debe mantenerlas fuera de audio y comprobar stereo, feedback y sample rate. El shimmer leído incorpora una lectura acelerada en feedback que requiere escucha y pruebas de continuidad/estabilidad; no se declara calidad final por el README. Es una fuente pertinente para prototipos creativos, no un paquete listo para insertar sin wrapper/auditoría.

### BYOD y chowdsp_wdf — roles distintos

[BYOD](https://github.com/Chowdhury-DSP/BYOD) tiene tube amp, spring, shimmer y otras distorsiones en su árbol. Se revisaron README y documentación de licencias. La fuente pública es GPLv3 y la documentación describe un CLA con distribución BSD para iOS; no supone que cada archivo esté disponible en BSD para nuestro proyecto. Bajo nuestra política actual de reutilización permisiva, mantenerlo como referencia salvo revisión/licencia compatible específica. No se ha copiado código.

[chowdsp_wdf](https://github.com/Chowdhury-DSP/chowdsp_wdf), BSD-3-Clause, es infraestructura de modelado de circuitos, no una colección terminada de los ocho efectos. Su licencia fue comprobada; la evaluación técnica previa sigue siendo de arquitectura. Útil si necesitamos diseñar o ampliar un circuito, con más trabajo propio que adaptar un procesador ya existente.

### DaisySP — base modular prioritaria para los efectos creativos

[Repositorio actual](https://github.com/daisyaudio/DaisySP), antes `electro-smith/DaisySP`. Añadido tras la sugerencia del usuario. Se verificó el árbol actual y se leyeron licencia, submódulos, Overdrive, Tremolo y PitchShifter. Su núcleo MIT está destinado también a plugins/JUCE; no requiere adoptar una placa Daisy para reutilizar DSP portátil.

Overdrive es soft clipping con ganancias pre/post, no una simulación dedicada de triodo. Tremolo multiplica por una modulación de oscilador, no representa por sí solo la dinámica de lámpara/fotorresistencia. PitchShifter usa líneas de retardo y crossfades: componente candidato para shimmer al combinarlo con reverb/feedback, no un shimmer completo ya evaluado. Wavefolder, ruido, resonadores y delay lines amplían las posibilidades creativas; esos módulos se inventariaron sin lectura completa ni escucha en esta revisión.

La separación de licencias importa: el submódulo DaisySP-LGPL fijado en el árbol tiene licencia LGPL-2.1 y contiene ReverbSc, entre otros. No tratar todo lo que aparece en ejemplos históricos como MIT. Evaluar módulos/dependencias concretos; no se copia código ni se declara una incompatibilidad general de LGPL. DaisySP entra como toolbox prioritario para los efectos futuros, mientras la válvula sigue requiriendo la comparación específica Tube2/DSPark.

## Recomendación y límite de la búsqueda

La comparación separada de cinco núcleos está completada: **315 renders**, pruebas de bloques, sanitizers y audios musicales. Resultados y límites en `V0_20_FX_COMPARISON.md`, datos en `calibration/v020_character_fx/` y harness en `research/character_fx/`. Recomendar **DSPark TubePreamp** para el primer prototipo de válvula y **Airwindows Tube2** como alternativa ligera. Aún falta selección musical y validación integrada; ningún núcleo se enlazó al plugin.

Para cinta/resorte/óptico/ring/shimmer, priorizar también la composición de bloques DaisySP MIT y conservar Patina y0xFX como fuentes de módulos a auditar individualmente. En los archivos inspeccionados no se verificó una implementación directa de los conceptos plasma ni ferrofluido. Eso no prueba que no exista en otros proyectos; evita prometer una biblioteca que resuelve exactamente toda la colección.

La búsqueda reduce trabajo DSP, pero no proporciona la bahía visual, telemetría, persistencia extendida o integración JUCE. Esas piezas comunes se diseñarán una vez para todos los efectos, empezando por la válvula.
