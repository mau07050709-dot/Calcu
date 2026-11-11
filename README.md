# Calcu · Fase 2

Calculadora científica y cliente de ChatGPT en C++17. La aplicación de Windows y el firmware ESP32 comparten el motor matemático, el editor, el teclado multitap, los modos y el renderizado de LCD 16×2.

## Abrir en Windows

Desde la carpeta del proyecto, abre **Abrir Calcu.cmd** con doble clic. En el primer inicio el lanzador genera `Calcu.exe` para Windows x64. En los siguientes inicios vuelve a compilarlo si cambian los archivos fuente. También puedes abrir el ejecutable generado directamente.

Para recompilar necesitas un compilador MinGW-w64 compatible con C++17. El lanzador reconoce `g++` o `clang++` en `PATH`, varias rutas habituales de MSYS2 y la variable `CALCU_CXX` si necesitas indicar una ruta concreta.

- En **Calculadora**, usa los botones de función, los dígitos y el paréntesis de cierre. Las flechas mueven el cursor. **DEG/RAD** cambia y guarda la unidad trigonométrica.
- Pulsa **Modo** o las dos teclas que aparecen en ese botón. La secuencia inicial es `*#`. En **Configuración** puedes elegir otra. El prefijo es `*` o `#`, y la segunda tecla puede ser `#`, `*`, `0`, `7`, `8` o `9`. La tecla especial que no se usa como prefijo conserva la función de confirmar por sí sola. Los botones de operaciones siguen disponibles.
- En **ChatGPT**, pulsa repetidamente un dígito para elegir una letra. **Confirmar letra** fija el carácter al instante. **ABC / 123** cambia a escritura numérica y las flechas permiten corregir en medio del borrador. **Página anterior** y **Página siguiente** permiten revisar preguntas largas y respuestas.
- Para probar sin conexión, activa **Usar respuesta de demostración local** en Configuración. Para una consulta real, desactívala e introduce la URL HTTPS completa del intermediario y su token de dispositivo. WinHTTP valida el certificado con la confianza de Windows. La URL, el modo de demostración, el atajo y la unidad angular se guardan en `calcu.ini` junto al ejecutable. Windows cifra el token con DPAPI para la cuenta actual.

## Simulador de consola para desarrollo

