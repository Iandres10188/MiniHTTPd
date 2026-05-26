#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

/* Estados del parser de la solicitud HTTP */
typedef enum {
    PARSE_OK = 0,        /* Válida y completa */
    PARSE_INCOMPLETE,    /* Faltan datos (esperando \r\n\r\n) */
    PARSE_BAD_REQUEST,   /* Malformada -> 400 Bad Request */
    PARSE_NOT_IMPLEMENTED/* Método no soportado (distinto de GET) -> 405 Method Not Allowed */
} parse_result_t;

/* Estructura con los datos esenciales de la solicitud */
typedef struct {
    char method[16];     
    char uri[2048];      /* Ruta sin query string */
    char version[16];    
    int  keep_alive;     /* 1 = mantener conexión activa, 0 = cerrar */
} http_request_t;

/**
 * Analiza el buffer crudo recibido y llena 'req' únicamente si retorna PARSE_OK.
 */
parse_result_t http_parse(const char *buf, size_t len, http_request_t *req);

/**
 * Procesa la solicitud, localiza el recurso y lo envía por 'fd'.
 * Retorna: 1 si se mantiene la conexión (keep-alive), 0 si se debe cerrar.
 */
int http_handle(int fd, const http_request_t *req);

/**
 * Envía una respuesta de error con un cuerpo HTML básico.
 * Retorna: 1 o 0 según el estado de 'keep_alive'.
 */
int http_send_error(int fd, int status, int keep_alive);

#endif /* HTTP_H */