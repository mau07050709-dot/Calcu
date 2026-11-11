# Calcu

Continúa cada fase en este proyecto y conserva el historial de Git. Antes de entregar una fase:

1. Ejecuta las pruebas pertinentes de Windows, del motor compartido, del intermediario y de compilación para ESP32.
2. Actualiza README y guarda el informe de la fase en `informes/Fase_N.md`. Separa las pruebas reales, las simuladas y las pendientes.
3. Revisa los cambios y el índice de Git. Incluye el código, README y el informe. Excluye claves, contraseñas, tokens, `.env`, certificados privados, datos locales, temporales y binarios generados.
4. Crea un commit descriptivo. Consulta `origin/main` y conserva los cambios remotos. Publica en `main` sin forzar y verifica el hash remoto.
5. Informa cualquier conflicto o fallo de autenticación. Preserva los cambios locales y ajenos.

La clave de OpenAI vive en el servidor. Realiza una consulta auténtica únicamente si hay una clave disponible para esa prueba.
