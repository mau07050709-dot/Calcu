# Calcu · Informe de la Fase 5

12 de noviembre de 2026

Proyecto principal en `Desktop\Calcu`.

## Funciones implementadas

- **Diagnóstico en tres pasos.** `GET /live` confirma el servidor HTTPS y no envía el token. `GET /health` valida el token. `POST /verify` requiere un token válido y solo confirma OpenAI si Responses API devuelve texto generado en una respuesta completada. La comprobación no cambia el historial. Windows ejecuta los tres pasos fuera del hilo de la interfaz.
- **Configuración del intermediario.** Puede iniciarse con token de dispositivo aunque falte OpenAI. Así se puede comprobar el servidor y el token primero. `--prompt-key` lee la clave sin eco en el servidor. `--check-openai` realiza allí una consulta breve con el modelo configurado. Las rutas de chat y verificación devuelven un error de configuración si falta la clave o el modelo.
- **Firmware y recuperación.** Tras perder una conexión, el ESP32 intenta reconectar de inmediato y después cada 15 segundos. Se distingue la falta de reloj NTP, la cuota y la configuración de la API. El cliente HTTPS rechaza redirecciones. Se conservan el perfil físico configurable, el teclado, la LCD 16×2 y el mismo motor matemático de Windows.
- **Carga USB.** `Cargar ESP32.cmd` enumera puertos, pide confirmar el perfil de la placa, carga por un COM explícito y puede abrir el monitor serie. El modo `-List` solo enumera. Se conservaron `Calcu.exe` y `Abrir Calcu.cmd`, el inicio en calculadora, las funciones científicas, el atajo de modo, el multitap y el historial paginado.

## Archivos modificados

| Archivo | Cambio |
| --- | --- |
| `server/proxy.py`, `server/test_proxy.py` | Rutas de diagnóstico, lectura sin eco, prueba desde servidor y pruebas de HTTPS. |
| `src/windows/desktop_chat.cpp`, `src/windows/desktop_chat.hpp`, `src/windows/main.cpp` | Sonda asíncrona en tres pasos e indicaciones en Configuración. |
| `include/wifi_retry.hpp`, `src/esp32/wifi.cpp`, `src/esp32/wifi.hpp`, `src/esp32/proxy_chat.cpp` | Política de reconexión, reloj y estados de transporte. |
| `test/test_core/test_main.cpp`, `test/windows/test_desktop.cpp` | Pruebas de política Wi-Fi y regresión tras rechazo de TLS. |
| `Cargar ESP32.cmd`, `scripts/Cargar ESP32.ps1` | Asistente de carga y listado de puertos USB. |
| `README.md`, `informes/Fase_5.md` | Configuración, operación, pruebas y publicación. |
| `Calcu.exe`, `.pio/build/esp32/firmware.bin` | Generados localmente y excluidos de Git. |

## Pruebas aprobadas y fallidas

| Prueba | Resultado |
| --- | --- |
| PlatformIO Native | Aprobada. Operaciones científicas, formatos, edición, multitap, cambio de modo, historial, perfil LCD y reintentos de Wi-Fi con reloj y conexión simulados. |
| Windows C++ | Aprobada. Configuración, DPAPI, validación de URL, rechazo de certificado no confiable en GET y POST y recuperación de la interfaz tras el fallo. |
| Intermediario HTTPS | Siete pruebas aprobadas. TLS local verificado con certificado efímero, disponibilidad, autenticación, OpenAI simulado en dos turnos, diagnóstico positivo simulado y fallos de configuración, autenticación, cuota, respuesta y espera. |
| Comprobación sin clave | Aprobada. `--check-openai` devolvió 503 e indicó configurar `OPENAI_API_KEY` y `OPENAI_MODEL`. |
| Compilación ESP32 | Aprobada para el perfil provisional `esp32dev`. PlatformIO informó 46.024 bytes de RAM estática y 812.573 bytes de flash de programa. |
| Asistente USB `-List` | El primer intento falló por dos rutas candidatas de Python. Pasó al seleccionar una sola. PlatformIO no enumeró una placa durante la prueba. |
| Lanzador y ejecutable | Aprobados. `Abrir Calcu.cmd` recompiló y abrió la ventana mediante la asociación de Windows. `Calcu.exe` abrió directamente. |
| Consulta auténtica a OpenAI | Pendiente. `OPENAI_API_KEY` no estaba definida en el entorno del proceso, del usuario ni del equipo. |
| Carga USB, reconexión Wi-Fi y LCD en placa | Pendientes. No se conectó ni cargó hardware físico. |

## Problemas solucionados

- El botón de autorización web de Git Credential Manager aparecía deshabilitado. El propietario completó el flujo oficial por código de dispositivo con `mau07050709-dot`. Un push de prueba confirmó que la cuenta puede avanzar `main`.
- Windows PowerShell detectó dos ejecutables llamados `python.exe`. El asistente selecciona ahora un solo candidato y comunica si falta PlatformIO.
- El diagnóstico de la fase anterior terminaba al validar HTTPS y el token. Esta fase prueba OpenAI por separado y conserva como pendientes los pasos que aún no pudo verificar.

## Verificación visual

Se observó el inicio en calculadora y el panel de configuración. Al pulsar **Probar conexión** con campos vacíos, Windows mostró **Servidor HTTPS: Sin confirmar**, **Token: Pendiente**, **OpenAI: Pendiente** y la instrucción de introducir la URL terminada en `/ask`. Los estados positivos de OpenAI se probaron en el intermediario con una respuesta simulada. El doble clic literal desde el Explorador sigue pendiente porque la habilidad computer-use restringe ejecutar archivos de comandos desde allí.

## Limitaciones y avance

- **Software: 95 % estimado.** El flujo está implementado, el diagnóstico distingue las tres etapas y las regresiones pasan. Falta comprobar un servidor con certificado de despliegue y credenciales válidas contra OpenAI.
- **Integración física verificada: 0 %.** Se compiló el firmware y se comprobó el listado USB, con pines y modelos físicos pendientes de confirmar. Falta cargar la placa y verificar radio, reloj, LCD y teclado.
- El historial sigue limitado a cuatro turnos por sesión. El servidor sigue pensado para un prototipo con una solicitud por vez.

## GitHub

Se publicaron ambos commits en `main` mediante un avance normal. Se verificó el hash remoto con `git ls-remote` y que el commit de la Fase 4 es antecesor del de la Fase 5. Se revisaron los 48 archivos del índice para excluir claves, configuraciones privadas, binarios y temporales.

- [Fase 4 · `154e583`](https://github.com/mau07050709-dot/Calcu/commit/154e583d90e33376b9d4d3b877b5ef67c192027a)
- [Fase 5 · `d0c172c`](https://github.com/mau07050709-dot/Calcu/commit/d0c172c63b24ff2b02d8933beeb09abf6611badb)

## Recomendaciones para la siguiente fase

1. Configurar `OPENAI_MODEL` y el token en el servidor, iniciar con `--prompt-key` y exponer el intermediario con un certificado de confianza. Usar **Probar conexión** y enviar dos preguntas relacionadas.
2. Confirmar el perfil de placa y los componentes antes de abrir **Cargar ESP32.cmd**. Probar primero en serie y luego activar la LCD y el teclado con el mapa confirmado.
3. Medir en placa el tiempo de reconexión, probar un fallo de DNS o NTP y verificar que la interfaz conserve el borrador tras un fallo de red.
