#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>

/* Configuración y límites del servidor */
#define DEFAULT_PORT      8080
#define MAX_EVENTS        1024     
#define MAX_REQUEST       8192     
#define MAX_URI           2048     
#define READ_CHUNK        4096     
#define WEB_ROOT          "www"    

/**
 * Estado de una conexión cliente.
 * Mantiene el contexto del socket para lecturas parciales (I/O no bloqueante)
 * y soporte de conexiones persistentes (keep-alive).
 */
typedef struct {
    int    fd;                    
    char   buf[MAX_REQUEST + 1];  
    size_t buf_len;               
} connection_t;

/* Configura y activa el socket TCP pasivo en modo no bloqueante */
int server_listen(int port);

/* Ciclo principal de eventos impulsado por epoll */
int server_run(int listen_fd);

/* Configura el flag O_NONBLOCK en un descriptor de archivo */
int set_nonblocking(int fd);

#endif /* SERVER_H */