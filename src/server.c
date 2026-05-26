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

    /* Evita el error 'Address already in use' al reiniciar el servidor rápidamente */
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

static void close_conn(int epfd, connection_t *c)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    free(c);
}

/**
 * Vacía el backlog de conexiones pendientes en el socket pasivo.
 * I/O No bloqueante: itera hasta que accept() retorne EAGAIN/EWOULDBLOCK.
 */
static void accept_connections(int epfd, int listen_fd)
{
    for (;;) {
        struct sockaddr_in cli;
        socklen_t len = sizeof(cli);
        int cfd = accept(listen_fd, (struct sockaddr *)&cli, &len);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; 
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
        ev.events   = EPOLLIN; /* Level-Triggered (LT) */
        ev.data.ptr = c;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev) < 0) {
            perror("epoll_ctl add");
            free(c);
            close(cfd);
        }
    }
}

/**
 * Acumula y procesa las ráfagas de bytes que llegan desde un cliente.
 */
static void handle_client(int epfd, connection_t *c)
{
    for (;;) {
        if (c->buf_len >= MAX_REQUEST) {
            http_send_error(c->fd, 400, 0);
            close_conn(epfd, c);
            return;
        }
        
        ssize_t n = read(c->fd, c->buf + c->buf_len, MAX_REQUEST - c->buf_len);
        if (n > 0) {
            c->buf_len += (size_t)n;
            continue;
        }
        if (n == 0) { /* EOF: El cliente cerró la conexión ordenadamente */
            close_conn(epfd, c);
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break; /* El buffer del sistema operativo se quedó sin bytes por ahora */
        if (errno == EINTR)
            continue;
        close_conn(epfd, c); 
        return;
    }

    http_request_t req;
    parse_result_t pr = http_parse(c->buf, c->buf_len, &req);

    int keep;
    switch (pr) {
        case PARSE_INCOMPLETE:
            return; /* Retorna al bucle epoll a esperar más fragmentos */
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
        c->buf_len = 0; /* Recicla el contexto para la siguiente solicitud HTTP (Keep-Alive) */
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
    ev.data.ptr = NULL; /* Convención interna: NULL mapea al socket de escucha */
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
                /* Evento en el socket pasivo: procesar nuevas conexiones */
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