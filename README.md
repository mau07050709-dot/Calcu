# Calcu · Fase 4

Calculadora científica y cliente de ChatGPT en C++17. La aplicación de Windows y el firmware ESP32 comparten el motor matemático, el editor, el teclado multitap, los modos y el renderizado de LCD 16×2.

## Abrir en Windows

En `Desktop\Calcu`, abre **Abrir Calcu.cmd** con doble clic. El lanzador usa `Calcu.exe` si está actualizado y lo recompila si falta el binario o si hay cambios en el código. También puedes abrir **Calcu.exe** directamente. El ejecutable incluido es para Windows x64 y usa bibliotecas del sistema.

Para recompilar necesitas un compilador MinGW-w64 compatible con C++17. El lanzador reconoce `g++` o `clang++` en `PATH`, varias rutas habituales de MSYS2 y la variable `CALCU_CXX` si necesitas indicar una ruta concreta.

- En **Calculadora**, usa los botones de función, los dígitos y el paréntesis de cierre. La pantalla gráfica dibuja el árbol de la expresión. Puedes situar el cursor con el ratón o con las flechas.
- **Decimal / Fracción / SCI** cambia y guarda la presentación. La vista gráfica marca con `≈` una aproximación. **DEG/RAD** cambia y guarda la unidad trigonométrica.
- **Ans** inserta el último resultado válido de la sesión. **Historial** permite recorrer hasta ocho cálculos válidos y **Editar cálculo** recupera la expresión y la unidad angular originales. Al recalcular una entrada con `Ans`, se usa el último resultado válido de la sesión.
- Pulsa **Modo** o las dos teclas que aparecen en ese botón. La secuencia inicial es `*#`. En **Configuración** puedes elegir otra. El prefijo es `*` o `#`, y la segunda tecla puede ser `#`, `*`, `0`, `7`, `8` o `9`. La tecla especial que no se usa como prefijo conserva la función de confirmar por sí sola. Los botones de operaciones siguen disponibles.
- En **ChatGPT**, pulsa repetidamente un dígito para elegir una letra. **Confirmar letra** fija el carácter al instante. **ABC / 123** cambia a escritura numérica y las flechas permiten corregir en medio del borrador. **Página anterior** y **Página siguiente** permiten revisar preguntas largas y respuestas.
- Para probar sin conexión, activa **Usar respuesta de demostración local** en Configuración. Para una consulta real, desactívala e introduce la URL HTTPS completa del intermediario y su token de dispositivo. Pulsa **Probar conexión** para verificar HTTPS y el token, y después **Guardar**. El certificado se verifica con la confianza de Windows. La URL, el modo de demostración, el atajo, el formato y la unidad angular se guardan en `calcu.ini` junto al ejecutable. Windows cifra el token con DPAPI para la cuenta actual.

- **Historial de conversación** abre el turno más reciente. Vuelve a pulsarlo para recorrer turnos anteriores y usa los botones de página para leer cada pregunta y respuesta. Calcu conserva hasta cuatro turnos exitosos en memoria y los envía como contexto con la siguiente pregunta. El historial se reinicia al cerrar, reiniciar el ESP32 o cambiar de conexión en Windows.

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
| `*7` / `*8` | Mover el cursor | Mover el cursor dentro de la pregunta para corregirla |
| `*9` | Sin acción asignada | Entrar al historial y recorrer turnos anteriores |
| `#` | Evaluar o insertar el símbolo seleccionado | Enviar la pregunta o volver de la respuesta al editor |

Para un atajo de modo personalizado, las funciones de `*7`, `*8` y `*9` se asignan en ese orden a los primeros tres sufijos libres de `7`, `8`, `9`, `0`. Se omite la tecla reservada para cambiar de modo y se usa el prefijo configurado. En el historial, `*2` y `*3` recorren las páginas. La tecla de confirmar vuelve a la vista anterior.

En ABC, pulsa repetidamente la misma tecla dentro de 750 ms para recorrer sus letras. Espera, confirma con `*6` o pulsa otra tecla para fijar la letra. La tecla 1 recorre `.,?!:;'-`. Si falla una consulta, vuelve al editor con `#` y reintenta con `#`. El borrador se conserva hasta recibir una respuesta.

La paleta incluye los símbolos matemáticos y las funciones del motor. En la LCD, `*1` abre la paleta. Desde su primera opción, `*2` una vez lleva a **Historial** y dos veces a **Formato**. `#` confirma. En el historial, `*2` va a cálculos anteriores, `*3` a más recientes y `#` recupera la entrada para editarla. El cursor permite recorrer expresiones de hasta 128 caracteres. El teclado, la LCD y el puerto serie usan esta representación lineal.

