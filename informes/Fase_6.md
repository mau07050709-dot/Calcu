# Calcu · Informe de la Fase 6

12 de noviembre de 2026

## Funcionalidades implementadas

- Al abrir **Abrir Calcu.cmd**, el simulador inicia su intermediario en segundo plano. El proceso padre usa tuberías anónimas para intercambiar el puerto y el token efímero, y cierra el intermediario al salir. El HTTP local se limita a `127.0.0.1` y WinHTTP evita proxies para esa conexión. El servidor externo y el firmware conservan HTTPS.
- Con la configuración externa vacía, Windows elige la conexión local automáticamente. Si falta la clave, conserva la pregunta y abre un asistente con campo enmascarado. **Configurar OpenAI** también lo abre directamente. La clave guardada se cifra con DPAPI fuera del proyecto y el proceso del servidor la lee al consultar.
- El modelo local predeterminado es `gpt-6-luna`, con `OPENAI_MODEL` como alternativa configurable. Las consultas usan Responses API y continúan en segundo plano. [Modelo GPT‑6 Luna en OpenAI Docs](https://developers.openai.com/api/docs/models/gpt-6-luna)
- Se conserva el historial por sesión, la paginación, el editor multitap, la combinación de modo y la configuración persistente. El motor matemático, la LCD 16×2, el firmware y **Cargar ESP32.cmd** se conservaron.

## Archivos modificados

| Archivo | Cambio |
| --- | --- |
| `server/desktop_proxy.py` | Proceso local, asistente Tkinter y almacenamiento DPAPI. |
| `src/windows/local_proxy.cpp`, `src/windows/local_proxy.hpp` | Inicio, generación del token, detección del puerto y cierre del proceso. |
| `src/windows/desktop_chat.cpp`, `src/windows/desktop_chat.hpp`, `src/windows/main.cpp` | HTTP limitado al loopback automático, diagnóstico y asistencia al faltar la clave. |
| `scripts/Compilar Calcu.ps1` | Enlace de BCrypt para el token efímero. |
| `server/test_desktop_proxy.py`, `test/windows/test_desktop.cpp` | Cobertura de DPAPI, ciclo de vida y conversación en loopback. |
| `README.md`, `informes/Fase_6.md` | Configuración, pruebas y estado de publicación. |
| `Calcu.exe`, `.pio/build/esp32/firmware.bin` | Binarios generados localmente y excluidos de Git. |

## Pruebas y errores encontrados

| Prueba | Resultado |
| --- | --- |
| `Abrir Calcu.cmd` | Aprobada. Se invocó por la asociación de Windows, recompiló y abrió Calcu con su servidor. El doble clic literal desde el Explorador queda pendiente por la restricción de computer-use sobre archivos de comandos. |
| Escucha del servidor de Calcu | Aprobada. Se observó una dirección de escucha `127.0.0.1` con puerto asignado dinámicamente. |
| Teclado y asistente | Aprobada visualmente hasta la credencial. Se entró con el botón de modo, se escribió `HOLA` por multitap y se envió. La pantalla indicó **Clave API falta** y se abrió el asistente. La combinación de modo se comprobó en la suite del núcleo. |
| PlatformIO Native | Aprobada. Operaciones científicas, edición, multitap, modo, historial, paginación y regresiones. |
| Windows C++ y Python | Aprobadas. Diez pruebas Python, incluida la integración WinHTTP de dos turnos y una respuesta larga con un proveedor simulado. También se verificaron autenticación local, ciclo de vida por EOF, rechazo de certificados no confiables y almacenamiento DPAPI con un valor sintético. |
| Firmware ESP32 | Compilación aprobada para el perfil provisional `esp32dev`. 46.024 bytes de RAM estática y 812.573 bytes de flash. |
| Recorrido auténtico de OpenAI | Pendiente. No había una clave API disponible en el proceso, usuario, equipo ni almacenamiento local al comprobarlo. No se obtuvo una respuesta auténtica ni se probó la paginación de esa respuesta. |

El error previo «Configura proxy» procedía de exigir una URL externa y un token que la instalación de Windows no tenía. La conexión local elimina esos dos pasos de configuración y, cuando falta la clave de OpenAI, abre el asistente. El intermediario acepta saltos de línea LF y CRLF en el protocolo privado de arranque para que funcione tanto con el ejecutable como con el banco de pruebas de Windows.

## OpenAI real y siguiente paso

Pega una clave válida en **Calcu · Configurar OpenAI**, pulsa **Guardar clave**, cierra el asistente y vuelve al borrador de `HOLA`. Al reenviar, Calcu leerá la clave sin reiniciar el servidor. Si el servicio indica un problema de cuota o acceso al modelo, revisa el proyecto de API y su facturación. La prueba con un proveedor simulado comprueba el transporte y el contexto, y no confirma acceso a OpenAI.

**Avance estimado del software: 97 %.** El arranque local, la configuración y los dos turnos por el transporte están verificados. Falta la conversación auténtica completa y su diagnóstico contra una cuenta con cuota.

**Integración física verificada: 0 %.** Se compiló el firmware. Queda confirmar la placa, el LCD, el teclado y los GPIO, cargar por USB y probar Wi-Fi, NTP y HTTPS en el ESP32.

## GitHub

Se publicó la [implementación de la Fase 6 (`744f48e`)](https://github.com/mau07050709-dot/Calcu/commit/744f48ebbf0fb780a63263f4ad97204031297e75) en `main`, conservando los commits anteriores. Se verificó el hash remoto y la página del commit respondió HTTP 200. Se revisaron los 53 archivos del índice para excluir claves, configuraciones privadas, binarios y temporales.
