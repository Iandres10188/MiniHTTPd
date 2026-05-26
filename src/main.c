#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

/*
 * MiniHTTPd - servidor HTTP/1.1 minimo basado en epoll.
 *
 * Uso:  ./minihttpd [puerto]
 * Si no se indica puerto, se usa DEFAULT_PORT (8080).
 * El contenido estatico se sirve desde el directorio "www/".
 */
int main(int argc, char *argv[])
{
    /* Ignoramos SIGPIPE: si un cliente cierra mientras escribimos,
     * write() devolvera EPIPE en vez de matar el proceso. */
    signal(SIGPIPE, SIG_IGN);

    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Puerto invalido: %s\n", argv[1]);
            return EXIT_FAILURE;
        }
    }

    int listen_fd = server_listen(port);
    if (listen_fd < 0) {
        fprintf(stderr, "No se pudo iniciar el servidor en el puerto %d\n", port);
        return EXIT_FAILURE;
    }

    printf("MiniHTTPd iniciado en http://localhost:%d/ (raiz: %s/)\n",
           port, WEB_ROOT);

    return server_run(listen_fd) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