| Función | Ejemplo |
| --- | --- |
| Fracción | `1/3+1/6` da `1/2` en Fracción y `0.5` en Decimal |
| Potencias | `2^3^2` da `512` y `-2^2` da `-4` |
| Raíces | `sqrt(9)`, `cbrt(-8)` o `root(8,3)` con valor e índice |
| Factorial y conteo | `5!`, `ncr(5,2)` y `npr(5,2)` |
| Logaritmos | `log(100)` para base 10, `ln(e)` para natural y `log(8,2)` para base variable |
| Trigonometría | `sin(30)`, `cos(60)`, `tan(45)`, `asin(.5)`, `acos(.5)` y `atan(1)` |
| Científica | `2.5E-3` equivale a `0.0025` |
| Constantes y memoria | `pi`, `e` y `Ans` |

Usa `*` para multiplicar y `x%` para `x/100`. Un radicando negativo requiere índice impar entero. Los factoriales, las combinaciones y las permutaciones requieren argumentos que el motor pueda reconocer como enteros exactos. El cálculo conserva fracciones reducidas de 64 bits mientras sus operaciones lo permitan. Los resultados fuera de esa capacidad y las funciones que necesitan aproximación usan `double`. La unidad angular se aplica a la trigonometría directa e inversa. La calculadora arranca en DEG de forma predeterminada.

Las preguntas se limitan a 240 caracteres. El intermediario adapta las respuestas a ASCII y a 768 caracteres.

## Compilar y configurar el ESP32

```powershell
pio run -e esp32
```

