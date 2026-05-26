#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

/**
 * MiniHTTPd - Servidor HTTP estático basado en epoll
 * Uso: ./minihttpd [puerto] (Por defecto: 8080)
 */
int main(int argc, char *argv[])
{
    /* Previene que el proceso muera si un cliente desconecta abruptamente en plena escritura */
    signal(SIGPIPE, SIG_IGN);

    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Puerto inválido: %s\n", argv[1]);
            return EXIT_FAILURE;
        }
    }

    int listen_fd = server_listen(port);
    if (listen_fd < 0) {
        fprintf(stderr, "No se pudo iniciar el servidor en el puerto %d\n", port);
        return EXIT_FAILURE;
    }

    printf("MiniHTTPd iniciado en http://localhost:%d/ (raíz: %s/)\n", port, WEB_ROOT);

    return server_run(listen_fd) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}