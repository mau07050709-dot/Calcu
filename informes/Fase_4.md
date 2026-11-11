# Calcu · Informe de la Fase 4

11 de noviembre de 2026

Proyecto principal en `Desktop\Calcu`

## 1. Funcionalidades implementadas

- **Intermediario HTTPS.** Python ofrece TLS directo con certificado y clave del servidor, o HTTP de loopback detrás de Caddy. `GET /health` autentica el token y confirma el intermediario. `POST /ask` envía el mensaje a Responses API y recibe texto para la LCD. La sonda de salud no llama a OpenAI.
- **Contexto de sesión.** El motor de aplicación compartido guarda hasta cuatro pares de pregunta y respuesta en RAM. Envía los pares válidos como mensajes de usuario y asistente con la siguiente pregunta. Los errores conservan el borrador y quedan fuera del historial. Cambiar URL, token o demostración en Windows limpia el contexto de la conexión anterior. OpenAI recibe `store: false`.
- **Windows.** Se añadió **Probar conexión** junto a URL y token. La prueba y las consultas se realizan en tareas separadas de la ventana. La interfaz muestra ausencia de configuración, procesamiento y errores de token, TLS, red, ruta, modelo o tiempo de espera. El historial permite revisar ambos mensajes de cada turno en páginas.
- **ESP32 y LCD.** El trabajador de FreeRTOS envía el mismo JSON que Windows. La LCD de 16×2 muestra los turnos por páginas. El teclado lógico permite mover el cursor del borrador y abrir el historial aunque se personalice el atajo de modo. El perfil físico configurable valida la capacidad de los pines, los duplicados, los solapamientos, las direcciones de fila y el mapa de teclas. Los adaptadores siguen desactivados por defecto.
- **Continuidad.** Se conservaron el modo calculadora inicial, el motor científico y la demostración local. El simulador y el firmware siguen usando el mismo núcleo C++.

## 2. Archivos modificados

| Archivos | Cambios |
| --- | --- |
| `server/proxy.py`, `server/test_proxy.py` | TLS, sonda autenticada, contexto, errores de API y pruebas de extremo a extremo con OpenAI simulado. |
| `include/chat.hpp`, `src/core/chat.cpp`, `include/app.hpp`, `src/core/app.cpp` | Contrato de contexto, serialización compartida, estado de sesión e historial paginado. |
| `include/keypad.hpp`, `src/core/keypad.cpp` | Atajos de edición e historial compatibles con el cambio de modo. |
| `include/config.hpp`, `include/local_config.example.hpp`, `include/hardware_profile.hpp`, `src/core/hardware_profile.cpp`, `src/esp32/hardware.cpp` | Perfiles y validación física, selección de LCD y fallback por serie. |
| `src/esp32/proxy_chat.cpp`, `src/esp32/proxy_chat.hpp`, `src/esp32/wifi.cpp` | Envío de contexto, resultados acotados y estados de red o proxy. |
| `src/windows/desktop_chat.cpp`, `src/windows/desktop_chat.hpp`, `src/windows/main.cpp` | Sonda WinHTTP, flujo asíncrono y controles de configuración e historial. |
| `test/test_core/test_main.cpp`, `test/windows/test_desktop.cpp` | Regresiones científicas, contexto, configuración y rechazo de un certificado sin confianza. |
| `scripts/Compilar Calcu.ps1`, `.gitignore` | Reemplazo del ejecutable compilado y exclusión de credenciales y temporales. |
| `README.md`, `AGENTS.md`, `informes/Fase_4.md` | Instrucciones de activación, procedimiento de publicación e informe. |
| `Calcu.exe`, `.pio/build/esp32/firmware.bin` | Binarios generados localmente y excluidos de Git. |

## 3. Pruebas aprobadas y fallidas

