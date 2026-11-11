# Plan de trabajo — Prophec-Y

Actualizado: 11 de noviembre de 2026. Este es el plan activo de `main`; los
planes de versión anteriores se conservan como historial. Alcance: estabilizar
el instrumento y completar sus flujos antes de añadir grandes funciones.

## En curso, sin necesitar el equipo del usuario

1. **Trazabilidad:** distinguir claramente código de `main`, artefactos de
   Actions y releases históricas. La publicación automática actual está ligada
   a `v0.20-ui`: migrarla a un flujo explícito de `main` con CI y checksums,
   sin publicar builds no verificados.
2. **Clics y niveles:** reproducir cambios de CC7, Program Volume, robado de
   voz, envolventes, sustain, cambios de preset/FX y arpegiador. Medir por
   etapas y bajo distintas frecuencias/buffers; distinguir discontinuidades
   reales de ataques y transitorios deliberados. El suavizado de CC7 de 5 ms
   aborda sólo el cambio de ese controlador; no acredita el cierre de los
   clics reportados. Preservar timbre y comportamiento en reposo.
3. **Presets:** ampliar la matriz de 1–6 notas, velocidad, mod wheel y
   sustain; estudiar silencio, picos, DC y estados no finitos. Verificar el
   flujo de carga y mantener los SysEx originales.
4. **Rendimiento:** medir motor + wrapper + Character + UI, con p99/picos,
   44,1/48/96 kHz, buffers 32–512 y casos musicales. Las mediciones históricas
   en máquinas compartidas no sustituyen una línea base estable. Optimizar
   solamente puntos medidos con pruebas de sonido, tiempo real y estado.
5. **Latencia:** medir end-to-end el retardo de las voces y Character a cada
   factor de oversampling antes de decidir cualquier cambio en PDC.
6. **UI:** caracteres del display, tooltip del medidor, prueba de escalado y
   tamaño para pantallas pequeñas, alineación y legibilidad. Mantener el logo,
   vidrio, Master físico y diales aprobados; FX estándar sigue en el sinte.
7. **Biblioteca:** mantener categorías automáticas marcadas como inferidas;
   planificar A/B, undo y persistencia de la identidad del preset con migración
   y fallback cuando la entrada ya no exista.
8. **Arpegiador:** validar transporte, loops y cambios de reloj; Swing aún usa
   timing recto, y su paridad con hardware no está demostrada. Aclarar estados
   visuales de nota/silencio y reproducción antes de añadir nuevos patrones.
9. **Pitch bend:** el manual oficial documenta 0–12 semitonos por dirección.
   El DSP limita a 12; conservar el dominio bruto 0–24 para compatibilidad y
   aclarar la presentación, sin afirmar que 24 semitonos sea rango confirmado.
   [Manual Sequential, p. 57](https://www.sequential.com/wp-content/uploads/2021/02/Prophet-6-Operation-Manual-2.1.pdf).
10. **Gates:** añadir casos pertinentes de nivel/transitorios y conexiones UI;
    los fingerprints actuales excluyen la tabla de headroom y no prueban por
    sí solos ausencia de clics ni equivalencia sonora con hardware.

## Pendiente de validación con el usuario

- En DAW: abrir/guardar/cerrar/reabrir, automatización, tempo y loops, distintos
  buffers y sample rates; registrar versión exacta, preset y ajustes.
- Con Roland FA-08: entrada, canales, pitch bend, mod wheel, sustain y reloj.
- Escucha de clics, niveles, Character y presets en una versión identificada.
- Revisión visual del plugin real en el tamaño y DPI de su pantalla.

Los resultados automáticos no sustituyen estas pruebas. No se darán por
cerrados los clics, la fidelidad al Prophet-6 ni la aceptación visual hasta
tener evidencia correspondiente.