Instala [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) y un compilador de C++17. El entorno [PlatformIO Native](https://docs.platformio.org/en/latest/platforms/native.html) utiliza GCC de forma predeterminada. En Windows puedes usar MinGW-w64 con `g++` en `PATH`. Ejecuta desde la carpeta que contiene `platformio.ini`.

```powershell
pio run -e native
& .\.pio\build\native\program.exe
```

En macOS o Linux ejecuta `./.pio/build/native/program` después de compilar.

Introduce una tecla o una secuencia y pulsa Enter en la computadora para transmitirla. Cada línea agrupa pulsaciones rápidas y termina con una pausa de 750 ms. Usa `:wait 400` para avanzar el reloj del simulador o `:quit` para salir. La respuesta de demostración es local.

Prueba estas secuencias, una por línea.

```text
2*1#3#
*#
44
666
555
2
#
*3
*2
*#
:quit
```

La primera línea inserta `+` desde la paleta y calcula `2+3`. Las siguientes escriben y envían `HOLA`, cambian de página y regresan a la calculadora.

## Teclas

El mapa lógico de prueba usa los doce símbolos `0–9`, `*` y `#`. Introduce los atajos como secuencias en un plazo de 1,2 s. Para cambiar el atajo de modo, ajusta `shortcutPrefix` y `modeKey` en [include/config.hpp](include/config.hpp). La otra tecla especial queda como tecla de confirmación.

| Secuencia | Calculadora | ChatGPT |
| --- | --- | --- |
| `*#` | Entrar a ChatGPT | Regresar a la calculadora |
| `0–9` | Insertar dígitos | ABC: 2 ABC, 3 DEF, 4 GHI, 5 JKL, 6 MNO, 7 PQRS, 8 TUV, 9 WXYZ, 0 espacio, 1 puntuación. En 123 inserta dígitos. |
| `*1` | Abrir o cerrar la paleta de símbolos | Alternar ABC y 123 |
| `*2` / `*3` | Mover el cursor, seleccionar un símbolo o recorrer el resultado | Abrir la vista de la pregunta desde el inicio y luego recorrer páginas, o recorrer una respuesta |
| `*4` o `**` | Borrar a la izquierda o volver del resultado al editor | Borrar la última letra o volver de la respuesta al editor |
| `*5` | Limpiar expresión | Limpiar pregunta |
| `*6` | Alternar DEG y RAD | Confirmar la letra para poder repetir la misma tecla |
| `#` | Evaluar o insertar el símbolo seleccionado | Enviar la pregunta o volver de la respuesta al editor |

En ABC, pulsa repetidamente la misma tecla dentro de 750 ms para recorrer sus letras. Espera, confirma con `*6` o pulsa otra tecla para fijar la letra. La tecla 1 recorre `.,?!:;'-`. Si falla una consulta, vuelve al editor con `#` y reintenta con `#`. El borrador se conserva hasta recibir una respuesta.

La paleta incluye `+ - * / . ^ % ( ) sqrt( cbrt( sin( cos( tan( log( ln( pi e`. `log` es base 10 y `ln` es natural. Usa `*` para multiplicar, por ejemplo `2*(3+4)`. `x%` vale `x/100`, así que el 10 % de 200 se escribe `200*10%`. Las potencias se asocian a la derecha y `-2^2` da `-4`. DEG es el modo inicial y la unidad se aplica a las funciones trigonométricas. Las expresiones se limitan a 128 caracteres y las preguntas a 240. El intermediario adapta las respuestas a ASCII y a 768 caracteres.

## Compilar y configurar el ESP32

```powershell
pio run -e esp32
```

`esp32dev` es el [perfil provisional de compilación para un ESP32 clásico](https://docs.platformio.org/en/latest/boards/espressif32/esp32dev.html). Antes de cargar, confirma el ID de placa de PlatformIO y sustitúyelo en [platformio.ini](platformio.ini).

En Windows, si Xtensa falla con `CreateProcess: No such file or directory`, abre el proyecto desde una ruta corta, por ejemplo `C:\Calcu`. Si la carpeta de paquetes de PlatformIO también tiene una ruta larga, configura `PLATFORMIO_CORE_DIR` con una ruta corta. La compilación de esta entrega se verificó desde una letra de unidad temporal.

Copia [include/local_config.example.hpp](include/local_config.example.hpp) a `include/local_config.hpp`. Ahí se introducen los datos de red y se habilitan los adaptadores físicos que correspondan. Los GPIO quedan sin asignar en el proyecto inicial. Hay adaptadores para una matriz pasiva 4×3 y una LCD compatible con HD44780 en paralelo de 4 bits. Para otros protocolos o dimensiones de teclado, adapta `BoardKeypad` o `BoardDisplay` en `src/esp32/hardware.*`. El teclado emite un símbolo por pulsación y la pantalla recibe filas de 16 caracteres.

Una vez confirmada la placa, puedes probar por USB/serie con los adaptadores físicos desactivados. El puerto serie recibe las mismas teclas y muestra la cuadrícula 16×2. Para multitap, envía una secuencia en el monitor o usa `*6` entre letras de la misma tecla.

```powershell
pio run -e esp32 -t upload
pio device monitor -e esp32 --echo
```

## Wi-Fi e intermediario HTTPS

1. En el equipo servidor, configura las variables `OPENAI_API_KEY`, `OPENAI_MODEL` (ID de un modelo disponible en tu proyecto y compatible con Responses API) y `CALCU_DEVICE_TOKEN`. Genera un token aleatorio con `python -c "import secrets; print(secrets.token_urlsafe(32))"`. La clave de OpenAI se configura únicamente en el servidor.
2. Ejecuta `python server/proxy.py` con Python 3.10 o posterior. Escucha en `127.0.0.1:8080`. La variable `CALCU_PORT` permite cambiar el puerto.
3. En ese mismo equipo, publica el servicio mediante HTTPS. [server/Caddyfile.example](server/Caddyfile.example) muestra la configuración mínima para Caddy. Sustituye el dominio y asegúrate de que el ESP32 pueda resolverlo y alcanzar su puerto HTTPS. El intermediario devuelve `Content-Length` para que el firmware pueda verificar el tamaño antes de leer.
4. En `include/local_config.hpp`, completa SSID, contraseña, URL HTTPS completa con `/ask`, token del dispositivo y certificado CA raíz en PEM de la cadena que usará el dominio. El ESP32 comprueba el certificado y el nombre del servidor, y sincroniza el reloj con NTP. Si tu red requiere otro servidor NTP, cambia `timeServer`.
5. Compila y carga nuevamente. Cambia a ChatGPT, espera `WiFi listo`, escribe la pregunta y envíala.

El contrato es `POST /ask` con `Authorization: Bearer <token del dispositivo>` y JSON `{"question":"..."}`. El servidor consulta [Responses API](https://developers.openai.com/api/reference/resources/responses/methods/create) y devuelve `{"answer":"..."}` o un error HTTP. El firmware mantiene la interfaz disponible durante HTTPS con una tarea de FreeRTOS. Reintenta conectar Wi-Fi cada 15 s cuando pierde la conexión y muestra fallos de red, TLS, autenticación o tiempo de espera. Cada pregunta se envía una vez por pulsación. El prototipo atiende una consulta por vez y cada pregunta se procesa de forma independiente.

## Pruebas

En Windows se comprobó el lanzador con el ejecutable ausente, con fuentes actualizadas y con el ejecutable vigente. Se abrió mediante la asociación de Windows y se inspeccionó la aplicación en pantalla. Con clics se calculó `sqrt(9) = 3`, se cambió de modo, se escribió `B` con dos pulsaciones rápidas y se navegó en ambas direcciones por una respuesta de demostración. Se verificó que RAD persistía al reabrir. Quedan pendientes el doble clic literal en el Explorador, una consulta con un intermediario HTTPS real y el montaje del hardware.

```powershell
pio test -e native
python -B -m unittest discover -s server -v
```

- C++ cubre jerarquía, funciones científicas, errores, DEG/RAD, multitap y desbordamiento de reloj, edición, cambio de modo, navegación, respuesta durante un cambio de modo y recuperación de borradores.
- Python prueba el contrato HTTP en localhost y la conversión de respuestas con una llamada a OpenAI simulada.
- El recorrido de consola anterior verifica la calculadora, multitap, el LCD y la respuesta paginada desde el punto de entrada normal.

La Fase 2 volvió a compilar el perfil provisional `esp32dev` y pasó la suite C++ y las cuatro pruebas de Python. La prueba `test/windows/test_desktop.cpp` comprueba la persistencia de atajo, ángulo y conexión, el cifrado del token, la validación de URL y la respuesta de demostración. Para repetirla con un compilador compatible disponible como `g++` (o reemplázalo por `clang++`):

```powershell
g++ -std=c++17 -I include -I src/windows test/windows/test_desktop.cpp src/windows/settings.cpp src/windows/desktop_chat.cpp -static -lwinhttp -lcrypt32 -o "$env:TEMP/calcu-test.exe"
& "$env:TEMP/calcu-test.exe"
```

## Datos que debemos pedir para el montaje

- **ESP32** — modelo exacto de la placa y del módulo, variante del chip, foto de ambas caras y diagrama de pines.
- **LCD** — referencia o foto de la placa y del conector, controlador e interfaz (paralelo, I²C u otra), tensión de alimentación y niveles lógicos. Si tiene adaptador I²C, indica el chip y la dirección si se conoce.
- **Teclado** — foto y referencia, disposición y leyendas, cantidad de filas y columnas, orden de los contactos o esquema y si usa diodos o un controlador.
- **Montaje** — GPIO ya ocupados, cableado o esquema si existe, alimentación prevista y disponibilidad de adaptación de niveles.