| Prueba | Resultado |
| --- | --- |
| PlatformIO Native y C++ con advertencias como errores | Aprobadas. Operaciones científicas, modo, multitap, edición física, historial, recuperación de borrador y validación de perfiles. |
| Suite de Windows | Aprobada. Persistencia de ajustes, DPAPI, validación de URL, configuración incompleta y demostración. |
| Seis pruebas del intermediario | Aprobadas. HTTPS local con certificado efímero confiado explícitamente por el cliente de prueba, token válido e inválido, dos turnos con contexto, validación, errores y OpenAI simulado. |
| WinHTTP con certificado no confiable | Aprobada. Rechazó GET y POST. La prueba no modificó el almacén de confianza de Windows. |
| Arranque del servidor sin credenciales | Aprobada. Informó que faltaban `OPENAI_API_KEY`, `OPENAI_MODEL` y `CALCU_DEVICE_TOKEN`. |
| Firmware ESP32 | Compiló para el perfil provisional `esp32dev`. PlatformIO reportó 46.016 bytes de RAM estática y 812.437 bytes de flash de programa. |
| `Abrir Calcu.cmd` | Primer intento de actualización fallido por el reemplazo del ejecutable existente. Se corrigió y la asociación de Windows recompiló y abrió el simulador. |
| Inicio directo de `Calcu.exe` | Aprobado. Windows abrió la ventana del ejecutable actualizado. |
| Publicación en GitHub | Fallida por autenticación. Se creó el commit local, se ejecutó el push y se verificó que `main` remoto conservaba el commit anterior. |
| Consulta auténtica a OpenAI | Pendiente. La clave no estaba disponible en las variables del proceso, usuario o equipo. |
| Wi-Fi, TLS y periféricos en placa | Pendientes. Los estados y el firmware se compilaron. Falta verificar la radio, la CA, el reloj y los periféricos físicos. |

## 4. Comprobaciones visuales

Se observó el inicio en calculadora, la ventana de configuración y el mensaje **Completa URL HTTPS y token**. En la demostración, dos pulsaciones rápidas de 2 escribieron `B`. Se observó **Consultando...**, la desactivación de envío mientras se procesaba, el contador de historial, la pregunta guardada y una página de su respuesta. Se volvió a calculadora sin reiniciar. Estas comprobaciones usaron la demostración local.

El lanzador se probó mediante la asociación de Windows. El doble clic literal en el Explorador sigue pendiente debido a la restricción de computer-use para ejecutar archivos de comandos desde el Explorador.

## 5. Problemas encontrados y soluciones

- **Actualización del binario.** PowerShell devolvió «No se puede crear un archivo que ya existe» al mover el resultado sobre el ejecutable anterior. Ahora se compila en un temporal por proceso y se usa un reemplazo de archivo al actualizar.
- **Rechazo esperado de TLS.** Una conexión de prueba cancelada por WinHTTP podía generar una traza ruidosa antes de recibir HTTP. El servidor maneja ese cierre sin registrar cabeceras ni consultas.
- **Carpeta sin metadatos Git.** Se vinculó la carpeta existente a `origin/main` conservando el árbol local y el historial remoto. El informe anterior sigue disponible localmente.
- **Falta de credenciales.** README describe la configuración exacta del servidor, las opciones de TLS y la prueba desde Windows. GitHub permitió consultar `main`. El `git push` falló con «could not read Username for 'https://github.com'». El commit local quedó preparado y la publicación requiere una sesión con permiso de escritura.

## 6. Limitaciones pendientes

- Probar una consulta auténtica y un seguimiento con una clave y un modelo habilitados, mediante un intermediario con certificado confiable.
- Confirmar el ESP32, el controlador y la tensión de la LCD, el teclado y sus conexiones. Quedan por medir uso de pila, legibilidad, rebote, Wi-Fi y TLS en placa.
- El historial conserva hasta cuatro turnos con el texto adaptado para la LCD. Se reinicia con la sesión. El intermediario está diseñado para el prototipo y atiende una solicitud por vez.
- La sonda comprueba TLS y token, y la consulta real comprueba la clave y el modelo de OpenAI.

## 7. Avance estimado

**Software del prototipo: 93 %.** Se ponderaron la continuidad científica y edición (30/30), la preparación de firmware y LCD (25/25), el flujo conversacional (23/25) y las verificaciones de entrega (15/20). Los puntos pendientes corresponden a OpenAI real, la publicación remota y las verificaciones finales de arranque.

**Integración física verificada: 0 %.** Se compiló el firmware y se probaron perfiles sintéticos. Aún no hubo carga ni medición en hardware real.

## 8. Recomendaciones para la siguiente fase

1. Configurar las tres variables del servidor y un certificado confiable. Ejecutar **Probar conexión**, enviar una pregunta y comprobar una segunda que requiera recordar la primera.
2. Confirmar los componentes y el pinout. Probar el teclado y la LCD primero por separado. Después verificar Wi-Fi, NTP y el intercambio HTTPS en la placa.
3. Autenticar GitHub con una cuenta que tenga permiso de escritura. Publicar el commit local y comparar su hash con `origin/main`.