`esp32dev` es el [perfil provisional de compilación para un ESP32 clásico](https://docs.platformio.org/en/latest/boards/espressif32/esp32dev.html). Antes de cargar, confirma el ID de placa de PlatformIO y sustitúyelo en [platformio.ini](platformio.ini).

En Windows, si Xtensa falla con `CreateProcess: No such file or directory`, abre el proyecto desde una ruta corta, por ejemplo `C:\Calcu`. Si la carpeta de paquetes de PlatformIO también tiene una ruta larga, configura `PLATFORMIO_CORE_DIR` con una ruta corta. La compilación de esta entrega se verificó desde una letra de unidad temporal.

Copia [include/local_config.example.hpp](include/local_config.example.hpp) a `include/local_config.hpp`. Ahí se introducen los datos de red y se habilitan los adaptadores físicos que correspondan. Los GPIO quedan sin asignar en el proyecto inicial. Hay adaptadores para una matriz pasiva 4×3 y una LCD compatible con HD44780 en paralelo de 4 bits. Selecciona `LcdController::serial` o `LcdController::hd44780Parallel`, y completa los GPIO, las direcciones de fila, el tipo de pull-up y el mapa lógico del teclado cuando se confirmen. El arranque valida pines repetidos, solapamientos y mapa de teclas. Si un adaptador es inválido se usa el puerto serie. Para otros protocolos o dimensiones de teclado, adapta `BoardKeypad` o `BoardDisplay` en `src/esp32/hardware.*`. El teclado emite un símbolo por pulsación y la pantalla recibe filas de 16 caracteres.

Una vez confirmada la placa, puedes probar por USB/serie con los adaptadores físicos desactivados. El puerto serie recibe las mismas teclas y muestra la cuadrícula 16×2. Para multitap, envía una secuencia en el monitor o usa `*6` entre letras de la misma tecla.

```powershell
pio run -e esp32 -t upload
pio device monitor -e esp32 --echo
```

## Activar ChatGPT real con un intermediario HTTPS

1. En el equipo servidor, crea u obtén una clave de un proyecto de OpenAI con acceso a Responses API. Identifica el ID de un modelo habilitado en ese mismo proyecto. [Claves de la API](https://platform.openai.com/api-keys) · [Referencia de Responses API](https://developers.openai.com/api/reference/resources/responses/methods/create)
2. Abre PowerShell en la raíz del proyecto **en el servidor**. El siguiente bloque lee la clave sin mostrarla, pide el modelo y genera un token separado para Calcu. Las variables se aplican a la sesión actual.

   ```powershell
   $env:OPENAI_API_KEY = [System.Net.NetworkCredential]::new(
       '', (Read-Host 'Clave API de OpenAI' -AsSecureString)).Password
   $env:OPENAI_MODEL = Read-Host 'ID de un modelo habilitado para Responses API'
   $env:CALCU_DEVICE_TOKEN = python -c "import secrets; print(secrets.token_urlsafe(32))"
   ```

3. Sirve el intermediario con Python 3.10 o posterior y elige una forma de ofrecer HTTPS.

   - **Con Caddy u otro proxy inverso:** ejecuta `python server/proxy.py`. Escucha por HTTP solo en `127.0.0.1:8080`. Configura el dominio y HTTPS en el proxy inverso. [Ejemplo de configuración de Caddy](server/Caddyfile.example)
   - **TLS directo:** en la misma sesión, define `CALCU_TLS_CERT` con la ruta PEM de la cadena de certificados y `CALCU_TLS_KEY` con la ruta de la clave privada PEM. El certificado debe cubrir el nombre de la URL y ser confiable para el cliente. `CALCU_BIND` permite elegir la interfaz del servidor y `CALCU_PORT` cambia el puerto (8443 por defecto en TLS). Ejecuta `python server/proxy.py`.

4. En Calcu para Windows, abre **Configuración**, desactiva **Usar respuesta de demostración local** y pega la URL HTTPS completa, terminada en `/ask`, y el valor de `CALCU_DEVICE_TOKEN`. **Probar conexión** consulta el endpoint autenticado `/health`. Si se confirma HTTPS y token, guarda y envía una pregunta desde el modo ChatGPT. Esa pregunta verifica el acceso a OpenAI y al modelo. El botón de prueba no consume una consulta de OpenAI.
5. En el ESP32, copia el ejemplo a `include/local_config.hpp` e introduce SSID, contraseña, URL `/ask`, token del dispositivo y CA raíz en PEM. La URL debe ser alcanzable desde la red de la placa y coincidir con el certificado. El ESP32 sincroniza su reloj mediante NTP. Si tu red necesita otro servidor NTP, cambia `timeServer`. Confirma la placa antes de compilar y cargar.

La clave API se usa exclusivamente en el proceso servidor. Guarda certificados privados y credenciales fuera de la carpeta versionada. El token de dispositivo autoriza el intermediario y es distinto de la clave API.

El contrato usa `Authorization: Bearer <token del dispositivo>`. `GET /health` comprueba el acceso al intermediario. `POST /ask` acepta `{"question":"...","history":[{"question":"...","answer":"..."}]}` y devuelve `{"answer":"..."}` o un error HTTP. El historial es opcional para clientes anteriores. El servidor reconstruye los mensajes de Responses API con los roles de usuario y asistente y usa `store: false`. [Gestión manual del contexto en Responses API](https://developers.openai.com/api/docs/guides/conversation-state)

Windows usa un trabajador WinHTTP y el ESP32 usa una tarea de FreeRTOS. La interfaz sigue disponible durante la consulta. El firmware reintenta Wi-Fi cada 15 segundos y muestra estados de red, reloj, TLS, autenticación o tiempo de espera. Un fallo conserva el borrador para corregirlo o reintentar.

## Pruebas

Consulta el [informe de Fase 4](informes/Fase_4.md) para los resultados de compilación, pruebas y comprobación visual.

```powershell
pio test -e native
python -B -m unittest discover -s server -v
```

- C++ cubre jerarquía, funciones científicas, errores, DEG/RAD, multitap y desbordamiento de reloj, edición, cambio de modo, navegación, respuesta durante un cambio de modo y recuperación de borradores.
- Python prueba el contrato HTTP en localhost y la conversión de respuestas con una llamada a OpenAI simulada.
- El recorrido de consola anterior verifica la calculadora, multitap, el LCD y la respuesta paginada desde el punto de entrada normal.

La Fase 4 volvió a compilar el perfil provisional `esp32dev` y pasó la suite C++ y las seis pruebas de Python. La prueba `test/windows/test_desktop.cpp` comprueba la persistencia de atajo, ángulo y conexión, el cifrado del token, la validación de URL y la respuesta de demostración. Para repetirla con un compilador compatible disponible como `g++` (o reemplázalo por `clang++`):

```powershell
g++ -std=c++17 -I include -I src/windows test/windows/test_desktop.cpp src/windows/settings.cpp src/windows/desktop_chat.cpp src/core/chat.cpp -static -lwinhttp -lcrypt32 -o "$env:TEMP/calcu-test.exe"
& "$env:TEMP/calcu-test.exe"
```

Si `openssl` está en `PATH`, la suite Python genera un certificado efímero y prueba dos turnos por HTTPS con OpenAI simulado. Puedes asignar la ruta del binario de prueba de Windows a `CALCU_TEST_WINHTTP_EXE` para comprobar también que el cliente rechaza el certificado efímero sin confianza en el almacén de Windows.

## Publicación por fase

Antes de publicar, ejecuta las pruebas, revisa el índice de Git y excluye credenciales, datos locales y binarios. Añade el código, README e informe de la fase. Conserva el historial remoto y sincroniza `main` sin forzar. Después de `git push origin main`, compara `git rev-parse HEAD` con `git ls-remote origin refs/heads/main`. Si GitHub pide autenticación o hay un conflicto, conserva los cambios y resuelve el bloqueo antes de volver a publicar.

## Datos que debemos pedir para el montaje

- **ESP32** — modelo exacto de la placa y del módulo, variante del chip, foto de ambas caras y diagrama de pines.
- **LCD** — referencia o foto de la placa y del conector, controlador e interfaz (paralelo, I²C u otra), tensión de alimentación y niveles lógicos. Si tiene adaptador I²C, indica el chip y la dirección si se conoce.
- **Teclado** — foto y referencia, disposición y leyendas, cantidad de filas y columnas, orden de los contactos o esquema y si usa diodos o un controlador.
- **Montaje** — GPIO ya ocupados, cableado o esquema si existe, alimentación prevista y disponibilidad de adaptación de niveles.
