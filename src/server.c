#define _GNU_SOURCE
#include "server.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int server_listen(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    /* Permite reusar el puerto inmediatamente tras reiniciar. */
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, SOMAXCONN) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    if (set_nonblocking(fd) < 0) {
        perror("set_nonblocking");
        close(fd);
        return -1;
    }
    return fd;
}

/* Cierra y libera la conexion asociada a un evento. */
static void close_conn(int epfd, connection_t *c)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    free(c);
}

/* Acepta todas las conexiones pendientes (modo nivel/edge seguro). */
static void accept_connections(int epfd, int listen_fd)
{
    for (;;) {
        struct sockaddr_in cli;
        socklen_t len = sizeof(cli);
        int cfd = accept(listen_fd, (struct sockaddr *)&cli, &len);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; /* ya no hay mas conexiones pendientes */
            if (errno == EINTR)
                continue;
            perror("accept");
            break;
        }
        if (set_nonblocking(cfd) < 0) { close(cfd); continue; }

        connection_t *c = calloc(1, sizeof(connection_t));
        if (!c) { close(cfd); continue; }
        c->fd = cfd;

        struct epoll_event ev;
        ev.events   = EPOLLIN;       /* nivel-disparado por simplicidad */
        ev.data.ptr = c;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev) < 0) {
            perror("epoll_ctl add");
            free(c);
            close(cfd);
        }
    }
}

/* Procesa datos legibles de un cliente. */
static void handle_client(int epfd, connection_t *c)
{
    /* Leemos lo que haya disponible y lo acumulamos en el buffer. */
    for (;;) {
        if (c->buf_len >= MAX_REQUEST) {
            /* Solicitud demasiado grande -> 400 y cerrar. */
            http_send_error(c->fd, 400, 0);
            close_conn(epfd, c);
            return;
        }
        ssize_t n = read(c->fd, c->buf + c->buf_len,
                         MAX_REQUEST - c->buf_len);
        if (n > 0) {
            c->buf_len += (size_t)n;
            continue;
        }
        if (n == 0) {            /* el cliente cerro la conexion */
            close_conn(epfd, c);
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;               /* no hay mas datos por ahora    */
        if (errno == EINTR)
            continue;
        close_conn(epfd, c);     /* error real                    */
        return;
    }

    /* Intentamos parsear lo acumulado. */
    http_request_t req;
    parse_result_t pr = http_parse(c->buf, c->buf_len, &req);

    int keep;
    switch (pr) {
        case PARSE_INCOMPLETE:
            return; /* esperamos mas bytes en el siguiente evento */
        case PARSE_BAD_REQUEST:
            keep = http_send_error(c->fd, 400, 0);
            break;
        case PARSE_NOT_IMPLEMENTED:
            keep = http_send_error(c->fd, 405, req.keep_alive);
            break;
        case PARSE_OK:
        default:
            keep = http_handle(c->fd, &req);
            break;
    }

    if (keep) {
        /* Conexion persistente: reiniciamos el buffer para la siguiente. */
        c->buf_len = 0;
    } else {
        close_conn(epfd, c);
    }
}

int server_run(int listen_fd)
{
    int epfd = epoll_create1(0);
    if (epfd < 0) { perror("epoll_create1"); return -1; }

    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.ptr = NULL;          /* NULL identifica al socket de escucha */
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl listen");
        close(epfd);
        return -1;
    }

    struct epoll_event events[MAX_EVENTS];
    printf("MiniHTTPd escuchando. Bucle de eventos epoll activo.\n");

    for (;;) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < n; i++) {
            if (events[i].data.ptr == NULL) {
                /* Evento en el socket de escucha: nuevas conexiones. */
                accept_connections(epfd, listen_fd);
            } else {
                connection_t *c = (connection_t *)events[i].data.ptr;
                if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    close_conn(epfd, c);
                } else if (events[i].events & EPOLLIN) {
                    handle_client(epfd, c);
                }
            }
        }
    }

    close(epfd);
    return 0;
}
