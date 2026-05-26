#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>

/* Limites del servidor (defensa contra solicitudes abusivas) */
#define DEFAULT_PORT      8080
#define MAX_EVENTS        1024     /* eventos por epoll_wait        */
#define MAX_REQUEST       8192     /* tamano maximo de la solicitud */
#define MAX_URI           2048     /* tamano maximo del URI         */
#define READ_CHUNK        4096     /* bytes leidos por iteracion    */
#define WEB_ROOT          "www"    /* directorio raiz de contenido  */

/*
 * Estado de una conexion cliente.
 * Cada descriptor activo en epoll tiene asociada una de estas
 * estructuras para soportar lecturas parciales y conexiones
 * persistentes (HTTP/1.1 keep-alive).
 */
typedef struct {
    int    fd;                    /* descriptor del socket cliente   */
    char   buf[MAX_REQUEST + 1];  /* buffer de acumulacion de bytes  */
    size_t buf_len;               /* bytes acumulados en buf         */
} connection_t;

/* Crea, enlaza y deja en escucha un socket TCP no bloqueante. */
int server_listen(int port);

/* Bucle principal de eventos basado en epoll. */
int server_run(int listen_fd);

/* Marca un descriptor como no bloqueante (O_NONBLOCK). */
int set_nonblocking(int fd);

#endif /* SERVER_H */
