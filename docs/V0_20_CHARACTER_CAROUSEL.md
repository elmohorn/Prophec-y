# Prophec-y FX — carrusel de módulos

Actualización posterior: el primer prototipo nativo está descrito en `V0_20_TUBE_PROTOTYPE.md`; incluye la política de latencia fija y las limitaciones actuales. El texto siguiente conserva el alcance de esta propuesta/evaluación.

Propuesta concreta a partir de la idea del usuario,1de noviembre de2026. Module y Studio siguen mostrando el mismo instrumento. Performance permanece fuera de esta fase.

## Comportamiento inicial

Un **slot de efecto Prophec-y activo**, elegido de un catálogo mediante carrusel. Es una propuesta de alcance inicial; el carrusel no se interpreta como ocho efectos simultáneos ni se compromete una pedalera ilimitada.

Al seleccionar otro efecto cambian el procesador y todo el módulo visual: proporción interna, materiales, controles y animación. El marco general conserva navegación, nombre, preset del dispositivo, posición, bypass y ayuda. El cuerpo puede alojar una válvula de vidrio, carretes, muelles o una superficie translúcida sin forzarlos a ocupar el mismo tipo de knob o dibujo.

La válvula sigue siendo la primera implementación. Los otros tipos entrarán cuando tengan DSP y pruebas; no se muestran como efectos operativos por tener un dibujo.

## Inicio / Final

La posición se refiere a la **cadena completa de efectos normales**, incluida la distorsión existente:

| Posición | Orden posterior a la suma de voces y ganancias existentes |
|---|---|
| Inicio | Prophec-y → distorsión normal → FX A → FX B → salida |
| Final | distorsión normal → FX A → FX B → Prophec-y → salida |

Un mismo slot ocupa una de las dos posiciones; no son dos procesadores independientes. Esto permite saturar antes de reverbs/delays o colorear también sus colas, sin modificar el orden interno de los FX Prophet.

## Ninguno, bypass y contraer

- **Ninguno:** sin procesador Prophec-y en la ruta, módulo visual oculto y espacio recuperado. Queda un selector compacto para volver a abrirlo.
- **Bypass:** conserva tipo y ajustes para comparar rápidamente; no confundir con borrar la selección. La política de cola/latencia de bypass se definirá y probará.
- **Contraer:** sólo oculta la edición detallada; el efecto sigue activo y su indicador compacto permanece visible.

En Module la sección se integra al lenguaje industrial del panel y puede desplegarse. Studio expone el módulo completo y los parámetros adicionales. La representación seleccionada se comparte entre vistas; no se duplican dos instancias DSP.

## Estado y transiciones

Conservar ajustes por tipo de efecto al navegar. Guardar `selected_effect`, `position`, versión y parámetros del catálogo en estado extendido del plugin, sin escribir bytes inexistentes al SysEx Prophet. Contraer es una preferencia de UI; Ninguno y bypass afectan el sonido y requieren semántica de preset definida.

El carrusel prepara el nuevo DSP fuera del hilo de audio. Al estar listo, realiza un intercambio seguro y una transición limitada. Preparar DSPark tarda del orden de0,1–0,24s en el ejecutor de prueba: no puede hacerse dentro de la callback. No dejar una UI animada esperando mientras bloqueamos el audio.

La latencia declarada debe propagarse al host y el dry/wet debe alinearse. Cambiar efecto puede cambiar latencia; fijar y comprobar esa política antes de permitir hot swap durante reproducción. Mover Inicio/Final tampoco es sólo cambiar un puntero: hay que gestionar continuidad y estados de toda la ruta afectada.

Para reverbs/delays definir una opción de cola acotada o fade al cambiar/quitar el efecto, con un máximo de procesadores de transición y presupuesto CPU explícito. No prometer colas ilimitadas acumuladas al pasar rápidamente por el carrusel. Navegar y activar deben tener una respuesta clara, y el estado transitorio ha de ser visible.

La animación recibe telemetría y nunca produce el audio. Cerrar el editor, contraer o desactivar movimiento no cambia el procesamiento.

## Pruebas antes de implementación final

Ninguno transparente; presets y sesión recuperables; posición correcta respecto de distorsión/A/B; cambios repetidos de tipo/posición/bypass; latencia y mezcla; colas; automatización con IDs estables; ausencia de asignaciones/locks en audio; comportamiento igual al cambiar Module/Studio o esconder la animación.
