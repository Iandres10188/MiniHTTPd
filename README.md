# MiniHTTPd

Servidor HTTP/1.1 minimo escrito en C puro, sin bibliotecas HTTP externas.
Usa `epoll` para atender multiples clientes de forma concurrente con un solo
hilo (arquitectura event-driven) y soporta conexiones persistentes (keep-alive).

## Caracteristicas

- Sirve archivos estaticos (HTML, CSS, JS, PNG, JPG, ...).
- Implementa el metodo `GET` y analiza encabezados basicos (`Host`,
  `Connection`, `User-Agent`).
- Concurrencia con `epoll` (multiples clientes simultaneos, no bloqueante).
- Conexiones persistentes HTTP/1.1 (keep-alive).
- Codigos de estado: `200`, `400`, `403`, `404`, `405`, `500`.
- Tipos MIME segun la extension del archivo.
- Defensa contra vulnerabilidades comunes (ver seccion Seguridad).

## Estructura del proyecto

```
minihttpd/
├── Makefile
├── README.md
├── include/
│   ├── http.h
│   ├── server.h
│   ├── mime.h
│   └── files.h
├── src/
│   ├── main.c      # punto de entrada
│   ├── server.c    # bucle de eventos epoll, sockets
│   ├── http.c      # parsing GET y generacion de respuestas
│   ├── mime.c      # tabla de tipos MIME
│   └── files.c     # acceso seguro al contenido estatico
└── www/
    ├── index.html
    ├── style.css
    └── image.png
```

## Compilacion

Requiere Linux (usa `epoll`) y `gcc`.

```bash
make          # compila el binario ./minihttpd
make run      # compila y ejecuta en el puerto 8080
make clean    # elimina objetos y binario
```

El Makefile activa advertencias estrictas (`-Wall -Wextra -Wpedantic`) y
endurecimiento (`-D_FORTIFY_SOURCE=2 -fstack-protector-strong`).

## Uso

```bash
./minihttpd            # puerto por defecto 8080
./minihttpd 9000       # puerto personalizado
```

Luego abrir en el navegador: `http://localhost:8080/`

### Ejemplos con curl

```bash
curl -i http://localhost:8080/                 # 200 OK, index.html
curl -i http://localhost:8080/style.css        # 200 OK, text/css
curl -i http://localhost:8080/noexiste         # 404 Not Found
curl -i -X POST http://localhost:8080/         # 405 Method Not Allowed
curl -i 'http://localhost:8080/../../etc/passwd' # 403/404 (bloqueado)
```

## Arquitectura (event-driven)

`main()` crea un socket de escucha no bloqueante y lo registra en una
instancia de `epoll`. El bucle principal (`server_run`) espera eventos con
`epoll_wait`:

- Evento en el socket de escucha -> `accept()` de todas las conexiones
  pendientes; cada cliente se vuelve no bloqueante y se registra en epoll.
- Evento `EPOLLIN` en un cliente -> se leen los bytes disponibles, se
  acumulan en un buffer por conexion y, cuando se detecta el fin de las
  cabeceras (`\r\n\r\n`), se parsea y se responde.

Cada conexion mantiene su propio estado (`connection_t`), lo que permite
lecturas parciales y reutilizar el socket en peticiones sucesivas (keep-alive).

## Seguridad

- **Directory Traversal**: `files.c` usa `realpath()` para canonicalizar la
  ruta solicitada y verifica que el resultado siga estando dentro de la raiz
  `www/`. Solicitudes como `/../../etc/passwd` quedan fuera de la raiz y se
  rechazan.
- **Buffer Overflows**: no se usa `strcpy` ni `sprintf`. Solo `snprintf`,
  `memcpy` con limites y `sscanf` con anchos de campo acotados.
- **Metodos invalidos**: cualquier metodo distinto de `GET` devuelve
  `405 Method Not Allowed` (con encabezado `Allow: GET`).
- **Tamano de solicitud**: si la linea/cabeceras superan `MAX_REQUEST`
  (8 KiB) se responde `400 Bad Request` y se cierra la conexion.
- `SIGPIPE` se ignora para que un cliente que cierra abruptamente no termine
  el proceso.

## Limitaciones conocidas

- Solo se procesa una solicitud por ciclo de lectura; el pipelining HTTP
  (varias peticiones enviadas sin esperar respuesta) no se soporta, aunque si
  el keep-alive secuencial (el caso real de los navegadores).
- Las escrituras usan reintento sobre `EAGAIN` en lugar de registrar
  `EPOLLOUT`; suficiente para contenido pequeno/mediano.

## Licencia

Proyecto academico. Uso libre con fines educativos.
