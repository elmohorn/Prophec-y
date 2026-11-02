# Evaluación exploratoria: Signalsmith Elliptic BLEP

Base de producción: `v0.20-ui` commit `b63ff2f909422779e683bd07bb3073154697f832` (preview 282 motor `83f433eaa7a34ef463511119eb6ce195b1c194d6`); el blob local del oscilador coincide con el remoto `87cc9e2f3316d525dd0caa3eaec4b56451612c16`. La rama `visual-plasma-ferrofluid` c980b55 incluye trabajos visuales y de audición posteriores, no una modificación del oscilador.

## Procedencia y alcance

Signalsmith Elliptic BLEP, commit `77bf9866b705ddffe4870b40020411cf9192cf3b`, MIT, copyright 2024 Signalsmith Audio Ltd. / Geraint Luff. Se incluye el header original y su licencia. El motor actual deriva de joshjetson/phosphor, MIT, según THIRD_PARTY_NOTICES.md. No modifica producción. El prototipo usa la geometría nominal del propio motor, calcula cambios de pendiente y eventos de hard sync, y usa el residual elíptico de orden 2 y 1 respectivamente. Es un primer experimento, NO una implementación equivalente lista para integrar.

Limitaciones: sólo parámetros estáticos; falta manejo de esquinas móviles, cambios de pendiente por FM/PWM/shape y eventos simultáneos robustos; falta evaluar Poly Mod real y señal de dos osciladores en P6Voice. El hard sync usa un master matemático, no la ruta real de producción. La inicialización y parámetros extremos necesitan auditoría adicional. Se detectó un fallo de doble corrección en el primer prototipo de sync y se corrigió antes de la medición; no certifica todos los eventos coincidentes. No hay evaluación de cadena real, su decimador ni efectos no lineales. CPU de render incluye E/S y no sirve como benchmark de CPU. El filtro elíptico añade respuesta de fase y de amplitud, con coeficientes de paso bajo de 20 kHz según su documentación; a 2× esto puede afectar contenido por encima de 20 kHz antes de etapas no lineales. Los tiempos y ganancia de alineación se registran, pero una traslación no iguala dispersión de fase.

## Reproducción

Desde la raíz, con C++20, NumPy y SciPy:

```
c++ -std=c++20 -O3 -DNDEBUG -Iinclude -Iexperiments/elliptic_blep experiments/elliptic_blep/static_probe.cpp src/PolyBLEPOscillator.cpp -o experiments/elliptic_blep/static_probe
python3 experiments/elliptic_blep/run_static.py
```

Produce 1,5 s por escenario, con forma y fase nominal iguales y sin cambiar código del motor. Frecuencias 44,1/48/96 kHz, varias formas y dos relaciones de sync. Se compara contra una referencia de forma nominal muestreada a 16× y filtrada con `scipy.signal.resample_poly` (Kaiser beta 12); 8× contra 16× estima la convergencia, sin garantizar una referencia exacta, especialmente para bordes de anchura 1e-6 y notas agudas. Se minimiza la diferencia total mediante ganancia y desplazamiento fraccional: no es una métrica aislada de aliasing, sino error de forma de onda incluyendo fase, filtros, transitorios y errores de referencia.

La energía fuera de armónicos usa una FFT coherente de 1 segundo, notas estáticas de frecuencia entera, sin sync; se excluyen DC y bins próximos a armónicos. Puede no detectar alias coincidente con un armónico. En 8 kHz/48 kHz todos los bins de alias caen en múltiplos armónicos: el resultado casi infinito es INVÁLIDO, no evidencia de ausencia de alias. No se reporta esta métrica para hard sync. Audios son pruebas aisladas, no grabaciones del Prophet-6; cada par iguala RMS sobre la parte estable y comparte reducción de pico, no iguala sonoridad percibida ni fase.

## Observaciones, no veredicto

En estas implementaciones preliminares y con la métrica limitada anterior: sierra 997 Hz/48 kHz muestra -32.4 dB fuera de armónicos en actual y -59.6 dB en prototipo; pulso 10% muestra -28.4 y -56.7 dB. La diferencia total alineada frente a la referencia 16× fue respectivamente -25.0/-15.5 dB y -23.0/-13.9 dB: incluye fase y respuesta espectral, no contradice necesariamente la reducción de energía fuera de armónicos. Las referencias 8×/16× difieren -35.7 y -32.5 dB, lo que limita la precisión. A 8 kHz difieren -22.2 dB y no sustentan una conclusión detallada. No se han comprobado Poly Mod, automatización, rendimiento, ni full voice. Por ello la recomendación actual es conservar producción y continuar con una implementación de eventos equivalentes y una referencia independiente validada antes de decidir. Una conclusión sobre fidelidad Prophet-6 exige grabaciones de hardware emparejadas.
